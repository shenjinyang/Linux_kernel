#include <linux/sched.h>

/* 内存中的i节点表，这个应该不是i节点位图，而是真正的i节点吧？是的 */
struct m_inode inode_table[NR_INODE] = {{0,},};

static void write_inode(struct m_inode * inode);

/* 等待指定的i节点可用，如该i节点不可用，则将当前任务置为不可中断的等待状态，
 * 并添加到该i节点的等待队列i_wait中，直到该节点解锁并明确地唤醒本任务。
 */
static inline void wait_on_inode(struct m_inode * inode)
{
	cli();
	while (inode->i_lock)
		sleep_on(&inode->i_wait);
	sti();
}

/* 锁定指定的i节点，如当前i节点已被上锁，则需要将当前任务置为不可中断的等待状态，
 * 并添加到当前i节点的等待队列中，直到该i节点解锁并明确唤醒本任务，再对其上锁。
 */
static inline void lock_inode(struct m_inode * inode)
{
	cli();
	while (inode->i_lock)
		sleep_on(&inode->i_wait);
	inode->i_lock = 1;
	sti();
}

static inline void unlock_inode(struct m_inode * inode)
{
	inode->i_lock = 0;
	wake_up(&inode->i_wait);
}

/* 同步所有i节点。
 * 把内存i节点表中所有的i节点与设备上i节点作同步操作。
 */
void sync_inodes(void)
{
	int i;
	struct m_inode * inode;

	inode = 0 + inode_table;

	/* 扫描整个i节点表，针对每个i节点，先等待该i节点解锁类型可用（若已被上锁），
	 * 然后判断该i节点是否被修改并且不是管道节点，若是这种情况则将该i节点写入
	 * 高速缓冲区中。缓冲区管理程序buffer.c会在适当时机将它们写入盘中。
	 */
	for (i = 0; i < NR_INODE; i++, inode++)
	{
		/* 等待该i节点可用（解锁） */
		wait_on_inode(inode);

		/* 若该节点已修改（修改就要置位i_dirt标志，在哪里？）且不是管道节点，则写盘 */
		if (inode->i_dirt && !inode->i_pipe)
			write_inode(inode);
	}
}

static void write_inode(struct m_inode * inode)
{
	struct super_block * sb;
	struct buffer_head * bh;
	int block;

	lock_inode(inode);

	/* 如果当前i节点没有被修改过，或该i节点的设备号为0（按理说所有i节点应该有相同的设备号啊？
	 * 是的，同一个设备上的i节点上都拥有相同的设备号，但是一块容量较大的硬盘，可能会被分出几个
	 * 区，那么每个区都是一个独立的块设备，有着自己的设备号、主引导扇区等结构。我们需要找到
	 * 当前i节点所在的设备号，并取其对应的超级块），
	 * 则解锁该节点并退出，否则获取该i节点的超级块。
	 */
	if (!inode->i_dirt || !inode->i_dev)
	{
		unlock_inode(inode);
		return;
	}

	/* 获取该i节点的超级块（i节点所在设备的超级块，i节点和其对应的超级块有着相同的设备号） */
	if (!(sb = get_super(inode->i_dev)))
		panic("trying to write inode without device");

	/* 变量block表示i节点所在的块号 =（启动块+超级块）2 + i节点位图占用块 + 逻辑块位图占用块 + 
	 *                               （i节点号 - 1）/每块包含的i节点数
	 */
	block = 2 + sb->s_imap_blocks + sb->s_zmap_blocks + (inode->i_num - 1)/INODES_PER_BLOCK;

	/* 读取i节点所在的块到高速缓存中 */
	if (!(bh = bread(inode->i_dev, block)))
		panic("unable to read i-node block");

	/* 将上面得到的高速缓冲区块强制转换设置为d_inode类型的数组（共有INODES_PER_BLOCK项），
	 * 然后将内存中的i节点强制转换为d_inode类型，即只保存inode前7项内容（对应盘设备上的
	 * i节点），并将其存储在高速缓冲区中i节点号对应的数组项中。待缓冲区刷新时（什么时候？
	 * 怎么刷新？）会写入到盘中。
	 */
	((struct d_inode *)bh->b_data)[(inode->i_num - 1] % INODES_PER_BLOCK = *(struct d_inode *)inode;

	/* 将该高速缓冲块脏标志置1，表示已经修改过，那么等后面有进程申请到这块缓冲块时，发现它是脏的，就会
	 * 将其写入到磁盘上，因此这里的同步实际上是异步的。
	 */
	bh->b_dirt = 1;

	/* 这个i节点到这里已经写入完成了，将脏标志清0 */
	inode->i_dirt = 0;

	/* 这个函数想了半天，不是它实现的逻辑复杂，而是没有想通为什么要释放这个i节点所在的内存块，
	 * 是这样的，经过上面的一系列操作，高速缓冲区中的i节点数据已经和内存中的数据同步了，下面
	 * 只需要等待高速缓冲区刷新即可，但是高速缓冲区什么时候刷新呢？很简单就是本块高速缓冲块
	 * 被别的进程申请使用时，申请其的进程会检查该高速缓冲块的脏标志位，为1说明已经修改，那么
	 * 在使用之前就需要将其同步到设备中去。也就是说，直到该高速缓冲区被其他进程使用，inode同
	 * 步才算真正的完成了。
	 */
	brelse(bh);

	unlock_inode(inode);
}