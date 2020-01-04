#ifndef _SCHED_H
#define _SCHED_H

/* linux内核所支持的最大任务数 */
#define NR_TASKS 64
/* 每个任务的长度，为64MB */
#define TASK_SIZE 0x04000000

/* 定义系统时钟滴答频率（100赫兹，每个滴答10ms） */
#define HZ 100

#define FIRST_TASK task[0]
#define LAST_TASK task[NR_TASKS - 1]

#include <linux/head.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <signal.h>

/* 定义进程的各种状态 */
#define TASK_RUNNING				0
#define TASK_INTERRUPTIBLE			1
#define TASK_UNINTERRUPTIBLE		2
#define TASK_ZOMBIE					3
#define TASK_STOPPED				4

extern int copy_page_tables(unsigned long from, unsigned long to, long size);
extern int free_page_tables(unsigned long from, unsigned long size);

extern void sched_init(void);
extern void schedule(void);
extern void trap_init(void);
extern void panic(const char * str);

/* 暂时不知道这个是干啥的 */
struct i387_struct {
};

/* 这个结构体对应TSS数据结构，即每个任务特有的数据结构，记录了任务用户栈、内核栈等数据 */
struct tss_struct {
	long	back_link;	/* 16 high bits zero */
	long	esp0;
	long	ss0;		/* 16 high bits zero */
	long	esp1;
	long	ss1;		/* 16 high bits zero */
	long	esp2;
	long	ss2;		/* 16 high bits zero */
	long	cr3;
	long	eip;
	long	eflags;
	long	eax,ecx,edx,ebx;
	long	esp;
	long	ebp;
	long	esi;
	long	edi;
	long	es;		/* 16 high bits zero */
	long	cs;		/* 16 high bits zero */
	long	ss;		/* 16 high bits zero */
	long	ds;		/* 16 high bits zero */
	long	fs;		/* 16 high bits zero */
	long	gs;		/* 16 high bits zero */
	long	ldt;		/* 16 high bits zero */
	long	trace_bitmap;	/* bits: trace 0, bitmap 16-31 */
	struct i387_struct i387;
};

struct task_struct {
/* these are hardcoded - don't touch */
	long state;	/* -1 unrunnable, 0 runnable, >0 stopped */
	long counter;
	long priority;
	long signal;
	struct sigaction sigaction[32];
	long blocked;	/* bitmap of masked signals */
/* various fields */
	int exit_code;
	unsigned long start_code,end_code,end_data,brk,start_stack;
	long pid,pgrp,session,leader;
	int	groups[NGROUPS];
	/* 
	 * pointers to parent process, youngest child, younger sibling,
	 * older sibling, respectively.  (p->father can be replaced with 
	 * p->p_pptr->pid)
	 */
	struct task_struct	*p_pptr, *p_cptr, *p_ysptr, *p_osptr;
	unsigned short uid,euid,suid;
	unsigned short gid,egid,sgid;
	unsigned long timeout,alarm;
	long utime,stime,cutime,cstime,start_time;
	struct rlimit rlim[RLIM_NLIMITS]; 
	unsigned int flags;	/* per process flags, defined below */
	unsigned short used_math;
/* file system info */
	int tty;		/* -1 if no tty, so it must be signed */
	unsigned short umask;
	struct m_inode * pwd;
	struct m_inode * root;
	struct m_inode * executable;
	struct m_inode * library;
	unsigned long close_on_exec;
	struct file * filp[NR_OPEN];
/* ldt for this task 0 - zero 1 - cs 2 - ds&ss */
	struct desc_struct ldt[3];
/* tss for this task */
	struct tss_struct tss;
};


/* 对于任务0来说，它的任务数据是在sched.h中写死的，被定义为宏定义INIT_TASK */
#define INIT_TASK \
{ 0,15,15, \					/* state etc */	
  0,{{},},0, \					/* signals */
  0,0,0,0,0,0, \				/* ec,brk... */
  0,-1,0,0,0, \					/* pid etc.. */
  0,0,0,0,0,0, \				/* uid etc */
  0,0,0,0,0,0, \				/* alarm */
  0, \							/* math */
  -1,0022,NULL,NULL,NULL,0, \	/* fs info */
  {NULL,}, \					/* filp */

  /* 任务0的ldt表，其中，
   * LDT1为任务0的代码段描述符，二进制形式为： 
   * 63-48：0000 0000 1100 0000 	（段限长高4位 + 属性值4位 + 基地址高8位）
   * 47-32：1111 1010 0000 0000	（基地址中8位 + 属性值8位）
   * 31-16：0000 0000 0000 0000	（基地址低16位）
   * 15-00：0000 0000 1001 1111	（段限长低16位）
   * ------------------------------------------------------
   * LDT2为任务0的数据段描述符，二进制形式为：
   * 63-48：0000 0000 1100 0000
   * 47-32：1111 0010 0000 0000
   * 37-16：0000 0000 0000 0000
   * 15-00：0000 0000 1001 1111
   * ------------------------------------------------------
   * 可以从中得到：
   * 1：LDT1和LDT2所指定的任务0代码段和数据段的段基地址相同，都为0x0；
   * 2：任务0的代码段和数据段段限长相同，都是640KB。
   */
  { \
	{0,0}, \					/* ldt[0] */
	{0x9f,0xc0fa00}, \			/* ldt[1] */
	{0x9f,0xc0f200}, \			/* ldt[2] */
  }, \

  /* 任务0的tss数据结构体
   * 当赋值给init_task.task.tss之后，具体为：
   * -------------------------------------------------------------
   * struct tss_struct {
   *   long    back_link;	16 high bits zero  
   *   long    esp0;				   			// PAGE_SIZE+(long)&init_task
   *   long    ss0; 	  	16 high bits zero   // 0x10
   *   long    esp1;				   			// 0
   *   long    ss1; 	   	16 high bits zero   // 0
   *   long    esp2;							// 0
   *   long    ss2; 	    16 high bits zero  	// 0
   *   long    cr3; 				   			// (long)&pg_dir
   *   long    eip; 				   			// 0
   *   long    eflags;							// 0
   *   long    eax,ecx,edx,ebx; 				// 0, 0, 0, 0
   *   long    esp; 				   			// 0
   *   long    ebp; 				   			// 0
   *   long    esi; 				   			// 0
   *   long    edi; 				   			// 0
   *   long    es;	    16 high bits zero 	   	// 0x17
   *   long    cs;	    16 high bits zero 		// 0x17
   *   long    ss;	    16 high bits zero 		// 0x17
   *   long    ds;	    16 high bits zero 		// 0x17
   *   long    fs;	    16 high bits zero 		// 0x17
   *   long    gs;	    16 high bits zero 		// 0x17
   *   long    ldt;     16 high bits zero 		// _LDT(0)，LDT的段选择子
   *   long    trace_bitmap;    bits: trace 0, bitmap 16-31 	   // 0x80000000
   *   struct i387_struct i387;
   * };
   * ----------------------------------------------------------
   * 其中重点注意ss0的值为0x10，该值为任务0的内核态堆栈段选择符，0x10就是内核的数据段选择符，
   * 我理解的是因为进程通过内核系统调用进入到内核态之后，就需要使用内核态堆栈，那么堆栈的段
   * 选择符就需要定位到内核数据段。而esp0倍初始化为“PAGE_SIZE+(long)&init_task”，这里的PAGE_SIZE
   * 在文件include/linux/mm.h中被定义为4096，即一页内存大小；而“&init_task”则正是任务0的tss数据
   * 结构所在的一页大小的内存初始位置，因此二者相加就将esp0定位到了任务0数据结构所在内存页面的
   * 顶端。
   * 还有很重要的一点要注意：任务0（实际上所有任务都适用）内核栈栈顶指针esp0指向了任务0数据结构
   * 所在内存页面的顶端，当在任务0进入到内核态后，就开始使用内核栈，那么如果有函数调用，就会使用
   * 内核栈，内核栈的大小理论上只有不足4KB大小，可以说是非常小的，实际上 = 4KB - sizeof(task_struct),
   * 如果进程在内核态使用诸如递归等需要大量使用栈空间的操作，就可能会导致内核栈溢出，从而覆盖进程状态
   * 信息从而导致系统崩溃。
   * ----------------------------------------------------------
   * 其次是es/cs/ss/ds/fs/gs 6个寄存器都被初始化为0x17，即任务0的代码段和数据段的段选择符都是0x17，
   * 二进制为0000 0000 0001 0111，索引值为2，正好定位到ldt[1]，即代码段描述符（代码段和数据段基地址、
   * 段限长都相同）。
   * ----------------------------------------------------------
   * 宏定义_LDT(0)是任务0的ldt段描述符在全局描述符表中的偏移值，也就是ldt0的段选择子
   */
  {0,PAGE_SIZE+(long)&init_task,0x10,0,0,0,0,(long)&pg_dir,\
   0,0,0,0,0,0,0,0, \
   0,0,0x17,0x17,0x17,0x17,0x17,0x17, \
   _LDT(0),0x80000000, \
   {} \
  }, \
}

extern struct task_struct *task[NR_TASKS];			// 任务指针数组
extern struct task_struct *last_task_used_math;		// 是一个用过协处理器的进程
extern struct task_struct *current;					// 当前运行进程结构指针变量
extern unsigned long volatile jiffies;				// 从开机开始算起的滴答数（每滴答10ms）

extern void sleep_on(struct task_struct ** p);
extern void wake_up(struct task_struct **p);

/* FIRST_TSS_ENTRY表示任务0的tss段描述符在gdt中的索引，因为前四项被内核使用，所以从gdt[4]开始 
 * 同理ldt的段描述符被存储在gdt[5]。
 */
#define FIRST_TSS_ENTRY 4
#define FIRST_LDT_ENTRY (FIRST_TSS_ENTRY + 1)

/* FIRST_TSS_ENTRY<<3是指前4个内核段的段描述符使用了一共32个字节，而(unsigned long) n)<<4是因为每个进程都会有一个LDT和TSS，
 * 即每个进程会使用掉8*2=16个字节，总结就是n*16+32即为第n个进程的TSS描述符在GDT表中的偏移地址；ldt0也是一样的分析。
 */
#define _TSS(n) ((((unsigned long) n)<<4)+(FIRST_TSS_ENTRY))
#define _LDT(n) ((((unsigned long) n)<<4)+(FIRST_LDT_ENTRY))

#define ltr(n) __asm__("ltr %%ax"::"a" (_TSS(n)))
#define lldt(n) __asm__("lldt %%ax"::"a" (_LDT(n)))

/* 该宏定义作用是将切换任务到n中。
 * 输出：空
 * 输入：%0 - 指向tmp结构体；
 * 		 %1 - 指向tmp结构体中的b；
 *       %2 - dx，新任务你的TSS段选择子；
 * 		 %3 - ecx， 新任务的任务结构体指针
 * 这里要注意的是"ljmp %0\n\t"，执行该行后，CPU资源就会分给被切换的进程，
 * 注意改行并不是switch_to的最后一行，其后还有语句，那么也就是说，在执行
 * 该行后，调用switch_to函数的进程被切换出CPU停止执行，一旦它被重新调度，
 * 它就会从紧接着的"cmpl"语句开始执行，然后退出switch_to函数。
 */
#define switch_to(n) {\
struct {long a, b} __tmp;
__asm__("cmpl %%ecx, _current\n\t" \
		"je 1f\n\t" \
		"movw %%dx, %1\n\t" \
		"xchgl %%ecx, _current\n\t" \
		"ljmp %0\n\t" \
		"cmpl %%ecx, _last_task_used_math\n\t" \
		"jne 1f\n\t" \
		"clts \n" \
		"1:" \
		: \
		:"m"(*&__tmp.a), "m"(*&__tmp.b), \
		 "d"(_TSS(n)), "c"((long) task[n])); \
		 }

/* 设置位于地址addr处描述符中的段基地址字段（段基地址是base）
 * 没有输出寄存器
 * 输入寄存器：%0：地址addr偏移2处
 *             %1：地址addr偏移4处
 *             %2：地址addr偏移7处
 *             %3：edx - 段基地址base
 */
#define _set_base(addr,base) \
__asm__("movw %%dx,%0\n\t" \
	"rorl $16,%%edx\n\t" \
	"movb %%dl,%1\n\t" \
	"movb %%dh,%2" \
	::"m" (*((addr)+2)), \
	  "m" (*((addr)+4)), \
	  "m" (*((addr)+7)), \
	  "d" (base) \
	:"dx")

/* 设置位于地址addr处描述符中的段限长字段（段限长是limit）
 * 没有输出寄存器
 * 输入寄存器：%0：地址addr
 *             %1：地址addr偏移6处
 *             %2：edx - 段限长值
 */
#define _set_limit(addr, limit) \
__asm__("movw %%dx, %0\n\t" \
		 "rorl $16, %%edx\n\t" \
		 "movb %1, %%dh\n\t" \
		 "andb $0xf0, %%dh\n\t" \
		 "orb %%dh, %%dl\n\t" \
		 "movb %%dl, %1" \
		: \
		:"m" (*(addr)), \
		 "m" (*((addr) + 6)), \
		 "d" (limit) \
		:"dx")


/* _set_limit的第二个参数右移12位是因为段描述符中的G位被置位，表明粒度为4KB */
#define set_base(ldt, base) _set_base( ((char *)&(ldt)), base )
#define set_limit(ldt, limit) _set_limit( ((char *)&(ldt)), (limit-1)>>12 )

/* 从地址addr处取得段基地址，与set_base功能相反
 * 输出EDX：            %0 - 存放基地址
 * 输入为内存，其中：%1 - 地址addr偏移2
 *                   %2 - 地址addr偏移4
 *                   %3 - 地址addr偏移7
 */
#define _get_base(addr) ({\
unsigned long __base; \
__asm__("movb %3, %%dh\n\t" \
		"movb %2, %%dl\n\t" \
		"shll $16, %%edx\n\t" \
		"movw %1, %%dx" \
		:"=d" (__base) \
		:"m" (*((addr) + 2)), \
		 "m" (*((addr) + 4)), \
		 "m" (*((addr) + 7))); \
		 __base;})

#define get_base(ldt) _get_base( ((char *)&(ldt)) )

/* 取段选择符segment指定的描述符中的段限长值：
 * lsl指令是Load Segment Limit的缩写，它从指定段描述符中取出分散的段限长比特位
 * 并拼成完整的段限长值放在指定的寄存器中，所得的段限长是实际字节数减1，所以
 * 这里还要加1后才返回。
 * %0：存放段限长值（字节数）
 * %1：段选择符segment
 * 语句中的r是指使用动态分配的寄存器
 */
#define get_limit(segment) ({ \
unsigned long __limit; \
__asm__("lsll %1, %0\n\t" \
		"incl %0" \
		:"=r" (__limit) \
		:"r"(segment)); \
		__limit;})

#endif
