#include <asm/system.h>
#include <linux/hdreg.h>
#include <linux/fs.h>
#include <asm/io.h>

#define MAJOR_NR 3
#include "blk.h"

#define CMOS_READ(addr) ({ \
outb_p(0x80|addr,0x70); \
inb_p(0x71); \
})

/* 系统支持的最大硬盘数 */
#define MAX_HD 2

struct hd_i_struct
{
	int head, sect, cyl, wpcom, lzone, ctl;
};

/* 如果在include/linux/config.h中定义了符号常数HD_TYPE，就取其中定义好的参数作为硬盘信息数组
 * hd_info[]中的数据，否则先默认设置为0，在setup函数中会另行设置。
 */
#ifdef HD_TYPE
struct hd_i_struct hd_info[] = {HD_TYPE};

/* 计算硬盘个数 */
#define NR_HD ((sizeof (hd_info)) / (sizeof(struct hd_i_struct)))
#else
/* 默认有两个硬盘？ */
struct hd_i_struct hd_info[] = { {0,0,0,0,0,0},{0,0,0,0,0,0} };
static int NR_HD = 0;
#endif

/* 定义硬盘分区结构，给出每个分区从硬盘0道开始算起的物理起始扇区号和分区扇区总数。
 * 其中5的倍数处的项（例如hd[0]和hd[5]）代表整个硬盘的参数。
 */
static struct hd_struct
{
	long start_sect;		/* 分区在硬盘中的起始物理（绝对）扇区号 */
	long nr_sects;			/* 分区中扇区总数 */
}hd[5*MAX_HD] = {{0,0},};

/* 硬盘每个分区的数据块总数数组 */
static int hd_sizes[5*MAX_HD] = {0, };

/* 该函数在system_call.s中定义 */
extern void hd_interrupt(void);
extern void rd_load(void);

/* 下面该函数只在初始化时被调用一次，用静态变量callable作为可调用标志。
 * 参数BIOS是由初始化程序init/main.c中init子程序设置为指向硬盘参数表结构
 * 的指针。
 */
int sys_setup(void * BIOS)
{
	static int callable = 1;
	int i, drive;
	unsigned char cmos_disks;
	struct partition *p;
	struct buffer_head * bh;

	if (!callable)
		return -1;
	callable = 0;

/* 如果没有定义HD_TYPE，则去硬盘参数表中读取硬盘相关数据，硬盘参数属于硬件参数，
 * 由配套的BIOS写死在BIOS所占的内存中的；开机时系统运行在实模式下，只能寻址1MB
 * 内存，BIOS就被映射到1MB内存的高端处。
 */
#ifndef HD_TYPE
	for (drive = 0; drive < 2; drive++)
	{
		hd_info[drive].cyl = *(unsigned short *)BIOS;			/* 柱面数 */
		hd_info[drive].head = *(unsigned char *) (2+BIOS);		/* 磁头数 */
		hd_info[drive].wpcom = *(unsigned short *) (5+BIOS);	/* 写前预补偿柱面号 */
		hd_info[drive].ctl = *(unsigned char *) (8+BIOS);		/* 控制字节 */
		hd_info[drive].lzone = *(unsigned short *) (12+BIOS);	/* 磁头着陆区柱面号 */
		hd_info[drive].sect = *(unsigned char *) (14+BIOS);		/* 每磁道扇区数 */
		BIOS += 16;		/* 每个硬盘参数表长16个字节，BIOS指向下一个硬盘参数表 */
	}

	/* setup.s程序在取BIOS硬盘参数表信息时，如果系统中只有1个硬盘，就会将对应第二个
	 * 硬盘的16字节全部清零。所以只要判断第二个硬盘柱面数是否为0就可以知道是否存在
	 * 第二个硬盘了。
	 */
	if (hd_info[1].cyl)
		NR_HD = 2;
	else
		NR_HD = 1;
#endif

	/* 设置硬盘的分区结构数组 */
	for (i = 0; i < NR_HD; i++)
	{
		hd[i*5].start_sect = 0;
		hd[i*5].nr_sects = hd_info[i].head * hd_info[i].sect * hd_info[i].cyl;
	}

	/* 确定了系统中的硬盘个数，还要确定是否所有硬盘都是AT控制器兼容的。 */
	if (cmos_disks = CMOS_READ(0x12) & 0xf0)
	{
		if (cmos_disks & 0x0f)
			NR_HD = 2;
		else
			NR_HD = 1;
	}
	else
		NR_HD = 0;

	/* 如果NR_HD=0，则两个硬盘都不是AT兼容的，全部清零
	 * 如果NR_HD=1，则第二个硬盘不是AT兼容的，清零硬盘2
	 */
	for (i = NR_HD; i < 2; i++)
	{
		hd[i*5].start_sect = 0;
		hd[i*5].nr_sects = 0;
	}

	for (drive = 0; drive < NR_HD; drive++)
	{
		/* 参数0x300是、0x305分别是两个硬盘的设备号，第2个参数是所需取的块号，
		 * bread函数负责将第1个硬盘上第1个扇区中的分区表信息，读到指定的缓冲
		 * 块中，用来设置分区结构数组hd[]。若操作成功，则数据会被存储在缓冲
		 * 块bh数据区中，若失败，则停机。
		 */
		if (!(bh = bread(0x300 + drive * 5, 0)))
		{
			printk("Unable to read partition table of drive %d\n\r", drive);
			panic("");
		}

		/* 每个硬盘上的分区信息都存储在硬盘上的第一个扇区中，即引导扇区，那么
		 * 就可以根据引导扇区的最后两个字节是否是0xAA55来判断硬盘分区数据是否
		 * 有效，无效则停机。
		 */
		if (bh->b_data[510] != 0x55 || (unsigned char)bh->b_data[511] != 0xAA)
		{
			printk("Bad partition table on drive %d\n\r",drive);
			panic("");
		}

		/* 硬盘分区数据存放在第0x1BE字节处 */
		p = 0x1BE + (void *)bh->b_data;
		for (i = 1; i < 5; p++)
		{
			hd[i + 5 * drive].start_sect = p->start_sect;
			hd[i + 5 * drive].nr_sects = p->nr_sects;
		}

		/* 好了，读完了数据以后释放该缓冲块 */
		brelse(bh);

		/* 接下来对每个分区中的数据块总数进行统计，并保存在硬盘分区数据块总数数组hd_sizes[]中。
		 * 有个问题，怎么确定分区数的就是5的？然后让设备数据块总数指针数组的本设备项指向该数组。
		 * 注意“hd[i].nr_sects>>1，是因为一块1024字节，等于两个扇区。
		 */
		for (i = 0; i < 5 * MAX_HD; i++)
			hd_sizes[i] = hd[i].nr_sects>>1;
		blk_size[MAJOR_NR] = hd_sizes;
	}

	/* 如果确实有硬盘存在并且已经读入其分区表，则显示分区表正常的信息。 */
	if (NR_HD)
		printk("Partition table%s ok.\n\r",(NR_HD>1)?"s":"");

	/* 然后尝试在系统内存虚拟盘中加载启动盘中包含的根文件系统映像 */
	rd_load();

	/* 初始化交换设备,又是一个找不到函数声明的函数，怪了 */
	init_swapping();

	/* 挂载根文件系统 */
	mount_root();
}

/* 这个函数没有找到函数原型在哪里声明，很奇怪 */
void do_hd_request(void)
{
	int i, r;
	unsigned int block, dev;
	unsigned int sec, head, cyl;
	unsigned int nsect;

	INIT_REQUEST;
}

/* 硬盘系统初始化
 * 设置硬盘中断描述符，并允许硬盘控制器发送中断请求信号
 */
void hd_init(void)
{
	/* 变量blk_dev是在 kernel\blk_drv\ll_rw_blk.c  中定义的，在blk.h中有使用extern关键字声明的语句 */
	blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;
	set_intr_gate(0x2E, &hd_interrupt);		// 设置中断门中的处理函数指针，中断号为0x2E
	outb_p(inb_p(0x21)&0xfb, 0x21);			// 复位主片上接联引脚屏蔽位（位2）
	outb(inb_p(0xA1)&0xbf, 0xA1);			// 复位从片上硬盘中断请求屏蔽位（位6）
}