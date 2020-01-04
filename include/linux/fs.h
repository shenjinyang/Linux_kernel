#ifndef _FS_H
#define _FS_H

#include <sys/types.h>

#define READ 0
#define WRITE 1
#define READA 2
#define WRITEA 3

void buffer_init(long buffer_end);

/* 设备好分为主设备号和次设备号，共16位，其中高字节是主设备号，低字节是次设备号 */
#define MAJOR(a) (((unsigned)(a))>>8)
#define MINOR(a) ((a)&0xff)

/* MINIX文件系统魔数 */
#define SUPER_MAGIC 0x137F

/* 进程最多打开文件数 */
#define NR_OPEN 20

/* 系统支持的最多使用的I节点的个数（即最多支持64个文件和目录项？） */
#define NR_INODE 64

/* 系统中最多文件个数（文件数组项数） */
#define NR_FILE 64

/* 系统所含超级块数（超级块数组项数） */
#define NR_SUPER 8

/* 缓冲区hash表数组项数值 */
#define NR_HASH 307

/* 系统所含缓冲块个数，初始化之后不再变化，这里只找到extern关键字的nr_buffers定义，
 * 没有在外部文件中找到其定义的地方，略感奇怪。
 */
#define NR_BUFFERS nr_buffers

/* 定义每个缓冲块大小为1024字节 */
#define BLOCK_SIZE 1024

#define BLOCK_SIZE_BITS 10

/* 每个逻辑块可以存放的i节点数，这里的i节点数是指在磁盘上的*/
#define INODES_PER_BLOCK ((BLOCK_SIZE)/(sizeof(struct d_inode)))

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

/* 这是在盘设备上使用的i节点数据结构 */
struct d_inode {
	unsigned short i_mode;
	unsigned short i_uid;
	unsigned long i_size;
	unsigned long i_time;
	unsigned char i_gid;
	unsigned char i_nlinks;
	unsigned short i_zone[9];
};

/* 这是在内存中的i节点结构。前7项与d_mode完全一样 */
struct m_inode {
	unsigned short i_mode;		/* 文件类型和和属性（rwx） */
	unsigned short i_uid;		/* 用户id（文件拥有者标识） */
	unsigned long i_size;		/* 文件大小（字节数） */
	unsigned long i_mtime;		/* 修改时间 */
	unsigned char i_gid;		/* 组id（文件所有者所在组） */
	unsigned char i_nlinks;		/* 文件目录项链接数 */
	unsigned short i_zone[9];	/* 直接（0-6）、间接（7）或双重间接（8）逻辑块号 */

	/* 只在内存中使用的成员 */
	struct task_struct * i_wait;	/* 等待i节点的进程 */
	struct task_struct * i_wait2;	/* for pipes（什么意思？） */
	unsigned long i_atime;
	unsigned long i_ctime;
	unsigned short i_dev;			/* i节点所在的设备号 */
	unsigned short i_num;
	unsigned short i_count;			/* i节点被使用的次数，0表示该i节点空闲 */
	unsigned char i_lock;
	unsigned char i_dirt;
	unsigned char i_pipe;
	unsigned char i_mount;
	unsigned char i_seek;
	unsigned char i_update;
};

/* 文件结构（用于在文件句柄与i节点之间建立关系） */
struct file {
	unsigned short f_mode;		/* 文件操作模式（RW位） */
	unsigned short f_flags;		/* 文件打开和控制的标志 */
	unsigned short f_count;		/* 对应文件引用计数值 */
	struct m_inode * f_inode;	/* 指向对应的i节点 */
	off_t f_pos;				/* 文件位置（读写偏移） */
};

struct super_block {
	unsigned short s_ninodes;				/* i节点总数 */
	unsigned short s_nzones;				/* 逻辑块总数（或称为区块数） */
	unsigned short s_imap_blocks;			/* i节点位图所占块数 */
	unsigned short s_zmap_blocks;			/* 逻辑块位图所占块数 */
	unsigned short s_firstdatazone;			/* 数据区中第一个逻辑块块号 */
	unsigned short s_log_zone_size;			/* 使用以2为底的对数表示的每个逻辑块包含的磁盘块数 */
	unsigned long s_max_size;				/* 支持的最大文件长度 */
	unsigned short s_magic;					/* 文件系统标志魔数 */
/* These are only in memory */
	struct buffer_head * s_imap[8];			/* i节点位图所在高速缓冲块指针数组，最多支持8个缓冲块存放i节点，即8*1024=8192个字节 */
	struct buffer_head * s_zmap[8];			/* 逻辑块位图所在高速缓冲块指针数组，同样是8个 */
	unsigned short s_dev;					/* 超级块所在的设备号 */
	struct m_inode * s_isup;				/* 被安装文件系统的根目录i节点（不太清楚） */
	struct m_inode * s_imount;				/* 该文件系统被安装到的i节点（迷惑） */
	unsigned long s_time;					/* 修改时间 */
	struct task_struct * s_wait;			/* 等待本超级块的进程指针 */
	unsigned char s_lock;					/* 锁定标志 */
	unsigned char s_rd_only;				/* 只读标志 */
	unsigned char s_dirt;					/* 已经被修改标志（脏标志） */
};

/* 这是存放在盘设备上的超级块数据结构 */
struct d_super_block {
	unsigned short s_ninodes;
	unsigned short s_nzones;
	unsigned short s_imap_blocks;
	unsigned short s_zmap_blocks;
	unsigned short s_firstdatazone;
	unsigned short s_log_zone_size;
	unsigned long s_max_size;
	unsigned short s_magic;
};


extern void sync_inodes(void);
extern struct buffer_head * get_hash_table(int dev, int block);
extern struct buffer_head * getblk(int dev, int block);
extern void ll_rw_block(int rw, struct buffer_head *bh);
extern void ll_rw_page(int rw, int dev, int nr, char * buffer);
extern void brelse(struct buffer_head *buf);
extern struct buffer_head * bread(int dev, int block);
extern struct buffer_head * breada(int dev, int block, ...);
extern int sync_dev(int dev);
extern struct super_block * get_super(int dev);
extern int ROOT_DEV;

extern void mount_root(void);
#endif
