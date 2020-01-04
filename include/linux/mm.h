#ifndef _MM_H
#define _MM_H

/* 定义每个页面的大小为4096字节 */
#define PAGE_SIZE 4096

extern int SWAP_DEV;

#define read_swap_page(nr,buffer) ll_rw_page(READ, SWAP_DEV, (nr), (buffer));

extern unsigned long get_free_page(void);
extern void free_page(unsigned long addr);
void swap_free(int page_nr);

/* 刷新页变换高速缓冲（TLB）宏函数
 * 使用重新加载页目录表基地址寄存器cr3的方法来进行刷新。
 */
#define invalidate() \
__asm__("movl %%eax, %%cr3"::"a" (0))

/* 这几个变量都是与head.s中的变量对应的，如要修改需先修改head.s中相关文件 */
#define LOW_MEM 0x100000						// 1MB内存，1<<20
extern unsigned long HIGH_MEMORY;
#define PAGING_MEMORY (15*1024*1024)			// 除去内核占据的1MB内存，还剩下15MB内存
#define PAGING_PAGES (PAGING_MEMORY>>12)		// 每页4KB，15MB/4KB = 3840（总的页面数）
#define MAP_NR(addr) (((addr)-LOW_MEM)>>12)		// 当前地址（主内存开始处） - 1MB（内核占用）>>12 = 总的要管理的主内存页面数
#define USED 100

extern unsigned char mem_map[PAGING_PAGES];

/* 页目录表项和页表（二级页表）中的一些标志位 */
#define PAGE_DIRTY 0x40							// 位6，0x40为二进制0x1000000，页面脏（已修改）
#define PAGE_PRESENT 0x01						// 位0，页面存在标志位，0-不存在；1-存在

#endif