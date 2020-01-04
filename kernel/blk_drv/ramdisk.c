#include <linux/sched.h>
#include <linux/fs.h>

#define MAJOR_NR 1
#include "blk.h"

char *rd_start;
int rd_length = 0;

void do_rd_request(void)
{
}

/* 返回内存虚拟盘ramdisk所需的内存量，该函数只在Makefile文件中设置了虚拟盘大小的前提下才会被调用。
 * 虚拟磁盘实际上是从主存中挖出来一块。
 */
long rd_init(long mem_start, int length)
{
	int i;
	char *cp;

	blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;		// 注册请求函数
	rd_start 	= (char *)mem_start;					// 将主内存起始位置赋值给全局变量rd_start
	rd_length 	= length;								// 将虚拟磁盘长度（字节）赋值给变量rd_length
	cp = rd_start;

	/* 对虚拟磁盘清零 */
	for (i = 0; i < length; i++)
		*cp++ = '\0';

	/* 返回虚拟磁盘的内存大小，单位是字节 */
	return (length);
}

/* 如果根文件系统设备（root device）是虚拟磁盘的话（ramdisk），则尝试加载它。root device原本是
 * 指向软盘的，我们将它改成指向ramdisk。
 */
 void rd_load(void)
{
	struct buffer_head *bh;
	struct super_block s;
	int block = 256;
	int i = 1;
	int nblocks;
	char *cp;

	/* 如果rd_length为0，即不存在虚拟磁盘，则返回 */
	if (!rd_length)
		return;

	/* 不为0则说明虚拟磁盘存在 */
	printk("Ram disk: %d bytes, starting at 0x%x\n", rd_length, (int) rd_start);

	/* 如果此时根文件设备不是软盘设备（这里指的是磁盘，具体：1 - 内存，2 - 磁盘，3 - 硬盘，
	 * 4 - ttyx，5 - tty，6 - 并行口，7 - 非命名管道），也退出。
	 */
	if (MAJOR(ROOT_DEV) != 2)
		return;

	/* 读软盘块256+1、256和256+2，其中block+1是超级块，超级块用来描述存放在盘设备上文件系统的
	 * 结构信息，并说明各部分的大小。
	 */
	bh = breada(ROOT_DEV, block+1, block, block+2, -1);

	if (!bh)
	{
		printk("Disk error while looking for ramdisk!\n");
		return;
	}

	/* 将上面读出来的超级块数据截断成盘设备上的形式，即只取前8项 */
	*((struct d_super_block *) &s) = *((struct d_super_block *)bh->b_data);

	/* 释放该缓冲块 */
	brelse(bh);

	/* 如果魔数不等于MINIX文件系统的魔数，则说明磁盘中没有虚拟盘ramdisk映像文件，退出去执行
	 * 通常的软盘引导。
	 */
	if (s.s_magic != SUPER_MAGIC)
		return;

	/* nblock为文件系统中的数据块总数，对于MINIX 1.0文件系统，s_log_zone_size为0，即逻辑块大小等于数据块大小 */
	nblocks = s.s_nzones << s.s_log_zone_size;

	/* 如果文件系统所需的数据块总数大于内存中分配的虚拟磁盘大小，则保存返回，这里做了一个将字节数转换为数据块数的转换 */
	if (nblocks > (rd_length) >> BLOCK_SIZE_BITS)
	{
		printk("Ram disk image too big!  (%d blocks, %d avail)\n", nblocks, rd_length >> BLOCK_SIZE_BITS);
		return;
	}

	/* 打印加载信息,单位为字节数 */
	printk("Loading %d bytes into ram disk... 0000k", nblocks << BLOCK_SIZE_BITS);

	cp = rd_start;

	while (nblocks)
	{
		/* 如果一次加载的数据块数大于2，就是用预读取函数，一次读三个块？那下一次读取如果block++，不就会重复读了么？ */
		if (nblocks > 2)
			bh = breada(ROOT_DEV, block, block+1, block+2, -1);
		else
			bh = bread(ROOT_DEV, block);

		if (!bh)
		{
			printk("I/O error on block %d, aborting load\n", block);
			return;
		}

		/* 好熟悉的函数啊，memcpy只是用作在内存中拷贝数据，这里将数据从高速缓存中将数据拷贝到虚拟磁盘上。
		 * 注意这里memcpy有两个定义，一个是宏一个是内联函数，查过资料显示宏定义的返回值不能被强制转换，
		 * 所以这里用void做了强制转换说明这里用的不是宏定义，而是内联函数的版本。
		 */
		(void)memcpy(cp, bh->b_data, BLOCK_SIZE);
		brelse(bh);

		/* \010表示制表符 */
		printk("\010\010\010\010\010%4dk",i);
		cp += BLOCK_SIZE;
		block++;
		nblocks--;
		i++;
	}
	printk("\010\010\010\010\010done \n");

	/* 更新根文件系统设备号，0x0101为虚拟磁盘的设备号 */
	ROOT_DEV = 0x0101;
}