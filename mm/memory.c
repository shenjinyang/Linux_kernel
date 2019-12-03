/* 这几个变量都是与head.s中的变量对应的，如要修改需先修改head.s中相关文件 */
#define LOW_MEM 0x100000						// 1MB内存，1<<20
#define PAGING_MEMORY (15*1024*1024)			// 除去内核占据的1MB内存，还剩下15MB内存
#define PAGING_PAGES (PAGING_MEMORY>>12)		// 每页4KB，15MB/4KB = 3840（总的页面数）
#define MAP_NR(addr) (((addr)-LOW_MEM)>>12)		// 当前地址（主内存开始处） - 1MB（内核占用）>>12 = 总的要管理的主内存页面数
#define USED 100

static long HIGH_MEMORY = 0;

static unsigned char mem_map[PAGING_PAGES] = {0, };		// 定义一个PAGING_PAGES大小的整型数组并初始化为0，用来记录每一页内存的使用情况

void mem_init(long start_mem, long end_mem)
{
	int i;

	HIGH_MEMORY = end_mem;
	for (i = 0; i < PAGING_PAGES; i++)
		mem_map[i] = USED;
	i = MAP_NR(start_mem)			// 主内存所在的页面号
	end_mem -= start_mem;
	end_mem >>= 12;					// 算出主内存所占的页面数
	while (end_mem-- > 0)
		mem_map[i++] = 0;			// 将其全部清零，表示初始时所有页面皆未使用
}
