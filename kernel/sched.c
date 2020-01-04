#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/sys.h>
#include <asm/system.h>
#include <asm/io.h>
#include <signal.h>

/* 该宏取信号nr在信号位图中对应位的二进制数值。信号编号1-32。
 * 例如，信号5的位图等于1<<(5-1) = 16 = 00010000b。
 */
#define _S(nr) (1<<((nr) - 1))
#define _BLOCKABLE (~(_S(SIGKILL) | _S(SIGSTOP)))

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

/* 为联合体init_task赋值，将其成员task赋值为宏定义INIT_TASK，而成员stack则被初始化为0？ */
static union task_union init_task = {INIT_TASK,};

/* 从开机开始算起的滴答数，每滴答10ms */
unsigned long volatile jiffies = 0;

/* 存储开机时间 */
unsigned long startup_time = 0;

/* current用来标定当前任务，内核启动期间被初始化为任务0 */
struct task_struct *current = &(init_task.task);
struct task_struct *last_task_used_math = NULL;


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

void schedule(void)
{
	int i, next, c;
	struct task_struct **p;

	/* 检测alarm（进程的报警定时值），唤醒任何已经得到信号的可中断任务 */
	for (p = &LAST_TASK; p > &FIRST_TASK; p--)
	{
		if (*p)
		{
			/* 如果设置过timeout，并且已经超时（为什么小于jiffies就是超时了呢），就复位timeout；
			 * 假设设置定时值为20ms，即当前时刻的jiffies+2，那么当过了10ms后，timeout为jiffies+2，
			 * 而当前时刻的jiffies（新）已经变成了jiffies（旧）+ 1，则timeout > jiffies，未超时，
			 * 而超时则表现为timeout（jiffies（旧）+ 2） < jiffies（新）。
			 */
			if ((*p)->timeout && (*p)->timeout < jiffies)
			{
				(*p)->timeout = 0;
				if ((*p)->state == TASK_INTERRUPTIBLE)
					(*p)->state = TASK_RUNNING;
			}

			/* 如果设置过任务的SIGALRM信号超时定时器值alarm，并且已经过期，则在信号位图中设置SIGALRM信号,
			 * 即向任务发送SIGALRM信号，然后清alarm。该信号的默认操作是终止进程。
			 */
			if ((*p)->alarm && (*p)->alarm < jiffies)
			{
				(*p)->signal |= (1<<(SIGALRM - 1));
				(*p)->alarm = 0;
			}

			/* 系统中一共有32个信号，分别编号为1-32，用一个32位的双字来表示信号位图中被阻塞的信号，即
			 * 每一位表示一种信号，为1的位表示该信号被阻塞，下面的语句表示如果信号位图中除了被阻塞的
			 * 信号外还有其他信号，且任务处于可中断状态，则置任务为就绪状态。
			 */
			if (((*p)->signal &~(_BLOCKABLE & (*p)->blocked)) && (*p)->state == TASK_INTERRUPTIBLE)
				(*p)->state = TASK_RUNNING;
		}

		/* 开始调度的主代码片 */
		while (1)
		{
			c = -1;
			next = 0;
			i = NR_TASKS;
			p = &task[NR_TASKS];
			while (--i)
			{
				/* 如果当前任务为空，则跳过 */
				if (!*p--)
					continue;

				/* counter是任务运行时间片，其值越大表明相应任务的运行时间还很多，其优先级较高，
				 * 故本if语句是将所有任务中剩余时间片最长的时间片值赋给变量c，并用next记录具有
				 * 最长时间片的任务数组号。
				 */
				if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
				{
					c = (*p)->counter;
					next = i;
				}

				/* 如果c不为0，说明此时已经记录到了所有任务中剩余时间片最长的任务，这个任务也是
				 * 下一个要调度的任务，那么就break跳出while(1)循环。
				 */
				if (c)
					break;

				for (p = &LAST_TASK; p > &FIRST_TASK; p--)
				{
					/* 根据每个任务的优先权，来更新每个任务的counter，这里的counter计算方法不太理解 */
					if (*p)
						(*p)->counter = ((*p)->counter >> 1) + (*p)->priority;
				}
			}
		}
	}

	switch_to(next);
}

int sys_pause(void)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	return 0;
}

/* 该函数把当前任务设置为可中断的或不可中断的睡眠状态，并让睡眠队列头指针指向当前任务。
 * 参数：p - 等待队列头指针
 *       state - 进程状态
 */
static inline void __sleep_on(struct task_struct **p, int state)
{
	struct task_struct *tmp;

	if (!p)
		return;

	if (current == &(init_task.task))
		panic("task[0] trying to sleep");

	/* 让tmp指向已经在等待队列上的任务（如果有的话） */
	tmp = *p;

	/* 将当睡眠队列头的指针指向当前任务 */
	*p = current;

	/* 将当前任务的状态置为state */
	current->state = state;
repeat:

	/* 当前任务current已经睡眠了，现在需要调用schedule函数来执行别的任务，
	 * schedule函数执行后，就会切换到别的进程去，然后当前进程进入可中断的
	 * 睡眠状态，等待被重新调度。
	 */
	schedule();

	/* 当本进程被重新调度之后，就会从此处开始执行。此时如果队列中还有等待的任务，
	 * 且队列头所指的任务不是本任务，那么说明在本任务睡眠之后，还有别的任务进入到
	 * 了本资源块的等待队列中，0.12内核的睡眠唤醒策略类似于出栈入栈，即最后进入
	 * 等待队列的进程最先被唤醒，这里私认为不合理，因为先进入等待队列的进程必然
	 * 等待时间更长，理应先被唤醒。
	 */
	if (*p && *p != current)
	{
		/* 等待队列头指向的进程首先被调度执行，将其状态置为就绪 */
		(**p).state = 0;

		/* 当前进程被设置为不可中断的等待状态，需要所有比本进程后
		 * 进入等待队列的进程被唤醒后才能用wake_up函数显式地唤醒。
		 */
		current->state = TASK_UNINTERRUPTIBLE;

		/* 重新回到上面去调用schedule，因为队列头指向的任务已经被置为就绪状态，
		 * 而本进程则置为不可中断的睡眠状态，因此不会被调度，这里有个问题，被
		 * 设置为不可中断睡眠模式的进程是在哪里被唤醒的呢？
		 */
		goto repeat;
	}

	/* 执行到这里，说明本进程确实被唤醒了，此时p指向的就是本任务，如为空，则报错 */
	if (!*p)
		printk("Warning: *P = NULL\n\r");

	/* 让p指向本任务内核态中的tmp，即比本任务先进入等待队列的任务，如不为空，说明
	 * 确实存在比本任务先进入等待队列的任务，将其状态置为就绪态。
	 */
	if (*p = tmp)
		tmp->state = 0;
}

void sleep_on(struct task_struct **p)
{
	__sleep_on(p, TASK_INTERRUPTIBLE);
}

void wake_up(struct task_struct ** p)
{
	if (p && *p)
	{
		if ((**p).state == TASK_STOPPED)
			printk("wake_up: TASK_STOPPED");

		if ((**p).state == TASK_ZOMBIE)
			printk("wake_up: TASK_ZOMBIE");

		(**p).state = 0;
	}
}

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
