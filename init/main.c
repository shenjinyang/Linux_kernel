/*
 *  linux/init/main.c
 *
 *  (C) 2019  Jinyang Shen
 */

#define __LIBRARY__
#include <unistd.h>
#include <time.h>

/*
 * we need this inline - forking from kernel space will result
 * in NO COPY ON WRITE (!!!), until an execve is executed. This
 * is no problem, but for the stack. This is handled by not letting
 * main() use the stack at all after fork(). Thus, no function
 * calls - which means inline code for fork too, as otherwise we
 * would use the stack upon exit from 'fork()'.
 *
 * Actually only pause and fork are needed inline, so that there
 * won't be any messing with the stack from main(), but we define
 * some others too.
 */

static inline _syscall0(int,fork)
static inline _syscall0(int,pause)
static inline _syscall1(int,setup,void *,BIOS)
static inline _syscall0(int,sync)

#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/head.h>
#include <asm/system.h>
#include <asm/io.h>

#include <stddef.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

#include <linux/fs.h>

#include <string.h>

static char printbuf[1024];				//静态字符数组，用作内核显示信息的缓存。

extern int mem_init(long start, long end);
extern void blk_dev_init(void);
extern void chr_dev_init(void);
extern void tty_init(void);
extern long kernel_mktime(struct tm * tm);
extern long startup_time;


/* 下面这些数据是在内和引导期间由setup.s程序设置的，
 * 分别将指定的线性地址强行转换为给定数据类型的指针，并获取指针所指的内容，由于内核代码
 * 被映射到从物理地址0开始的位置，所以一这些线性地址正好也是对应的物理地址。
 */
#define EXT_MEM_K (*(unsigned short *)0x90002)				// 1MB以后的扩展内存大小（KB），1MB之前为内核所用。
#define CON_ROWS ((*(unsigned short *)0x9000e) & 0xff)		// 选定的控制台屏幕行、列数。
#define CON_COLS (((*(unsigned short *)0x9000e) & 0xff00) >> 8)
#define DRIVE_INFO (*(struct drive_info *)0x90080)			// 硬盘参数表32字节内容。
#define ORIG_ROOT_DEV (*(unsigned short *)0x901FC)			// 根文件系统所在设备号。
#define ORIG_SWAP_DEV (*(unsigned short *)0x901FA)			// 交换文件所在设备号。

/* 读取CMOS实时时钟信息，outb_p和inb_p是include/asm/io.h中定义的端口输入输出宏。
 * 0x70是写地址端口号，0x71是读数据端口号。0x80|addr是要读取的CMOS内存地址。
 */
#define CMOS_READ(addr) ({ \ 
outb_p(0x80|addr,0x70); \
inb_p(0x71); \
})

/* 将BCD码转换为十进制数，这里只转换了个位和十位，那么表示天数的百位呢？ */
#define BCD_TO_BIN(val) ((val) = ((val)&15) + ((val)>>4)*10)

static void time_init(void)
{
	struct tm time;

	do {
		time.tm_sec 	= CMOS_READ(0);
		time.tm_min 	= CMOS_READ(2);
		time.tm_hour	= CMOS_READ(4);
		time.tm_mday	= CMOS_READ(7);
		time.tm_mon		= CMOS_READ(8);
		time.tm_year	= CMOS_READ(9);
	}while (time.tm_sec != CMOS_READ(0));
	BCD_TO_BIN(time.tm_sec);
	BCD_TO_BIN(time.tm_min);
	BCD_TO_BIN(time.tm_hour);
	BCD_TO_BIN(time.tm_mday);
	BCD_TO_BIN(time.tm_mon);
	BCD_TO_BIN(time.tm_year);
	time.tm_mon--;
	startup_time = kernel_mktime(&time);
}

static long memory_end 			= 0;  		// 机器具有的物理内存容量（字节数）。
static long buffer_memory_end 	= 0;		// 高速缓存最后一个字节处。
static long main_memory_start	= 0;

struct drive_info {char dummy[32]; } drive_info;

void main(void)
{
	/* 此时中断仍然被禁止着，做完必要的设置之后就将其开启 */
	ROOT_DEV 	= ORIG_ROOT_DEV;
	drive_info 	= DRIVE_INFO;
	memory_end	= (1<<20) + (EXT_MEM_K<<10);		// 统计总内存大小（字节），1MB + 剩余。
	memory_end &= 0xfffff000;						// 忽略不到4KB的内存数。
	if (memory_end > 16*1024*1024)					// 如果大于16MB
		memory_end = 16*1024*1024；
	if (memory_end > 12*1024*1024)
		buffer_memory_end = 4*1024*1024;
	else if (6*1024*1024)
		buffer_memory_end = 2*1024*1024;
	else
		buffer_memory_end = 1*1024*1024;
#ifdef RAMDISK										// 如果设置了虚拟内存盘，那么还要从主内存中分配出去一块作为虚拟内存盘。
	main_memory_start += rd_init(main_memory_start, RAMDISK*1024);
#endif
	mem_init(main_memory_start, memory_end);
	trap_init();
	blk_dev_init();
	chr_dev_init();
	tty_init();
	time_init();
	sched_init();
}
