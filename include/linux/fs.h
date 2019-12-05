#ifndef _FS_H
#define _FS_H

void buffer_init(long buffer_end);

/* 缓冲区hash表数组项数值 */
#define NR_HASH 307

/* 系统所含缓冲块个数，初始化之后不再变化 */
#define NR_BUFFERS nr_buffers

/* 定义每个缓冲块大小为1024字节 */
#define BLOCK_SIZE 1024

/* 缓冲块头部数据结构，极为重要 */
struct buffer_head {
	char * b_data;							// 指向该缓冲块中数据区（1024字节）的指针
	unsigned long b_blocknr;				// 块号
	unsigned short b_dev;					// 数据源的设备号
	unsigned char b_uptodate;				// 更新标志，表示数据是否已经更新
	unsigned char b_dirt;					// 修改标志，0-未修改，1-已修改
	unsigned char b_count;					// 使用的用户数
	unsigned char b_lock;					// 锁定标志位，0-未锁定，1-已锁定
	struct task_struct * b_wait;			// 指向等待该缓冲区解锁的任务
	struct buffer_head *b_prev;				// hash队列上前一块（这四个指针用于缓冲区的管理）
	struct buffer_head *b_next;				// hash队列上下一块
	struct buffer_head *b_prev_free;		// 空闲表上前一块
	struct buffer_head *b_next_free;		// 空闲表上下一块
};
