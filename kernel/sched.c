#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>
#include <asm/io.h>
#include <signal.h>

#define LATCH (1193180/HZ)

/* extern关键字，函数定义在kernel/system_call.s中 */
extern int timer_interrupt(void);
extern int system_call(void);

/* 每个任务（或进程）的特征数据结构，被声明为联合数据类型。
 * 联合中的宏定义“PAGE_SIZE”的值是4096，这里需要注意的是union的成员使用相同的内存，
 * 即占用内存大小以最大的成员为准，成员结构体“task”肯定小于4096字节，实际上是104字节，
 * 所以联合大小就是4096字节。
 * 因此可以得知，一个任务的数据结构与其内核态堆栈放在同一个内存页中，即可以从堆栈段寄存器
 * ss中获得其数据段选择符。
 */
union task_union {
	struct task_struct task;
	char stack[PAGE_SIZE];
};

/* 为联合体init_task赋值，将其成员task赋值为宏定义INIT_TASK，而成员task则被初始化为0？ */
static union task_union init_task = {INIT_TASK,};

long startup_time = 0;

/* 定义一个数组（数组的成员是指针），成员指向每个对应任务的task结构体地址 */
struct task_struct *task[NR_TASKS] = {&(init_task.task), };

/* 定义用户栈，名字虽然叫user，实际上在内核初始化前期，它一直被用作系统的临时栈，
 * 在创建第一个进程即任务0后，被用作任务0的用户态堆栈空间。
 */
long user_stcak[PAGE_SIZE>>2];

/* stact_start是系统初始化时（启动任务0之前）使用的临时堆栈，在head.s中被加载，
 * 任务0启动后会被用作任务0的用户栈。
 */
struct {
	long *a;		// 栈顶指针，赋值为数组user_stcak最后一个元素
	short b;		// 段选择符，0x10是内核数据段的段选择符
} stact_start = { & user_stcak[PAGE_SIZE>>2], 0x10 };

void sched_init(void)
{
	int i;
	struct desc_struct * p;

	if (sizeof(struct sigaction) != 16)
		panic("Struct sigaction MUST be 16 bytes");
	set_tss_desc(gdt+FIRST_TSS_ENTRY, &(init_task.task.tss));		// 设置任务0的tss段描述符，参数gdt实际上是在boot/head.s中定义的
	set_ldt_desc(gdt+FIRST_LDT_ENTRY,&(init_task.task.ldt));		// 设置任务0的ldt段描述符
	p = gdt + 2 + FIRST_TSS_ENTRY;									// 将p指向任务0 tss和ldt后的gdt表项
	for (i = 1; i < NR_TASKS; i++)									// 将剩余的gdt表项全部清零
	{
		task[i] = NULL;
		p->a = p->b = 0;
		p++;
		p->a = p->b = 0;
		p++;
	}
	/* EFLAGS中的NT标志位用于控制任务的嵌套调用。当NT置位时，当前中断任务执行IRET时
	 * 就会引起任务切换。NT指出TSS中的back_link字段是否有效，NT = 0时无效。
	 */
	__asm__("pushfl; andl $0xffffbfff, (%esp); popfl");

	/* 将任务0的tss段选择符加载到寄存器tr中 */
	ltr(0);

	/* 将GDT中相应LDT描述符的端选择符加载到寄存器ldtr中。只明确加载这一次，以后的新任务
	 * LDT的加载，是由CPU根据TSS中的LDT项自动加载。
	 */
	lldt(0);

	/* 下面的代码用于初始化8253定时器。通道0，选择工作方式3，二进制计数方式	
	 * 通道0的输出引脚链接在中断控制主芯片上的IRQ0上，它每10ms发出一个IRQ0请求，
	 * LATCH是初始定时计数值。
	 */
	outb_p(0x36, 0x43);
	outb_p(LATCH & 0xff, 0x40);		// LSB：定时值高字节
	outb(LATCH>>8, 0x40);			// MSB：定时值低字节

	/* 设置时钟中断门（挂载时钟中断处理程序） */
	set_intr_gate(0x20, &timer_interrupt);

	/* 修改中断控制器屏蔽码，允许时钟中断 */
	outb(inb_p(0x21)&~0x01, 0x21);

	/* 挂载0x80中断处理程序，当在代码中执行语句“int $0x80”时，就会触发软中断，
	 * 中断号为0x80，中断表共有256个表项，每个表项8字节，0x80换算为十进制是128，
	 * 当中断触发时，就会调用中断处理函数system_call，至于system_call的具体实现。
	 * 那就是后话了。
	 */
	set_system_gate(0x80, &system_call);
}
