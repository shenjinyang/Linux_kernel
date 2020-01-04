#include <linux/sched.h>
#include <linux/kernel.h>

unsigned long HIGH_MEMORY = 0;

unsigned char mem_map[PAGING_PAGES] = {0, };		// 定义一个PAGING_PAGES大小的整型数组并初始化为0，用来记录每一页内存的使用情况

/* 释放物理地址addr开始的1页内存 */
void free_page(unsigned long addr)
{
	if (addr < LOW_MEM)
		return;
	if (addr >= HIGH_MEMORY)
		panic("trying to free nonexistent page");

	/* 计算该页在mem_map中的数组号 */
	addr -= LOW_MEM;
	addr >>= 12;

	/* 如果mem_map[addr]不为0，则减1返回 */
	if (mem_map[addr]--)
		return;

	/* 否则，mem_map[addr]原本就是0，说明内核在尝试释放一个本来就没有使用的页面，报错。
	 * 这里有一点值得注意，那就是：释放物理页面，只是将管理页面的数组的对应项减1，如果
	 * 这个页面原本只有本进程使用，那么其值应该为1，减1后为0，表明该页面可以被使用；如果
	 * 有多余1个进程使用该页面，那么该项减1后仍不为0，即不可使用状态；释放页面并不是将
	 * 该页面中的内容清0，只是将其状态设置为可被覆盖。
	 */
	mem_map[addr] = 0;
	panic("trying to free free page");
}

/* 根据指定的线性地址和限长（页表个数），释放内存块并置表项空闲
 * 页目录表位于物理地址0处，每项4字节，共1024项，共占4K字节；每个目录指定一个页表。内核一共使用了4个
 * 页目录表项，内核页表从物理地址0x1000处开始（紧接着页目录表），每个页表有1024项，每项4字节。除了内核
 * 代码中的进程0和1以外，其它各进程的页表所占的页面在进程被创建时有内核为其在主内存去申请得到（应该就是
 * 通过调用get_free_page函数）。
 * 参数：from - 起始线性地址
 *       size - 释放的字节长度
 */
int free_page_tables(unsigned long from, unsigned long size)
{
	unsigned long *pg_table;
	unsigned long * dir, nr;

	if (from & 0x3fffff)
		panic("free_page_tables called with wrong alignment");
	if (!from)
		panic("Trying to free up swapper memory space");
	size = (size + 0x3fffff)>>22;
	dir = (unsigned long *) ((from>>20) & 0xffc);
	for (; size-- > 0; dir++)
	{
		if (!(1 & *dir))
			continue;
		pg_table = (unsigned long *) (0xfffff000 & dir);
		for (nr = 0; nr < 1024; nr++)
		{
			if (*pg_table)
			{
				if (1 & *pg_table)
					free_page(0xfffff000 & *pg_table);
				else
					swap_free(*pg_table >> 1);

				/* 清空页表项 */
				*pg_table = 0;
			}
			pg_table++;
		}
		/* 清空页目录项 */
		free_page(0xfffff000 & *dir);
		*dir = 0
	}
	invalidate();
	return 0;
}

/* 复制页目录表项和页表项
 * 复制指定线性地址和长度内存对应的页目录表项和页表项，从而被复制的页目录和页表对应的原物理内存页面区被
 * 两套页表映射而共享使用，此时需要将共享物理页面设置为只读，当需要向其中写数据时，会触发硬件相应从而分
 * 配新的内存，即写时复制技术。
 * 参数from和to是线性地址，size是需要复制（共享）的内存长度，单位是字节。
 */
int copy_page_tables(unsigned long from, unsigned long to, long size)
{
	unsigned long * from_page_table;
	unsigned long * to_page_table;
	unsigned long this_page;
	unsigned long * from_dir, * to_dir;
	unsigned long new_page;
	unsigned long nr;

	/* 验证源线性地址和目的线性地址是4MB对齐的，只有这样才能保证复制的是一个完整的页表 */
	if ((from & 0x3fffff) || (to & 0x3fffff))
		panic("copy_page_tables called with wrong alignment");

	/* 源线性基地址对应的起始页目录项，想想线性地址的构成，高10位用来索引页目录表，共可索引1024项，
	 * 每项页目录表项4字节，所以实际目录项指针 = 目录项号<<2，也就是from>>20；“与”上0xffc是为了确保
	 * 目录项指针范围有效。
	 * Linux0.12内核所有进程共享同一个页目录表项么？我记着之前看的是每个进程都有自己的页目录表和页表呢。
	 */
	from_dir = (unsigned long *) ((from>>20) & 0xffc);
	to_dir = (unsigned long *) ((to>>20) & 0xffc);

	/* 计算参数size给出的长度所占的页目录项数（4MB的进位整数倍），即页表数；加上0x3fffff表示
	 * 除操作如有余数则进1。
	 */
	size = ((unsigned) (size+0x3fffff)) >> 22;

	for (; size-- > 0; from_dir++, to_dir++)
	{
		/* 页目录表项最低位P是存在标志位，如果目的页目录表项的P为1，则表示不能被覆盖，死机 */
		if (1 & *to_dir)
			panic("copy_page_tables: already exist");

		/* 如果源页目录表项不存在，那么就没办法复制这一项页目录，复制下一项 */
		if (!(1 & from_dir))
			continue;

		/* 取源目录表项中页表地址，页目录表项中的第12-21位为页表物理地址 */
		from_page_table = (unsigned long *) (0xfffff000 & *from_dir);

		/* 分配一页物理内存用于存放新任务的页表,如失败返回-1 */
		if (!(to_page_table = (unsigned long *) get_free_page()))
			return -1;

		/* 将页目录表项的低3位置1，表示对应页表映射的内存页面（即存放新任务页表的物理内存页面）
		 * 是用户级的，可读写，且存在。
		 */
		*to_dir = ((unsigned long) to_page_table) | 7;

		/* 设置针对当前处理的页目录表项对应的页表，需要复制的页面数；如果是在内核空间，则
		 * 仅需要复制头160页对应的页表项，对应于开始640KB物理内存（为啥？），否则需要复制一个页表中
		 * 的所有1024个页表项，可映射4MB物理内存。
		 */
		nr = (from == 0) ? 0xA0:1024;

		for (; nr-- > 0; from_page_table++, to_page_table++)
		{
			this_page = *from_page_table;

			/* 如果当前页面没有使用，即项内容为0，则不用复制该项，继续处理下一项 */
			if (!this_page)
				continue;

			/* 如果当前页面有内容，但是其存在位P为0，则表明该表项对应的页面可能在交换设备中，
			 * 于是先申请一页内存，并从交换设备中读入该页面（若有交换设备的话）。然后将该页表项
			 * 复制到目的页表项中。并修改源页表项内容指向该新申请的内存页（这里有一点不解，将
			 * 源页表表项指向新申请的页面，那么该页表项原来指向的那页物理内存咋整？就这么丢了？
			 * 不是丢了，因为页面被选作牺牲页面交换到交换空间中，而从这个物理页面就被映射到一个
			 * 新的页表条目中了，所以我们新申请一个物理页面用来存储交换空间中的页面，只需要重新
			 * 将这个也表条目中的页面地址指向新申请的物理页面即可，与原来的物理页面没有任何关系。），
			 * 并设置页表项标志为“页面脏”，并或上7设置页面为存在，用户级，只读。
			 */
			if (!(1 & this_page))
			{
				if (!(new_page = get_free_page()))
					return -1;
				/* 这个函数等后面遇到再来 */
				read_swap_page(this_page>>1, (char *) new_page);
				*to_page_table = this_page;
				*from_page_table = new_page | (PAGE_DIRTY | 7);
				continue;
			}

			/* 设置该页面为只读，因为此时两个进程共享该物理内存 */
			this_page &= ~2;
			*to_page_table = this_page;

			/* 对于进程0创建进程1，这个if语句不会执行 */
			if (this_page > LOW_MEM)
			{
				/* 这里也要设置源页面为只读，同样是因为两个进程共享一个物理内存区域 */
				*from_page_table = this_page;

				/* 计算页面号。需要先减去低端1MB被内核占用的物理内存 */
				this_page -= LOW_MEM;
				this_page >>= 12;
				mem_map[this_page]++;
			}
		}
	}
	invalidate();
	return 0;
}

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
