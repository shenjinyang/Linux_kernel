#include <stdarg.h>
#include <linux/sched.h>
#include <asm/system.h>
#include <linux/kernel.h>

/* 这个end是在外部文件定义的，实际上是在编译过程中由链接器指定的，
 * 其值代表内核模块末端，同时也是高速缓冲区的起始位置。
 */
extern int end;

/* 定义系统缓冲区中含有的缓冲块数 */
int NR_BUFFERS = 0;

struct buffer_head * start_buffer = (struct buffer_head *) &end;

/* 定义hash列表指针数组，数组成员为指针 */
struct buffer_head * hash_table[NR_HASH];

/* 用来指向第一个缓冲块头部 */
static struct buffer_head * free_list;

/* 等待空闲缓冲块而睡眠的任务队列 */
static struct task_struct * buffer_wait = NULL;

/* 等待指定缓冲块解锁。
 * 如果指定的缓冲块bh已经上锁，那么我们就让进程不可中断地睡眠在该缓冲块的等待队列
 * b_wait中。在缓冲块解锁时，其等待队列上的所有进程将被唤醒。
 */
static inline void wait_on_buffer(struct buffer_head * bh)
{
	cli();
	while (bh->b_lock)
		sleep_on(&bh->b_wait);
	sti();
}

/* 对指定设备执行高速缓冲区数据与设备上数据的同步操作 */
int sync_dev(int dev)
{
	int i;
	struct buffer_head * bh;

	bh = start_buffer;

	for (i = 0; i < NR_BUFFERS; i++, bh++)
	{
		/* 从第一个缓冲块开始扫描所有的缓冲块头部数据结构，找到其
		 * 设备号与函数入参相等的缓冲块头部。
		 */
		if (bh->b_dev != dev)
			continue;

		/* 若已经上锁就让当前进程睡眠等待其解锁，上锁并不意味着被修改 */
		wait_on_buffer(bh);

		/* 再次判断该缓冲块是否还是指定设备的缓冲块，并且已经被修改 */
		if (bh->b_dev == dev && bh->b_dirt)
			/* 就将其写回到指定设备的内存中（磁盘上的内存？） */
			ll_rw_block(WRITE, bh);
	}

	/* 下面的函数用来将内存中的i节点同步到盘设备中去，我们将高速缓存中的数据
	 * 同步到设备中去，那么就相当于将盘上的某个文件修改了，那么对于修改盘上
	 * 文件这件事，就需要上报给文件系统，即该文件所在的块对应的i节点数据结构
	 * 需要修改，所以此时需要将内存中的i节点同步到磁盘上去。
	 */
	sync_inodes();

	/* 这里要再一次执行缓冲区数据同步到设备中，是因为同步内存i节点数据到高速缓冲区
	 * 改变了i节点中的数据，同样需要将高速缓冲中的i节点数据同步到设备中去，所以需
	 * 再一次执行同步操作。
	 */
	for (i = 0; i < NR_BUFFERS; i++, bh++)
	{
		if (bh->b_dev == dev && bh->b_dirt)
			ll_rw_block(WRITE, bh);
	}

	return 0;
}

/* 下面两行是hash（散列）函数定义和hash表项的计算宏。
 * hash表的主要作用是减少查找比较元素所花费的时间。通过在元素的存储位置
 * 与关键字之间建立一个对应关系（hash函数），我们就可以直接通过函数计算立刻
 * 查找出指定的元素。
 */
#define _hashfn(dev, block) (((unsigned)(dev ^ block)) % NR_HASH)
#define hash(dev, block) hash_table[_hashfn(dev, block)]

/* 从hash队列和空闲队列中移走该缓冲块，因为每个缓冲块都是根据其“绑定”的设备号
 * 和逻辑块号来计算其在hash列表中的位置的，若我们要使用一个缓冲块，就需要先将
 * 其之前的绑定信息删除，即从hash队列中和空闲队列中移走，然后
 */
static inline void remove_from_queues(struct buffer_head * bh)
{
	/* 从hash队列中移除缓冲块 */
	if (bh->b_next)
		bh->b_next->b_prev = bh->b_prev;
	if (bh->b_prev)
		bh->b_prev->b_next = bh->b_next;

	/* 如果该缓冲块是该哈希表项的头一块，则让哈希表的对应项指向本队列中的下一个缓冲块 */
	if (hash(bh->b_dev, bh->b_blocknr) == bh)
		hash(bh->b_dev, bh->b_blocknr) = bh->b_next;

	/* 从空闲队列块表中移除该缓冲块，然后将其放在空闲链表的末尾，即最近被使用的块放在
	 * 链表末尾，而当寻找新的空闲块时，是从空闲链表头开始寻找，这样找到空闲缓冲块需要
	 * 的时间理论上会缩短。
	 */
	if (!(bh->b_prev_free) || !(bh->b_next_free))
		panic("Free block list corrupted");

	bh->b_prev_free->b_next_free = bh->b_next_free;
	bh->b_next_free->b_prev_free = bh->b_prev_free;

	if (free_list == bh)
		free_list = bh->b_next_free;
}

/*  */
static inline void insert_into_queues(struct buffer_head * bh)
{
	/* 首先将其放在空闲链表最后面 */
	bh->b_next_free = free_list;
	bh->b_prev_free = free_list->b_prev_free;
	free_list->b_prev_free->b_next_free = bh;
	free_list->b_prev_free = bh;

	/* 根据新的设备号和逻辑块号将该缓冲块放到新的哈希散列表项中 */
	bh->b_prev = NULL;
	bh->b_next = NULL;

	/* 注意，后来的块要放在散列表项的头部，类似堆栈操作 */
	if (!bh->b_dev)
		return;
	bh->b_next = hash(bh->b_dev, bh->b_blocknr);
	hash(bh->b_dev, bh->b_blocknr) = bh;

	if (bh->b_next)
		bh->b_next->b_prev = bh;
}


/* 利用hash表在高速缓冲区中寻找给定设备和指定块号的缓冲块 */
static struct buffer_head * find_buffer(int dev, int block)
{
	struct buffer_head * tmp;

	/* 搜索hash表，寻找指定设备块号的缓冲块。既然指定了dev和block，那么
	 * 就应该只有一个满足条件的元素，但是这里使用了for循环，即确定的位置
	 * 也可能会不满足？且链表中剩下的成员有可能满足的情况？（其实不是的，
	 * 不同的设备号和块号通过哈希函数计算后可能会落在相同的哈希表项中，
	 * 所以在结构体buffer_head中有b_next和b_prev成员分别指向同一个哈希
	 * 数组项中挂载的不同的缓冲头部）
	 */
	for (tmp = hash(dev, block); tmp != NULL; tmp = tmp->b_next)
	{
		if (tmp->b_dev == dev && tmp->b_blocknr == block)
			return tmp;
	}
	return NULL;
}

/* 利用hash表在高速缓冲去中寻找指定的缓冲块，若找到则对该缓冲块上锁并返回块头指针 */
struct buffer_head * get_hash_table(int dev, int block)
{
	struct buffer_head * bh;

	for (; ;)
	{
		if (!(bh = find_buffer(dev, block)))
			return NULL;
		bh->b_count++;

		/* 如果bh上锁，即被别的进程占用，则令当前进程睡眠在bh上的等待队列中 */
		wait_on_buffer(bh);

		/* 再次验证该缓冲块的正确性，正确则返回缓冲块头指针 */
		if (bh->b_dev == dev && bh->b_blocknr == block)
			return bh;

		/* 如不正确，则撤销本程序对其的引用计数，重新在for循环中寻找 */
		bh->b_count--;
	}
}

/* 同时判断缓冲块的修改标志和你锁定标志，并定义修改标志的权重比锁定标志大 */
#define BADNESS(bh) (((bh)->b_dirt << 1) + (bh)->b_lock)

/* 取高速缓冲中指定的缓冲块
 * 检查指定（设备号和块号）的缓冲块是否已经在高速缓冲中，如指定块已经在高速缓冲中，
 * 则返回对应缓冲块头部指针退出，反之就需要在高速缓冲中设置一个对应设备号和块号的
 * 新项，返回相应缓冲块头部指针。
 */
struct buffer_head * getblk(int dev, int block)
{
	struct buffer_head * tmp, * bh;

repeat:

	/* 搜索hash表，如果指定块已经在高速缓冲中，则返回对应缓冲块的头指针，退出。
	 * 这里注意，一共有两个列表，一个是哈希散列表，一个是空闲链表，一个缓冲块
	 * 一定在空闲链表中，有可能在哈希散列表中；存在于哈希列表中的缓冲块，意味
	 * 着它已经被分配给了某一个块设备，那么在后面如果想读取某个块设备中的数据
	 * 到缓冲区时，就首先从哈希散列表中找寻该设备之前是否已经分配了缓冲块，如
	 * 果有的话，一定会在哈希散列表中有记录，根据哈希函数就可以计算出该缓冲块
	 * 存在于哈希散列表（数组）的项数；如果没有，则只能从头开始搜索空闲链表，
	 * 找到一个空闲的缓冲块。
	 */
	if (bh = get_hash_table(dev, block))
		return bh;

	/* 否则就扫描空闲数据块链表，寻找空闲缓冲块 */
	tmp = free_list;

	do {
		/* b_count不为0，说明该缓冲块 不空闲，继续寻找 */
		if (tmp->b_count)
			continue;

		/* 没有在hash表中找到指定的缓冲块，则bh一定为NULL */
		if (!bh || BADNESS(tmp) < BADNESS(bh))
		{
			/* 将bh指向tmp所指向的缓冲块头部数据结构 */
			bh = tmp;

			/* 如果tmp既没有被锁定也没有被修改，说明我们找到了想要的缓冲块，退出循环 */
			if (!BADNESS(tmp))
				break;
		}
	} while ((tmp = tmp->b_next_free) != free_list);

	/* 如果扫表了全部的缓冲块依然没有找到合适的缓冲块，那么就要将当前进程睡眠在等待队列buffer_wait上 */
	if (!bh)
	{
		sleep_on(&buffer_wait);

		/* 当前进程被唤醒时（什么时候会被唤醒呢？），会执行到此行代码，然后重新回到repeat继续寻找空闲块 */
		goto repeat;
	}

	/* 执行到这里说明已经找到合适的空闲块了，先等待该缓冲区解锁（为啥还要解锁啊，既然找到了那么就应该是
	 * 不上锁的啊），如果在睡眠过程中该缓冲块又被其他进程占用，就只能再次寻找新的缓冲块了，这里的设计明
	 * 显存在缺陷，很可能有的进程会被饿死。
	 */
	wait_on_buffer(bh);

	/* 又被占用？ */
	if (bh->b_count)
		goto repeat;

	/* 如果该缓冲区已经被修改，则说明其中的数据不能被直接覆盖，需要将其写回到对应的磁盘缓冲块上，
	 * 每个缓冲区都隶属于一个设备，并保存其设备号，该缓冲区被修改的前提是其已经与一个设备相互绑定，
	 * 所以可以根据缓冲区头部数据结构中存储的设备号找到对应的磁盘空间。
	 */
	while (bh->b_dirt)
	{
		sync_dev(bh->b_dev);

		/* 再次等待缓冲区解锁 */
		wait_on_buffer(bh);

		/* 又被占用？ */
		if (bh->b_count)
			goto repeat;
	}

	/* 在hash表中检查指定的缓冲块是否乘我们睡眠时已经被加入进高速缓冲中，如果是，就再次重复上述找寻过程 */
	if (find_buffer(dev, block))
		goto repeat;

	/* 执行到这里就说明该缓冲块是指定参数唯一的一块，且目前还没有被占用，也没有被上锁，并且是干净的，
	 * 于是我们占用该缓冲块，置引用计数为1，复位修改标志和有效（更新）标志。
	 */
	bh->b_count = 1;
	bh->b_dirt = 0;

	/* 注意在这里，只是分配到了一个空闲的缓冲块，但是还没有向其中写入具体的数据，真正的写操作是异步进行的，
	 * 所以这里需要将数据有效标志置和修改标志（脏标志）置0，虽然还没有看到写操作函数的具体实现，但是我猜测
	 * 在写操作函数中一定会将这两个标志置1，后面看到了可以回来验证（2020.01.01 AM）。
	 */
	bh->b_uptodate = 0;

	/* 重新排列该缓冲块在哈希列表和空闲队列中的位置，首先将它移除这两个列表 */
	remove_from_queues(bh);

	/* 重新绑定设备号和逻辑块号 */
	bh->b_dev = dev;
	bh->b_blocknr = block;

	/* 根据新的设备号和逻辑块号将该缓冲块插入到两个队列中 */
	insert_into_queues(bh);
	return bh;
}

/* 释放指定缓冲块。
 * 释放是指将使用本缓冲块的进程数减1，若除当前任务外没有进程使用它，它就是自由的。
 */
void brelse(struct buffer_head *buf)
{
	if (!buf)
		return;

	wait_on_buffer(buf);

	if (!(buf->b_count--))
		panic("Trying to free free buffer");

	wake_up(&buffer_wait);
}

/* 从设备读取数据块到指定的缓冲区 
 * 参数中：dev - 设备号
 *         block - 磁盘上的数据块号
 * 首先在高速缓冲区申请一块缓冲块，如果该缓冲块中已经包含有效数据就直接返回该
 * 缓冲块指针，否则就从设备中读取指定的数据块到该缓冲块中并返回缓冲块指针。
 */
struct buffer_head * bread(int dev, int block)
{
	struct buffer_head * bh;

	if (!(bh = getblk(dev, block)))
		panic("bread: getblk returned NULL\n");

	/* 判断该数据是否是有效的，如果是有效的则直接返回使用 */
	if (bh->b_uptodate)
		return bh;

	/* 如果无效，则需调用底层设备读写函数，产生读设备请求，然后等待指定数据块被读入（异步操作），
	 * 因为执行读写操作是异步的，在读写操作进行过程中，进程需要睡眠在该缓冲块的等待队列上并等待
	 * 缓冲区解锁。
	 */
	ll_rw_block(READ, bh);
	wait_on_buffer(bh);

	/* 醒来后判断块上数据是否有效，若有效则返回该设备块 */
	if (bh->b_uptodate)
		return bh;

	/* 否则表示读设备操作失败，释放该缓冲块并返回NULL */
	brelse(bh);
	return NULL;
}

/* 从指定设备读取指定的一些块。
 * 函数参数个数可变，是一系列指定的块号，需要使用一个负数参数来表明参数列表的结束。
 * 成功时返回第1块的缓冲头指针，否则返回NULL。
 */
struct buffer_head * breada(int dev, int first, ...)
{
	va_list args;
	struct buffer_head * bh, *tmp;

	/* 首先取可变参数表中第1个参数（超级块的块号）到args中 */
	va_start(args, first);

	/* 接着根据设备号和块号来读取设备中的数据块到缓冲块中 */
	if (!(bh = getblk(dev, first)))
		panic("bread: getblk returned NULL\n");

	/* 如果数据无效，则还需要发出读设备数据块请求 */
	if (!bh->b_uptodate)
		ll_rw_block(READ, bh);

	/* 接着顺序读可变参数表中其他预读块号 */
	while ((first = va_arg(args, int)) >= 0)
	{
		tmp = getblk(dev, first);
		if (tmp)
		{
			if (!tmp->b_uptodate)
				ll_rw_block(READA, tmp);
			tmp->b_count--;
		}
	}

	va_end(args);
	wait_on_buffer(bh);
	if (bh->b_uptodate)
		return bh;
	brelse(bh);
	return NULL;
}

void buffer_init(long buffer_end)
{
	struct buffer_head * h = start_buffer;
	void * b;									// 实际缓冲区高端位置
	int i;

	if (buffer_end == 1<<20)					// 如果高速缓冲区结束位置为1MB
		b = (void *) (640*1024);
	else
		b = (void *) buffer_end;
	while ((b -= BLOCK_SIZE) >= ((void *) (h + 1)))
	{
		h->b_dev = 0;
		h->b_dirt = 0;
		h->b_count = 0;
		h->b_lock = 0;
		h->b_uptodate = 0;
		h->b_wait = NULL;
		h->b_next = NULL;
		h->b_prev = NULL;
		h->b_data = (char *) b;
		h->b_prev_free = h - 1;			// 指向链表中前一项，这里对于第一个缓冲块，其前一项已经不属于高速缓冲区的内容了，所以后面有处理
		h->b_next_free = h + 1;			// 指向链表中的下一项
		h++;
		NR_BUFFERS++;
		if (b == (void *) 0x100000)		// 如果b递减到等于1MB，则跳过384KB，为啥？
			b = (void *) 0xA0000;		// 让b指向地址0xA0000（640KB）处。
	}
	h--;								// 让h指向最后一个缓冲块头；
	free_list = start_buffer;			// 让空闲链表头指向第一个缓冲块头
	free_list->b_prev_free = h;			// 空闲链表头中的b_prev_free成员指向最后一个缓冲块头
	h->b_next_free = free_list;			// 让最后一个链表头的b_next_free成员指向第一个缓冲块头，从而形成了环形链表。
	for (i = 0; i < NR_HASH; i++)
	{
		hash_table[i] = NULL;
	}
}
