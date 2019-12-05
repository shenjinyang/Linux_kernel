#include <asm/system.h>
#include <asm/io.h>

#define MAJOR_NR 3
#include "blk.h"

/* 该函数在system_call.s中定义 */
extern void hd_interrupt(void);


/* 这个函数没有找到函数原型在哪里声明，很奇怪 */
void do_hd_request(void)
{
}

/* 硬盘系统初始化
 * 设置硬盘中断描述符，并允许硬盘控制器发送中断请求信号
 */
void hd_init(void)
{
	/* 变量blk_dev是在 kernel\blk_drv\ll_rw_blk.c  中定义的，在blk.h中有使用extern关键字声明的语句 */
	blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;
	set_intr_gate(0x2E, &hd_interrupt);		// 设置中断门中的处理函数指针，中断号为0x2E
	outb_p(inb_p(0x21)&0xfb, 0x21);			// 复位主片上接联引脚屏蔽位（位2）
	outb(inb_p(0xA1)&0xbf, 0xA1);			// 复位从片上硬盘中断请求屏蔽位（位6）
}