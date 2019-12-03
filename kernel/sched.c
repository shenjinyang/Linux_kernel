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

void sched_init(void)
{
	int i;
	struct desc_struct * p;

	if (sizeof(struct sigaction) != 16)
		panic("Struct sigaction MUST be 16 bytes");
	set_tss_desc(gdt+FIRST_TSS_ENTRY, &(init_task.task.tss));	// 设置任务0的tss段描述符，参数gdt实际上是在boot/head.s中定义的
	set_ldt_desc(gdt+FIRST_LDT_ENTRY,&(init_task.task.ldt));	// 设置任务0的ldt段描述符
}
