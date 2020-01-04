#include <linux/sched.h>
#include "blk.h"

struct request request[NR_REQUEST];

/* 用于在请求数组没有空闲项时进程的临时等待处 */
struct task_struct * wait_for_request = NULL;

struct blk_dev_struct blk_dev[NR_BLK_DEV] = {
	{ NULL, NULL },		/* no_dev */
	{ NULL, NULL },		/* dev mem */
	{ NULL, NULL },		/* dev fd */
	{ NULL, NULL },		/* dev hd */
	{ NULL, NULL },		/* dev ttyx */
	{ NULL, NULL },		/* dev tty */
	{ NULL, NULL }		/* dev lp */
};

/* 设备数据块总数组指针数组，每个指针项指向指定主设备号的总块数数组，该总块数数组的
 * 每一项对应子设备好确定的一个子设备上所拥有的数据块总数（1块 = 1KB）
 */
int * blk_size[NR_BLK_DEV] = { NULL, NULL };

/* 锁定指定缓冲块
 * 如果指定的缓冲块已经被其他任务锁定，则使自己进入不可中断的睡眠状态，直到执行
 * 解锁的任务（到底是什么任务负责唤醒啊？）明确唤醒该任务。
 */
static inline void lock_buffer(struct buffer_head * bh)
{
	cli();

	while (bh->b_lock)
		sleep_on(bh->b_wait);

	/* 当前进程被唤醒后立刻锁定该缓冲区 */
	bh->b_lock = 1;

	sti();
}

static inline void unlock_buffer(struct buffer_head * bh)
{
	if (!bh->b_lock)
		printk("ll_rw_block.c: buffer not locked\n\r");
	bh->b_lock = 0;
	wake_up(&bh->b_wait);
}

/* 总有一个时刻，某个设备的请求项等待队列是空的，例如系统
 * 启动后的第一个请求项，那么针对该请求项，函数就会直接调用
 * 请求项处理函数，在该请求项被处理的过程中，假设有别的针对
 * 该设备的请求项到来，那么因为设备此时处于繁忙状态，后续到
 * 来的请求项就会被插入到请求项等待队列中，一旦第一个请求项
 * 处理完，处理函数发现等待队列中还有其他的请求项，就会接着
 * 去处理，直到等待队列为空。也就是说，无论什么时刻，设备的
 * 请求项处理函数只有两种状态，一种是该设备一段时间内没有请
 * 求，处理函数不会被调用；一种是处理函数一直在处理请求项的
 * 请求，也就是说，加入请求项等待队列中的请求项，要么会被立
 * 处理，要么就是等待其前面的请求项被处理完成后被处理。所以
 * 只要将对块设备的读写请求项加入到队列中，就已经算是完成了
 * 一半的读写操作了，剩下的工作都是异步完成的。
 */
static void add_request(struct blk_dev_struct *dev, struct request *req)
{
	struct request * tmp;

	req->next = NULL;
	cli();

	/* 为什么要把缓冲块中的脏标志清除掉呢？ */
	if (req->bh)
		req->bh->b_dirt = 0;

	/* 如果当前设备的请求队列为空，就将当前的请求指向req */
	if (!(tmp = dev->current_request))
	{
		dev->current_request = req;
		sti();

		/* 调用该设备对应的处理函数 */
		(dev->request_fn)();
		return;
	}

	for (; tmp->next; tmp = tmp->next)
	{
		/* 如果当前请求项没有对应的缓冲块，那么就暂时不管这个请求项，
		 * 查找下一个直到找到有对应缓冲块的请求项。
		 */
		if (!req->bh)
		{
			if (tmp->next->bh)
				break;
			else
				continue;
		}

		/* 应用电梯算法将req所指的请求项与请求队列中已有的请求项作比较，找出req插入该队列
		 * 的正确顺序位置。
		 */
		if ((IN_ORDER(tmp, req) || !IN_ORDER(tmp, tmp->next)) && IN_ORDER(req, tmp->next))
			break;
	}

	req->next = tmp->next;
	tmp->next = req;
	sti();
}


/* 创建请求项并插入请求队列中
 * 参数：major 	- 	主设备号
 *       rw 	- 	指定命令
 *       bh		-	存放数据的缓冲区头指针
 */
static void make_request(int major, int rw, struct buffer_head * bh)
{
	struct request * req;

	/* 预读写标志 */
	int rw_ahead;

	if (rw_ahead = (rw == READA || rw == WRITEA))
	{
		if (bh->b_lock)
			return;
		if (rw == READA)
			rw = READ;
		else
			rw = WRITE;
	}

	if (rw != READ && rw != WRITE)
		panic("Bad block dev command, must be R/W/RA/WA");

	/* 锁定该缓冲区 */
	lock_buffer(bh);

	/* b_dirt对应写操作，b_uptodate对应读操作，如果是写操作且b_dirt为复位，表示写
	 * 操作已经完成；如果是读操作且b_uptodate为置位，表示读操作已经完成，那么就可
	 * 以解锁该缓冲块，并唤醒该缓冲块上等待队列中的任务。
	 */
	if ((rw == WRITE && !bh->b_dirt) || (rw == READ && bh->b_uptodate))
	{
		/* 直接就解锁返回了？也没有看见写操作啊 */
		unlock_buffer(bh);
		return;
	}
	repeat:

	if (rw == READ)
		req = request + NR_REQUEST;

	else
		req = request + ((NR_REQUEST * 2) / 3);

	/* 从后向前扫描请求项，试图找到一个空的请求项，即dev = -1的项 */
	while (--req >= request)
		if (req->dev < 0)
			break;

	/* 如果没有找到空闲的请求项，就需要让本次请求操作睡眠，首先需检查是否为预读写 */
	if (req < request)
	{
		if (rw_ahead)
		{
			unlock_buffer(bh);
			return;
		}

		sleep_on(&wait_for_request);

		/* 进程被唤醒后，就重新回到上面去查找空闲项。 */
		goto repeat;
	}

	/* 代码执行到这里，表示已经找到了空闲项，则向空闲请求项中填入请求信息，并将其加入到请求队列中 */
	req->dev = bh->b_dev;
	req->cmd = rw;
	req->errors = 0;
	req->sector = bh->b_blocknr<<1;		/* 块号转换成扇区数，1块等于2个扇区 */
	req->nr_sectors = 2;				/* 1块为2个扇区 */
	req->buffer = bh->b_data;			/* 请求项缓冲区指针，指向需读写的数据缓冲区 */
	req->waiting = NULL;				/* 任务等待操作执行完成的地方（什么意思？） */
	req->bh = bh;						/* 缓冲块头指针 */
	req->next = NULL;					/* 指向下一个请求项 */
	add_request(major + blk_dev, req);	/* 将请求项加入到请求队列中 */
}

/* 低级页面读写函数
 * 以页面为单位访问块设备数据，即每次读取8个扇区，猜测也是异步执行的
 */
void ll_rw_page(int rw, int dev, int page, char *buffer)
{
	struct request * req;
	unsigned int major = MAJOR(dev);

	if (major >= NR_BLK_DEV || !(blk_dev[major].request_fn))
	{
		printk("Trying to read nonexistent block-device\n\r");
		return;
	}

	if (rw != READ && rw != WRITE)
		panic("Bad block dev command, must be R/W");

repeat:
	/* 从等待队列尾部向前搜索 */
	req = request + NR_REQUEST;

	/* 找到一个空闲的等待队列项，将本次读/写操作插入其中 */
	while (--req >= request)
		if (req->dev < 0)
			break;

	/* 如果等待队列满了，需要将当前进程睡眠在等待队列上 */
	if (req < request)
	{
		sleep_on(&wait_for_request);

		/* 醒来后重新查找 */
		goto repeat;
	}

	/* 若找到，则填充等待队列项 */
	req->dev = dev;
	req->cmd = rw;
	req->errors = 0;
	req->sector = page << 3;	/* 起始读写扇区，1页8个扇区 */
	req->nr_sectors = 8;		/* 读取1页，即8个扇区 */
	req->buffer = buffer;		/* 读到哪个物理页面去 */
	req->waiting = current;		/* 当前进程进入该等待队列 */
	req->bh = NULL;				/* 不需要缓冲区，直接读到内存中（其实缓冲区也是内存） */

	/* 咋是NULL呢？对了，就应该是NULL，搜索请求队列的时候都是从后向前搜索的，说明请求插入的顺序是从前向后插入的 */
	req->next = NULL;
	current->state = TASK_UNINTERRUPTIBLE;
	add_request(major + blk_dev, req);

	/* 在低级数据块读写函数中，将请求插入队列后就直接返回了，是因为块读写一次只读2个扇区，时间较短，而低级页读写
	 * 函数则需要读8个扇区，相对时间较长，所以选择让当前进程睡眠，然后调用schedule函数去执行其他进程。
	 */
	schedule();
}

/* 低级数据块读写函数
 * 该函数是块设备驱动程序与系统其他部分之间的接口函数。其作用的创建块设备读写请求项
 * 并插入到指定块设备请求队列中。
 */
void ll_rw_block(int rw, struct buffer_head * bh)
{
	unsigned int major;

	/* 系统一共设置了7种类型的块设备，每种块设备都对应了一个请求处理函数，而各自的
	 * 请求处理函数都是在各自块设备的初始化函数中注册的，这里有疑问的点是主设备号的
	 * 确定。
	 * 如果设备的主设备号不存在或者设备的请求操作函数不存在，则报错并返回。
	 */
	if ((major = MAJOR(bh->b_dev)) >= NR_BLK_DEV || !(blk_dev[major].request_fn))
	{
		printk("Trying to read nonexistent block-device\n\r");
		return;
	}

	/* 创建块设备读写的请求项 */
	make_request(major, rw, bh);
}

void blk_dev_init(void)
{
	int i;

	for (i = 0; i < NR_REQUEST; i++) {
		request[i].dev 	= -1;
		request[i].next	= NULL; 
	}
}