/* 如果说“sched_init”函数的作用是构造任务0的各种条件，那么“move_to_user_mode”就是通过
 * 使用“sched_init”函数所构造的条件来启动任务0，而这个工作最主要的实际上就是改变进程的特权级别，从0改到3 
 * -----------------------------------------------------------------
 * movl %%esp, %%eax：
 * 将当前栈顶指针esp的内容赋给eap，当前栈顶指针是哪个？就是在head.s中最开始为系统设置的一个堆栈，
 * 从内核启动代码执行到现在，内和使用的都一直是这个堆栈空间。具体语句是“lss	_stack_start, %esp”，语句中的“_stack_start”
 * 是在文件“\kernel\sched.c”中定义的一个页大小的内存，也即是说内核系统的初始栈空间大小是4096字节，这个变量跟之前提到的
 * gdt变量相似，我还是不清楚编译过程中是怎么让其与“sched.c”中的“stack_start”联系在一起的。
 * -----------------------------------------------------------------
 * pushl $0x17：将0x17压栈，后面会看到这里压栈的0x17就是任务0的用户栈段选择符
 * -----------------------------------------------------------------
 * pushl %%eax：将第一行保存的栈顶指针压栈；
 * -----------------------------------------------------------------
 * pushfl：将标志寄存器压栈；
 * -----------------------------------------------------------------
 * pushl $0x0f：将0x0f压栈，后面会看到这里的0xf就是任务0对应的CS；
 * -----------------------------------------------------------------
 * pushl $1f：将指令1f的偏移压栈（它是iret的下一条指令）。
 * -----------------------------------------------------------------
 * *****************************************************************
 * -----------------------------------------------------------------
 * 上面一共压栈了5个参数，光看这些压栈指令也许不容易看出代码片的意图，但是紧接着的iret指令则说明了一切，因为iret指令会相应弹出以下值：
 * 第一个pop出来的是eip：就是标号1的指令的偏移，即iret返回的下一条指令的偏移；
 * -----------------------------------------------------------------
 * 第二个pop出来的是cs：为0xf，这个就是任务0的LDT表中的第二项，也就是代码段的段选择子；
 * -----------------------------------------------------------------
 * 第三个pop出来的是eflags：没什么好说的；
 * -----------------------------------------------------------------
 * 第四个pop出来的是esp：esp就是栈顶指针，这个要尤其注意，这个esp表示的就是系统最开始使用的系统堆栈空间，
 * 其大小为4096字节，地址在编译时确定（我认为的）；
 * -----------------------------------------------------------------
 * 第五个pop出来的是ss，上一条准备好了esp，这一条设置对应的ss，这里的ss是0x17，也就是任务0的数据段选择符，
 * 前面说到，内核最开始使用的系统堆栈，其段选择符对应内核数据段，为0x10，其对应的内核数据段描述符中的基地址是0x0；
 * 而0x17是任务0的数据段选择符，其对应的任务0数据段描述符中的段基地址同样也是0x0，那么就是说，实际上只是段选择符变了，
 * 但是堆栈的地址和大小都没变，只是换了一种描述方法，将内核的初始栈转变成了任务0的用户栈。但是要注意一点，
 * 内核数据段的段限长为16MB，而任务0的段限长只有640KB，也就是说，要想使得内核的初始系统堆栈能够“平移”成为任务0的用户态堆栈，
 * 该堆栈的地址必须存在于二者内存重叠的部分，即线性地址0~640KB之间，这点很重要。
 */
#define move_to_user_mode() \
__asm__ ("movl %%esp,%%eax\n\t" \
	"pushl $0x17\n\t" \				// 这里有push操作，说明此时已经有堆栈了（这是一定的），那么这个堆栈是什么时候设置的呢？
	"pushl %%eax\n\t" \				// 我现在知道的是在bootsect.S中将堆栈设置到了物理内存0x9000:0xfef4处，这个是引导启动程序的第二个栈，
	"pushfl\n\t" \					// 第一个栈是当bootsect.S被BIOS加载到0x7c00处时设置的，为物理地址0x0000:0xfffe。
	"pushl $0x0f\n\t" \				// 回到刚开始的问题，这里的堆栈是什么时候设置的？应该就是head.s里第23行设置的堆栈：lss _stack_start,%esp
	"pushl $1f\n\t" \				// 这个栈空间先不管是在哪里定义的，只需要知道，这个是内核最开始的栈，姑且叫做系统栈，就是现在push所使用的
	"iret\n" \						// 堆栈；
	"1:\tmovl $0x17,%%eax\n\t" \
	"movw %%ax,%%ds\n\t" \
	"movw %%ax,%%es\n\t" \
	"movw %%ax,%%fs\n\t" \
	"movw %%ax,%%gs" \
	:::"ax")

/* 开中断 */
#define sti() __asm__ ("sti"::)

/* 关中断 */
#define cli() __asm__ ("cli"::)

/* 设置门描述符宏
 * 根据参数中的中断或异常处理程序地址addr、门描述符类型type和特权级别dpl，设置位于gate_addr处的门描述符；
 * 参数：gate_addr：门描述符地址；type：门描述符类型；dlp：门描述符特权级别；addr：处理过程偏移地址注意此处的
 * 偏移地址都是针对内核代码段或内和数据段来说的。
 * %0 - （由dpl、type组成的类型标志字）；%1 - （描述符低4字节地址）；%2 - （描述符高4字节地址）；
 * %3 - eax（程序偏移地址addr）；%4 - edx（高字中含有段选择符）
 */
#define _set_gate(gate_addr, type, dpl, addr) \
__asm__ ("movw %%dx, %%ax\n\t" \
		"movw %0, %%dx\n\t" \
		"movl %%eax, %1\n\t" \
		"movl %%edx, %2" \
		: \
		: "i" ((short) (0x8000+(dpl<<13) +(type<<8))), \
		"o" (*((char *) (gate_addr))), \
		"o" (*(4+(char *) (gate_addr))), \
		"d" ((char *) (addr)), "a" (0x00080000))

/* 设置中断门，中断门与陷阱门之间的区别在于对EFLAGS的中断允许标志位IF的影响。
 * 通过中断门描述符执行的中断会复位IF标志，因此可以避免其他中断干扰当前中断处理，
 * 并且随后的中断结束指令IRET会从堆栈上恢复对IF标志的原值；陷阱门不会影响IF标志。
 * 不同之处在代码实现中体现在宏定义的第2个参数。
 */
#define set_intr_gate(n, addr) \
	_set_gate(&idt[n], 14, 0, addr)

#define set_trap_gate(n, addr) \
	_set_gate(&idt[n], 15, 0, addr)

#define set_system_gate(n, addr) \
	_set_gate(idt[n], 15, 3, addr)

/* 构造tss或ldt的段描述符。
 * -----------------------------------
 * 第0个参数："a" (addr)，将addr即&(init_task.task.tss)给eax；
 * 第1个参数："m" (*(n))，一个内存数，将n的值也就是gdt+4这个数作为一个内存地址，其就代表gdt中进程0的TSS描述符的首字节，
 *             第0至第1字节（16位）存放这个段的段限长；
 * 第2个参数："m" (*(n+2))，一个内存数，gdt中进程0的TSS描述符的第2个字节。第2-3字节放着这个基地址的低16位；
 * 第3个参数："m" (*(n+4))，进程0的tss段描述的第4个字节。这里面放的基地址的中8位。
 * 第4个参数："m" (*(n+5))，进程0的tss段描述符的第5个字节，里面放的类型。
 * 第5个参数："m" (*(n+6))，进程0的tss段描述符的第6个字节，里面放的段限长，粒度啥的。
 * 第6个参数："m" (*(n+7))，进程0的tss段描述符的第7个字节，里面放的基地址的高8位。
 */
#define _set_tssldt_desc(n, addr, type) \
__asm__ ("movw $104,%1\n\t" \				// 将TSS（或LDT）长度（段限长）放入描述符长度域（第0~1字节）
	"movw %%ax,%2\n\t" \					// 将基地址的低字放入描述符低2-3字节。
	"rorl $16,%%eax\n\t" \					// 将基地址高字循环右移到描述符的第四字节
	"movb %%al,%3\n\t" \					// 我觉得这个算法暂时能看懂就可以，先不用仔细研究，后面也会忘。
	"movb $" type ",%4\n\t" \
	"movb $0x00,%5\n\t" \
	"movb %%ah,%6\n\t" \
	"rorl $16,%%eax" \
	::"a" (addr), "m" (*(n)), "m" (*(n+2)), "m" (*(n+4)), \
	 "m" (*(n+5)), "m" (*(n+6)), "m" (*(n+7)) \
	)

/* 这里再次做一次宏定义，是因为要将参数n和addr做强值类型转换，并加上段属性值。
 * ------------------------------------------------------------------
 * 参数n：n = gdt+FIRST_TSS_ENTRY，表示TSS该描述符是全局描述符表除内核四个段之外的第一个TSS段描述符，
 * 其被强制转换乘char类型的指针以表示地址，之所以这样做是因为gdt在编译后是一个数（可能是整型？），但
 * 是这个数的实际含义是一个地址，为了能在后面展开的内联函数中将其作为指针（地址）来操作，必须首先将其
 * 强制转换为指针类型；
 * ------------------------------------------------------------------
 * 参数addr：addr = &(init_task.task.tss)，表示任务0的tss数据结构的地址，其被强制转换成为int型，这样
 * 做的原因是addr虽然是个地址，但是它在后面展开的内联函数中是要被当做立即数去操作进而构造TSS段描述符。
 */
#define set_tss_desc(n, addr) _set_tssldt_desc(((char *) (n)), ((int)(addr)), "0x89")
#define set_ldt_desc(n, addr) _set_tssldt_desc(((char *) (n)), ((int)(addr)), "0x82")
