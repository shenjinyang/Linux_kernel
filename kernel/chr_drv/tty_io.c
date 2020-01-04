#include <tty.h>

/* QUEUES是tty终端使用的缓冲队列最大数量，共54项 */
#define QUEUES (3 * (MAX_CONSOLES + NR_SERIALS + 2 * NR_PTYS))

/* tty缓冲队列数组，每个tty终端使用3个tty缓冲队列：
 * a - 用于缓冲键盘或串行输入的读队列read_queue；
 * b - 用于缓冲屏幕或串行输出的写队列write_queue；
 * c - 用于保存规范模式字符的辅助缓冲队列secondary。
 */
static struct tty_queue tty_queues[QUEUES];

/* tty表结构数组 */
struct tty_struct tty_table[256];

/* 定义各种类型的tty终端所使用的缓冲队列结构在tty_queues[]数组中的起始项位置。
 * 8个虚拟控制台终端占用tty_queues[]数组开头24项（0 -- 23）；
 * 2个串行终端占用随后6项（24 -- 29）；
 * 4个主伪终端占用随后12项（30 -- 41）；
 * 4个从伪终端占用随后12项（42 -- 53）。
 */
#define con_queues tty_queues
#define rs_queues ((3 * MAX_CONSOLES) + tty_queues)
#define mpty_queues ((3 * (MAX_CONSOLES + NR_SERIALS) + tty_queues))
#define spty_queues ((3 * (MAX_CONSOLES + NR_SERIALS + NR_PTYS) + tty_queues))

int fg_console = 0;

/* 等待按键，如果前台控制台读队列缓冲区为空，则让进程进入到可中断睡眠状态 */
void wait_for_keypress(void)
{
	sleep_if_empty(tty_table[fg_console].secondary);
}

/* tty终端初始化函数。
 * 初始化所有中断缓冲队列，初始化串口终端和控制台终端。
 */
void tty_init(void)
{
	int i;

	/* 初始化所有终端的缓冲队列结构，设置初值 */
	for (i = 0; i < QUEUES; i++)
		tty_queues[i] = (struct tty_queue) {0, 0, 0, 0, ""};

	/* 对于串口终端的读/写缓冲队列，将它们的data字段设置为串行端口基地址，串口1是0x3f8，串口2是0x2f8 */
	rs_queues[0] = (struct tty_queue) {0x3f8, 0, 0, 0, ""};
	rs_queues[1] = (struct tty_queue) {0x3f8, 0, 0, 0, ""};
	rs_queues[3] = (struct tty_queue) {0x2f8, 0, 0, 0, ""};
	rs_queues[4] = (struct tty_queue) {0x2f8, 0, 0, 0, ""};

	/* 初步设置所有终端的tty结构 */
	for (i = 0; i < 256; i++)
	{
		tty_table[i] = (struct tty_struct) {
			{0, 0, 0, 0, 0, INIT_C_CC},
			0, 0, 0, NULL, NULL, NULL, NULL
		}
	}

	/* 初始化控制台终端 */
	con_init();
	rs_init();
	
}

void chr_dev_init(void)
{
}
