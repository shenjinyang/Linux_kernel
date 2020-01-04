#include <mm.h>
#include <linux/sched.h>
#include <linux/head.h>

/* 我们只分配1页内存（4KB）来作为交换位图，4KB = 4096 * 8 = 32768 bit，每个bit表征1交换页
 * 是否有效，即最多支持32768个交换页。 
 */
#define SWAP_BITS (4096<<3)

static char * swap_bitmap = NULL;

/* 交换设备号，初始值为0，在main.c中被初始化 */
int SWAP_DEV = 0;

/* 我们从不交换任务0（task[0]）的页面，因为它是内核页面，必须一直在内存中；
 * 第一个虚拟内存页面，即从任务0末端（64MB）处开始的虚拟内存页面。
 */
#define FIRST_VM_PAGE (TASK_SIZE>>12)				// =64MB/4KB 	= 第16384个虚拟页
#define LAST_VM_PAGE (1024*1024)					// =4GB/4KB		= 1048576
#define VM_PAGES (LAST_VM_PAGE - FIRST_VM_PAGE)		// =1032192(从0开始计数)

/* 申请取得一个交换页面号 */
static int get_swap_page(void)
{
	int nr;

	if (!swap_bitmap)
		return 0;
	for (nr = 0; nr < 32768; nr++)
		if (clrbit(swap_bitmap, nr))
			return nr;
	return 0;
}

/* 释放交换设备中指定的交换页面 */
void swap_free(int swap_nr)
{
}

/* 该函数负责将swap_out函数中挑选出来的有效页面换出主存，其参数为被选中页面在页表中对应的表项 */
int try_to_swap_out(unsigned long * table_ptr)
{
	unsigned long page;
	unsigned long swap_nr;

	page = *table_ptr;								// 取表项中存储的页面物理地址
	if (!(PAGE_PRESENT & page))
		return 0;
	if (page - LOW_MEM > PAGING_MEMORY)
		return 0;
	if (PAGE_DIRTY & page)							// 如果页面已脏，即已经被修改
	{
		page &= 0xfffff000;
		if (mem_map[MAP_NR(page)] != 1)				// 如果当前页面没有使用，说明没有被换出去的意义
			return 0;
		if (!(swap_nr = get_swap_page()))
			return 0;
	}
}

/* 将页面换到交换设备中，交换设备应该就是在磁盘上分出来的一块内存空间
 * 这个函数什么情况下需要被调用呢？就是在分配物理页面时，没有可用的页面了，
 * 这时候就需要找到一个有效的（注意是有效的，即最低位为1）页面，将其换出去，
 * 换出去就是指将这一整页物理页面的内容复制到磁盘上，然后将页表的最低位置0，
 * 
 */
int swap_out(void)
{
	static int dir_entry = FIRST_VM_PAGE>>10;		// 任务1的第一个页目录项索引，值为16，注意是虚拟页目录
	static int page_entry = -1;						// 定位到某个页表的表项，最小值0，最大值1024
	int counter = VM_PAGES;
	int pg_table;

	/* 既然要将有效的虚拟页交换出去，那么首先就要先找到有效的页目录表项 */
	while (counter > 0)
	{
		pg_table = pg_dir[dir_entry];				// 页目录表项中的内容，为20位页表地址+12位属性位
		if (pg_table & 1)							// 如页目录表项的最低位为1，则表示该页目录表项有效
			break;
		counter -= 1024;							// 一个页表对应1024个页帧，counter是页数目
		dir_entry++;								// 转移到下一个页目录表项
		if (dir_entry >= 1024)						// 如果页目录表项计数值大于1024，说明溢出了
			dir_entry = FIRST_VM_PAGE>>10;			// 将页目录表项的计数从新指向任务1的第一个页目录表项处
	}

	/* 找到并取得了当前页目录表项中的页表指针后，针对该页表中的1024个页面，逐一调用交换函数try_to_swap_out
	 * 尝试把它交换出去，一旦摸个页面交换成功，就返回1，否则显示交换内存使用完的警告，并返回0。交换内存
	 * 为什么会使用完呢，因为在磁盘上分配的交换空间是有限的。
	 */
	pg_table &= 0xfffff000;							// 过略掉低12位属性位，得到页表指针（页表地址）
	while (counter-- > 0)
	{
		page_entry++;

		/* 如果整个页表中的1024项都已经试过且不能交换，那么就需要重复上面的代码来选出下一个存在的页表 */
		if (page_entry >= 1024)
		{
			page_entry = 0;
		repeat:
			dir_entry++;
			if (dir_entry >= 1024)
				dir_entry = FIRST_VM_PAGE>>10;
			pg_table = pg_dir[dir_entry];
			if (!(pg_table&1))
			{
				if ((counter -= 1024) > 0)
					goto repeat;
				else
					break;
			}
			pg_table &= 0xfffff000;
		}
		if (try_to_swap_out(page_entry + (unsigned long *) pg_table))
			return 1;
	}
}

/* 在主内存中申请一个空闲的物理页面，如果没有可用的物理内存页面，则调用执行交换处理，
 * 然后再次申请。
 * ------------------------------------------------------------
 * 输出寄存器：%0：ax = 物理页面起始地址(0)
 * ------------------------------------------------------------
 * 输入寄存器：%1：ax = 0
 *             %2：LOW_MEM，1MB内存大小
 *             %3：cx = PAGING_PAGES，主存页面总数（3480）
 *             %4：edi = mem_map + PAGING_PAGES - 1，mem_map是内存字节位图数组，共PAGING_PAGES项
 */
unsigned long get_free_page(void)
{
/* 定义一个局部寄存器变量__res，该变量保存在寄存器eax中，以便高效访问和操作 */
register unsigned long __res asm("ax");
repeat:
__asm__("std; repne; scasb\n\t"			/* std置方向位，使得edi从高地址向低地址偏移，这里注意：edi寄存器默认与 */
										/* ES联合使用，而此时我们在任务0的上下文中，所以ES中存储的是任务0的数据段 */
										/* 段选择子0x17，其基地址为0，段限长为640KB，那么此时我只能假设，数组mem_map */
										/* 编译后所在的地址位于640KB以内 */
										/* 逐一比较al(0)与每个页面的di中的内容； */
		"jne 1f\n\t"					/* 如果没有等于0的字节，则跳转结束返回0； */
		"movb $1, 1(%%edi)\n\t"			/* 1 =>[1+edi]，将对应页面内存映像数组元素置1； */
		"sall $12, %%ecx\n\t"			/* ecx左移12位，即 页面数*4K = 相对于主存页面起始处的偏移量； */
		"addl %2, %%ecx\n\t"			/* 再加上低端1MB内存地址，得出页面实际物理起始地址； */
		"movl %%ecx, %%edx"				/* 将实际页面起始物理地址赋值给edx寄存器； */
		"movl $1024, %%ecx\n\t"			/* 设置寄存器ecx计数值为1024； */
		"leal 4092(%%edx), %%edi\n\t"	/* 将4092+edx的位置赋值给edi（该页面的顶端）； */
		"rep; stosl\n\t"				/* STOSL指令相当于将EAX中的值保存到ES:EDI指向的地址中（反方向，即将该页面清0）； */
		"movl %%edx, %%eax\n"			/* 将页面起始位置赋值给eax（返回值）； */
		"1:"
		:"a"(__res)
		:"0"(0), "i"(LOW_MEM), "c"(PAGING_PAGES),
		"D"(mem_map + PAGING_PAGES - 1)
		:"di", "cx", "dx");				// 这里第三个冒号后面的字符串是告诉gcc寄存器的使用情况，以免出现优化错误。
		if (__res >= HIGH_MEMORY)		// 如果返回的物理地址大于本内核支持的最大值，则返回顶端重新查找。
			goto  repeat;
		if (!__res && swap_out())		// 如果没有得到空闲页面则执行交换处理，并重新查找。任务0创建任务1swap_out肯定返回0
			goto  repeat;				// 这里的swap_out函数只有在本函数中使用，定义在本函数之前，所以没有看到有函数声明
		return __res;					// 返回找到的物理页面号。
}

/* 内存页面交换初始化，没找到函数声明 */
void init_swapping(void)
{
	/* blk_size是指向指定主设备号的块设备的块数数组（二维数组，数组成员是指针，每个成员指针指向一类块设备的各个子设备所含数据块总数的数组），
	 * 在blk_drv/ll_rw_blk.c中定义，本函数使用extern关键字引用。
	 */
	extern int * blk_size[];
	int swap_size, i, j;

	/* 如果SWAP_DEV为0则表示不存在交换设备号，也就不存在交换设备，返回 */
	if (!SWAP_DEV)
		return;

	/* 如果blk_size中指向交换设备中各个子设备数据块总数的数组为NULL，则报错返回，没有找到blk_size数组中交换设备项初始化的地方，
	 * 如果非要解释的话，那就是该版本内核虽然支持了交换设备，但是并没有提供真正的交换设备，即在main.c中交换设备号同样初始化为0。
	 */
	if (!blk_size[MAJOR(SWAP_DEV)])
	{
		printk("Unable to get size of swap device\n\r");
		return;
	}

	/* 取交换设备中的总数据块数 */
	swap_size = blk_size[MAJOR(SWAP_DEV)][MINOR(SWAP_DEV)];

	if (!swap_size)
		return;

	if (swap_size < 100)
	{
		printk("Swap device too small (%d blocks)\n\r",swap_size);
		return;
	}

	/* 将交换设备数据块数换算成页面数，1块为1KB，1页为4KB，右移2表示除以4 */
	swap_size >>= 2;

	/* 如果交换设备支持的页面数大于32768，则设置其等于32768 */
	if (swap_size > SWAP_BITS)
		swap_size = SWAP_BITS;

	/* 在内存中申请一页物理页面作为交换页面管理页面 */
	swap_bitmap = (char *)get_free_page();

	if (!swap_bitmap)
	{
		printk("Unable to start swapping: out of memory :-)\n\r");
		return;
	}

	/* 交换区的管理页面是交换设备上的页面0 */
	read_swap_page(0, swap_bitmap);

	/* 交换设备页面0（即管理页面）的最后10个字节存放着字符"SWAP-SPACE"，用来标识交换设备的有效性 */
	if (strncmp("SWAP-SPACE", swap_bitmap + 4086, 10))
	{
		printk("Unable to find swap-space signature\n\r");
		free_page((long) swap_bitmap);
		swap_bitmap = NULL;
		return;
	}

	memset(swap_bitmap + 4086, 0, 10);

	/* 这个for循环跳过了位图的1 ~ （swap_size-1)的比特位，只检查位0和位swap_size ~ SWAP_BITS，
	 * 他们应该都为0表示不可用，
	 */
	for (i = 0; i < SWAP_BITS; i++)
	{
		if (i == 1)
			i = swap_size;
		
		if (bit(swap_bitmap,i))
		{
			printk("Bad swap-space bit-map\n\r");
			free_page((long) swap_bitmap);
			swap_bitmap = NULL;
			return;
		}
	}

	/* 这个for循环检查位1 ~ 位 */
	j = 0;
	for (i = 1 ; i < swap_size ; i++)
		if (bit(swap_bitmap,i))
			j++;
	if (!j) {
		free_page((long) swap_bitmap);
		swap_bitmap = NULL;
		return;
	}
	printk("Swap device ok: %d pages (%d bytes) swap-space\n\r",j,j*4096);
}
