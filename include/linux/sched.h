extern void trap_init(void);

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
	/* 任务0的ldt成员：局部描述符表，第0项-空，第1项-代码段，第2项-数据段 */
	struct desc_struct ldt[3];
	/* 任务0的tss成员 */
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
   * 其次是es/cs/ss/ds/fs/gs 6个寄存器都被初始化为0x17，即任务0的代码段和数据段的段选择符都是0x17，因为
   * 全局段描述符表gdt的前四项被内核占用，所以任务0的
   */
  {0,PAGE_SIZE+(long)&init_task,0x10,0,0,0,0,(long)&pg_dir,\
   0,0,0,0,0,0,0,0, \
   0,0,0x17,0x17,0x17,0x17,0x17,0x17, \
   _LDT(0),0x80000000, \
   {} \
  }, \
}

#define FIRST_TSS_ENTRY 4
#define FIRST_LDT_ENTRY (FIRST_TSS_ENTRY + 1)
