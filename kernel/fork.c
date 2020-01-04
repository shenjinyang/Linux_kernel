#include <errno.h>
#include <linux/sched.h>
#include <linux/kernel>
#include <asm/system.h>

long last_pid = 0;

/* 复制内存页表，参数nr是新任务号，p是新任务数据结构指针
 * 该函数为新任务在线性地址空间中设置代码段和数据段基址、段限长，并复制页表。
 * 由于Linux系统使用了写时复制技术，所以这里只为新进程设置自己的页目录表项和
 * 页表项，并没有为新进程分配实际物理内存页面，此时新进程与其父进程共享所有的
 * 内存页面，所以在复制页目录表和页表之后要将所有共享页设置为只读。
 * 操作成功返回0，否则返回出错号。
 */
int copy_mem(int nr, struct task_struct * p)
{
	unsigned long old_data_base, new_data_base, data_limit;
	unsigned long old_code_base, new_code_base, code_limit;

	/* 0x0f是任务0的代码段段选择符，取当前进程代码段和数据段的段限长 */
	code_limit = get_limit(0x0f);
	data_limit = get_limit(0x17);

	/* 根据段描述符取当前进程代码段和数据段的基地址 */
	old_code_base = get_base(current->ldt[1]);
	old_data_base = get_base(current->ldt[2]);

	/* 0.12版内核不支持代码段和数据段分立的情况，所以要求代码段和数据段基地址都相同，
	 * 且数据段长度至少不小于代码段的长度。
	 */
	if (old_data_base != old_code_base)
		panic("We don't support separate I&D");
	if (data_limit < code_limit)
		panic("Bad data_limit");

	/* 计算新任务的起始虚拟地址 */
	new_data_base = new_code_base = nr * TASK_SIZE;
	p->start_code = new_code_base;

	/* 设置新任务ldt表中的代码段和数据段描述符中的基地址字段；
	 * 注意这里没有调用set_limit来设置新任务的段限长，是因为
	 * 每个任务的段限长都是一样的，直接使用父进程中的就可以了,
	 * 不需要修改。
	 */
	set_base(p->ldt[1], new_code_base);
	set_base(p->ldt[2], new_data_base);
	if (copy_page_tables(old_data_base, new_data_base, data_limit))
	{
		free_page_tables(new_data_base, data_limit);
		return -ENOMEM;
	}
	return 0;
}

/* 复制进程信息，一个进程需要很多参数标定，所以这个函数有很多入参。
 * 首先要知道，这个函数试运行在内核态的，它是由系统调用函数system_call
 * 发起的调用链中的一环，而system_call就是系统中断0x80的中断处理函数，
 * 所以该函数使用的是内核态堆栈。下面这些参数分别都是从system_call开始
 * 逐一push到任务0的内核态堆栈中的。
 * ---------------------------------------------------------------
 * ss/esp/eflags/cs/eip分别是CPU执行中断指令压入的用户栈地址、标志和返回地址
 * ---------------------------------------------------------------
 * ds/es/fs/edx/ecx/ebx是由system_call一开始push到内核栈的；
 * ---------------------------------------------------------------
 * none是system_call中调用sys_fork时入栈的返回地址
 * ---------------------------------------------------------------
 * gs/esi/edi/ebp/eax(nr)是sys_fork调用copy_process之前入栈的，nr是调用find_empty_process分配的任务数组项号。
 */
int copy_process(int nr, long ebp, long edi, long esi, long gs, long none,
		long ebx, long ecx, long edx,
		long fs, long es, long ds,
		long eip, long cs, long eflags, long esp, long ss)
{
	struct task_struct *p;
	int i;
	struct file *f;

	p = (struct task_struct *) get_free_page();
	if (!p)						// 在任务0创建任务1的过程中，物理内存页面大部分都还没有被占用，
		return -EAGAIN;			// 所以一定能找到一个空闲的物理页面的。
	task[nr] = p;
	*p = *current;				// 注意，因为只是将union中的task成员复制给了*p，所以这样做不会
								// 复制超级用户堆栈（内核态堆栈），只复制进程结构。

	/* 下面对复制来的进程结构内容做一些修改，作为新进程的任务结构 */
	p->state	= TASK_UNINTERRUPTIBLE;
	p->pid		= last_pid;
	p->counter	= p->priority;	// 运行时间片，任务0的priority和counter都被设置为15
	p->signal	= 0;
	p->alarm	= 0;
	p->leader	= 0;			// 进程的领导权是不能继承的，领导权是什么鬼
	p->utime = p->stime = 0;	// 创建进程的过程中，进程运行时间都是0
	p->cutime = p->cstime = 0; 
	p->start_time = jiffies;	// 留意jiffies是怎么更新的
	p->tss.back_link = 0;
	p->tss.esp0 = PAGE_SIZE + (long)p;	/* 栈顶指针指向被创建任务数据结构所在页的顶端 */
	p->tss.ss0 = 0x10;					/* ss0被赋值为内核数据段选择子 */
	p->tss.eip = eip;					/* 指令代码指针 */
	p->tss.eflags = eflags;
	p->tss.eax = 0;						/* 这是当fork返回时新进程会返回0的原因所在 */
	p->tss.ecx = ecx;
	p->tss.edx = edx;
	p->tss.ebx = ebx;
	p->tss.esp = esp;
	p->tss.ebp = ebp;
	p->tss.esi = esi;
	p->tss.edi = edi;
	p->tss.es = es & 0xffff;			/* 段寄存器只有16位有效 */
	p->tss.cs = cs & 0xffff;
	p->tss.ss = ss & 0xffff;
	p->tss.ds = ds & 0xffff;
	p->tss.fs = fs & 0xffff;
	p->tss.gs = gs & 0xffff;
	p->tss.ldt = _LDT(nr);				/* 本任务LDT段的段选择符 */
	p->tss.trace_bitmap = 0x80000000;

	/* 这个先放在这里，有点绕 */
	if (last_task_used_math == current)
		__asm__("clts; fnsave %0; frstor %0"::"m"(p->tss.i387));

	/* 复制进程页表，成功则返回0，否则就释放新任务数据结构所在的物理页面 */
	if (copy_mem(nr, p))
	{
		task[nr] = NULL;
		free_page((long)p);
		return -EAGAIN;
	}

	/* 因为新创建的子进程与父进程共享打开这的文件，所以父进程若有打开着的文件，则需要将
	 * 对应文件的打开次数增1.同样道理需要把当前进程（父进程）的pwd、root、executable这些
	 * i节点的引用次数增加1。
	 */
	for (i = 0; i < NR_OPEN; i++)
	{
		if (f = p->filp[i])
			f->f_count++;
	}

	if (current->pwd)
		current->pwd->i_count++;
	if (current->root)
		current->root->i_count++;
	if (current->executable)
		current->executable->i_count++;
	if (current->library)
		current->library->i_count++;

	/* 在GDT表中设置新任务TSS段和LDT段描述符项。因为每个任务在GDT表中占两项，
	 * 所以第一个参数（gdt表中进程nr的TSS/LDT段描述符的首字节）有nr<<1；
	 */
	set_tss_desc(gdt+(nr<<1)+FIRST_TSS_ENTRY, &(p->tss));
	set_ldt_desc(gdt+(nr<<1)+FIRST_LDT_ENTRY, &(p->ldt));

	p->p_pptr = current;			/* 设置新进程的父进程指针指向当前指针current */
	p->p_cptr = 0;					/* 复位新进程的最新子进程指针 */
	p->p_ysptr = 0;					/* 复位新进程的比邻年轻兄弟进程指针 */
	p->p_osptr = current->p_cptr;	/* 设置新进程的比邻年老兄弟进程指针指向当前进程的最新进程 */
	if (p->p_osptr)
		p->p_osptr->p_ysptr = p;
	current->p_cptr = p;
	p->state = TASK_RUNNING;
	return last_pid;
}

/* 该函数由kernel/system_call.s中的sys_call汇编函数调用，但是在sys_call中没有找到
 * 使用extern关键字声明，在工程的其他文件中也没有找到该函数的声明语句，很奇怪。
 * 该函数的主要作用是为新进程取得不重复的进程号last_pid，函数返回在任务组中的任务号（数组项）。
 */
int find_empty_process(void)
{
	int i;

	repeat:
		/* 如果last_pid增加1后超出进程号的正数范围，则从1开始使用pid号 */
		if ((++last_pid)<0) last_pid = 1;
		/* 在任务数组中搜索刚设置的pid号是否已经被任何任务使用，如果是则跳回函数开始处重新获得pid号 */
		for (i = 0; i < NR_TASKS; i++)
			if (task[i] && task[i]->pid == last_pid) goto repeat;
		/* 在任务数组中为新任务找寻一个空闲项，并返回项号（存在eax中），注意需要排除任务0 */
		for (i = 1; i < NR_TASKS; i++)
			if (!task[i])
				return i;
		return -EAGAIN;
}
