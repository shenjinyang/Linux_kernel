#include <linux/kernel.h>
#include <linux/sched.h>

/* 对指定设备执行高速缓冲与设备上数据的同步操作 */
int sync_dev(int dev);

void wait_for_keypress(void);

/* 定义在内存中的超级块数组 */
struct super_block super_block[NR_SUPER];

/* 根文件系统设备号，已经在init/mian.c中初始化。 */
int ROOT_DEV = 0;

static void wait_on_super(struct super_block * sb)
{
	cli();


	while (sb->s_lock)
	{
		sleep_on(&sb->s_wait);
	}

	sti();
}

/* 取指定设备的超级块 */
struct super_block * get_super(int dev)
{
	struct super_block * s;

	if (!dev)
		return NULL;

	s = 0 + super_block;

	while (s < NR_SUPER + super_block)
	{
		/* 如果找到与i节点设备号相同的超级块 */
		if (s->s_dev == dev)
		{
			/* 检查如果该超级块被上锁，则让当前进程睡眠在超级块上的等待队列中 */
			wait_on_super(s);

			/* 进程醒来后再次检查一次i节点设备号是否与超级块相同，相同则返回该超级块 */
			if (s->s_dev == dev)
				return s;

			/* 否则重新从超级块第0项开始查找 */
			s = 0 + super_block;
		}

		/* 如此项超级块中的设备号与i节点的设备号不相同，则将超级块数组指针加1指向下一项 */
		else
			s++;

		/* 如果没有找到，返回NULL */
		return NULL;
	}
}

void mount_root(void)
{
	int i, free;
	struct super_block * p;

	struct m_inode * mi;

	if (32 != sizeof (struct d_inode))
		panic("bad i-node size");

	/* 将文件结构中的引用计数置为0表示空闲 */
	for (i = 0; i < NR_FILE; i++)
		file_table[i].f_count = 0;

	/* 如果根文件系统为软盘,则提示插入磁盘（所以根文件系统必须是软盘？） */
	if (MAJOR(ROOT_DEV) == 2)
	{
		printk("Insert root floppy and press ENTER");
		wait_for_keypress();
	}
}