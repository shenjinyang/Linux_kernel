#ifndef _TTY_H
#define _TTY_H

#define MAX_CONSOLES 	8		/* 最大虚拟控制台数量 */
#define NR_SERIALS		2		/* 串行终端数量 */
#define NR_PTYS			4		/* 伪终端数量 */

#include <termios.h>

#define TTY_BUF_SIZE 1024

/* tty字符缓冲队列数据结构，用于tty_struct结构中的读、写和辅助缓冲队列 */
struct tty_queue {
	unsigned long data;					/* 队列缓冲区中含有字符行数值（不是当前字符数） */
										/* 对于串口终端，则存放串行端口地址 */
	unsigned long head;					/* 缓冲区中数据头指针 */
	unsigned long tail;					/* 缓冲区中数据尾指针 */
	unsigned task_struct *proc_list;	/* 等待本队列的进程表 */
	char buf[TTY_BUF_SIZE];				/* 队列的缓冲区 */
};

/* tty数据结构 */
struct tty_struct {
	struct termios termios;						/* 终端io属性和控制字符数据结构 */
	int pgrp;									/* 所属进程组 */
	int session;								/* 会话号 */
	int stopped;								/* 停止标志 */
	void (*write)(struct tty_struct * tty);		/* tty写函数指针 */
	struct tty_queue *read_q;					/* tty读队列 */
	struct tty_queue *write_q;					/* tty写队列 */
	struct tty_queue *secondary;				/* tty辅助队列（存放规范模式字符序列），可称为规范（熟）模式队列 */
};

extern int fg_console;

/* 中断 intr=^C 
 * 退出 quit=^|
 * 删除 erase=del
 * 终止 kill=^U
 * 文件结束 eof=^D vtime=\0 vmin=\1 sxtc=\0
 * 开始 start=^Q
 * 停止 stop=^S
 * 挂起 susp=^Z
 * 行结束 eol=\0
 * 重显 reprint=^R
 * 丢弃 discard=^U
 * werase=^W
 * lnext=^V
 * 行结束 eol2=\0
 */
#define INIT_C_CC "\003\034\177\025\004\0\1\0\021\023\032\0\022\017\027\026\0"

void rs_init(void);
void con_init(void);
void tty_init(void);

void update_screen(void);

#endif
