/* 块设备类型数量 */
#define NR_BLK_DEV 7

/* 该宏是定义请求队列中所包含的项数，什么请求队列？估计是块设备排队吧 */
#define NR_REQUEST 32

struct request {
	int dev;		/* -1 if no request */	// 发出请求的设备号
	int cmd;		/* READ or WRITE */
	int errors;								// 读操作时产生的错误次数
	unsigned long sector;					// 起始扇区
	unsigned long nr_sectors;				// 读、写扇区数
	char * buffer;							// 数据缓冲区
	struct task_struct * waiting;			// 任务等待请求完成操作的地方（队列）。
	struct buffer_head * bh;				// 缓冲区头指针。
	struct request * next;					// 指向下一个请求项。
};

struct blk_dev_struct {
	void (*request_fn)(void);
	struct request * current_request;
};

extern struct blk_dev_struct blk_dev[NR_BLK_DEV];

#ifdef MAJOR_NR

#if (MAJOR_NR == 1)
/* ram disk */
#define DEVICE_NAME "ramdisk"				// 设备名称
#define DEVICE_REQUEST do_rd_request		// 设备请求项处理函数，只看到了函数定义，没有看到函数原型，很奇怪
#define DEVICE_NR(device) ((device) & 7)	// 子设备号（0 - 7）
#define DEVICE_ON(device)					// 开启设备（虚拟盘无需开启和关闭，所以被定义为空）
#define DEVICE_OFF(device)					// 关闭设备

#elif (MAJOR_NR == 2)
/* floppy */
#define DEVICE_NAME "floppy"
#define DEVICE_INTR do_floppy				// 设备中断处理函数
#define DEVICE_REQUEST do_fd_request
#define DEVICE_NR(device) ((device) & 3)
#define DEVICE_ON(device) floppy_on(DEVICE_NR(device))
#define DEVICE_OFF(device) floppy_off(DEVICE_NR(device))

#elif (MAJOR_NR == 3)
/* harddisk */
#define DEVICE_NAME "harddisk"
#define DEVICE_INTR do_hd					// 设备中断处理函数
#define DEVICE_REQUEST do_hd_request
#define DEVICE_NR(device) (MINOR(device)/5)
#define DEVICE_ON(device)					// 一开机硬盘就总是运转着，所以这两个宏同样不需要，定义为空。
#define DEVICE_OFF(device)