/*
 * linux/boot/head.s
 *
 * (C) 1991 Linus Torvalds
 */
 
/*
 * head.s contain the 32-bit startup code.
 *
 * NOTE!!! Startup happens at absolute address 0x00000000, which is also where
 * the page directory will exist. The startup code will be overwritten by
 * the page directory.
 */
.text
.globl _idt,_gdt,_pg_dir,_tmp_floppy_area
_pg_dir:														# 页目录将会存放在这里，这不就是一个标号么，这是system的第一个标号，最后会落在0x0处啊！
startup_32:
				movl 	$0x10, %eax
				mov 	%ax, %ds
				mov 	%ax, %es
				mov 	%ax, %fs
				mov 	%ax, %gs
				lss		_stack_start, %esp		# 用_stack_start同时给ss和%esp寄存器赋值，其中ss高字，%esp低字
				call 	setup_idt
				call 	setup_gdt
				movl	$0x10, %eax
				movl 	%ax, %ds							# 因为已经重新加载了GDT，所以各个段寄存器要重新加载
				movl 	%ax, %es
				movl 	%ax, %fs
				movl 	%ax, %gs
				lss 	_stack_start, %esp
				xorl 	%eax, %eax						# 通过比较0x000000和0x100000内存处的值是否相同来检查地址线A20是否开启
1:			incl 	%eax
				movl 	%eax, 0x000000
				cmpl	%eax, 0x100000
				je 		1b										# 如相同则一直比较下去，即死循环。
/*
 * NOTE! 486 should set bit 16, to check for write-protect in supervisor
 * mode. Then it would be unnecessary with the "verify_area()"-calls.
 * 486 users probably want to set the NE (#5) bit also, so as to use
 * int 16 for math errors.
 */
				movl 	%cr0, %eax
				andl 	$0x80000011, %eax
/* "orl $0x10020, %eax" here for 486 might be good */
				orl 	$2, %eax
				movl	%eax, %cr0
				call 	check_x87
				jmp		after_page_table			# 跳转到135行

/*
 * We depend on ET to be correct. This checks for 287/387.
 */
check_x87:
				fninit											# 向协处理器发出初始化命令
				fstsw	%ax										# 取协处理器状态字到ax寄存器中
				cmpb	$0, %al								# 初始化后状态字应该为0，否则说明协处理器不存在。
				je		1f										# 向前跳转到标号1处，那不是回到检查A20地址线的地方去了？
				movl	%cr0, %eax						# 如存在则改写cr0
				xorl 	$6, %eax
				movl	%eax, %cr0
				ret
.align 2
1:			.byte	0xDB, 0xE4
				ret

/*
 * setup_idt
 *
 * sets up a idt with 256 entries pointing to
 * ignore_int, interrupt gates. It then loads
 * idt. Everything that wants to install itself
 * in the idt-table may do so themselves. Interrupts
 * are enabled elsewhere, when we can be relatively
 * sure everything is ok. This routine will be over-
 * written by the page tables.
 */
setup_idt:
				lea		ignore_int, %edx				# 将ignore_idt的有效地址（偏移值）放到edx寄存器
				movl 	$0x00080000, %eax				# 0x0008 ---> eax的高16位
				movw	%dx, %ax
				movw	$0x8E00, %dx						# 中断门描述符的属性

				lea		_idt, %edi							# _idt是中断描述符表的地址
				mov 	$256, %ecx
rp_sidt:
				movl 	%eax, (%edi)
				movl 	%esx, 4(%edi)
				addl 	$8, %edi
				dec		%ecx
				jne		rp_sidt
				lidt	idt_descr								# 加载中断描述符表寄存器值
				ret

/*
 * setup_gdt
 *
 * This routines sets up a new gdt and loads it.
 * Only two entries are currently built, the same
 * ones that were built in init.s. The routine
 * is VERY complicated at two whole lines, so this
 * rather long comment is certainly needed :-).
 * This routine will beoverwritten by the page tables.
 */
setup_gdt:
				lgdt gdt_descr
				ret

/*
 * I put the kernel page tables right after the page directory,
 * using 4 of them to span 16 Mb of physical memory. People with
 * more than 16MB will have to expand this.
 */
.org 0x1000
pg0:

.org 0x2000
pg1:

.org 0x3000
pg2:

.org 0x4000
pg3:

.org 0x5000														# 定义下面的代码块从偏移0x5000开始。
/*
 * tmp_floppy_area is used by the floppy-driver when DMA cannot
 * reach to a buffer-block. It needs to be aligned, so that it isn't
 * on a 64kB border.
 */
_tmp_floppy_area:
				.fill 	1024, 1, 0						# 保留1024项，每项1字节，填充数值0.

after_page_tables:
				pushl	$0											# 这段代码是为最后调用main函数做准备工作，
				pushl	$0											# main函数就可以理解为第一个任务，这在x86保护模式中有说，
				pushl	$0											# 这三个入栈的0就是main函数的三个参数：envp、argv和argc。
				pushl	$L6											# 这里模拟的是调用main函数时将返回地址入栈的操作，即main函数
				pushl	$_main									# 真的返回时，就会返回到L6这里继续执行下去，即死循环，那么是不是main就不应该返回呢？
				jmp		setup_paging						# 将main的地址入栈，则当setup_paging返回时就会弹出它，从而去执行main函数。
L6:
				jmp		L6


/* This is the default interrupt "handler" :-) */
int_msg:
				.asciz "Unknow interrupt\n\r"	# 定义字符串“未知字符串（回车换行）”
align 2
ignore_int:
				pushl %eax
				pushl %ecx
				pushl %edx
				pushl %ds
				pushl %es
				pushl %fs
				movl 	$0x10, %eax
				mov 	%ax, %ds
				mov 	%ax, %es
				mov 	%ax, %fs
				pushl	$int_msg								# $很重要，否则入栈的不会是地址，而是字符‘Unkn’
				call 	_printk									# 该函数在/kernel/printk.c中，编译过后现在就可以使用，跨文件函数调用。
				popl	%eax
				pop		%fs
				pop		%es
				pop		%ds
				popl	%edx
				popl	%ecx
				popl	%eax
				iret													# 中断返回，把中断调用时入栈的CPU标志寄存器CFLAGS值也弹出。


/*
 * Setup_paging
 *
 * This routine sets up paging by setting the page bit
 * in cr0. The page tables are set up, identity-mapping
 * the first 16MB. The pager assumes that no illegal
 * addresses are produced (ie >4Mb on a 4Mb machine).
 *
 * NOTE! Although all physical memory should be identity
 * mapped by this routine, only the kernel page functions
 * use the >1Mb addresses directly. All "normal" functions
 * use just the lower 1Mb, or the local data space, which
 * will be mapped to some other place - mm keeps track of
 * that.
 *
 * For those with more memory than 16 Mb - tough luck. I've
 * not got it, why should you :-) The source is here. Change
 * it. (Seriously - it shouldn't be too difficult. Mostly
 * change some constants etc. I left it at 16Mb, as my machine
 * even cannot be extended past that (ok, but it was cheap :-)
 * I've tried to show which constants to change by having
 * some kind of marker at them (search for "16Mb"), but I
 * won't guarantee that's all :-( )
 */
.align 2
setup_paging:
				movl	$1024*5, %ecx
				xorl	%eax, %eax
				xorl	%edi, %edi
				cld;rep;stosl										# eax内容存放到es:edi所指内存位置处，且edi增4？
				movl 	$pg0+7, pg_dir
				movl 	$pg1+7, pg_dir+4
				movl 	$pg2+7, pg_dir+8
				movl 	$pg3+7, pg_dir+12
				movl	$pg3+4092, %edi
				movl	$0xfff007, %eax
				std															# 方向位置位，edi值递减（4字节）
1:			stosl														# 将eax寄存器中的内容转存到es:edi中，方向位置位则递增，复位递减
				subl	$0x1000, %eax
				jge		1b
				xorl	%eax, %eax
				movl	%eax, %cr3
				movl	%cr0, %eax
				orl		$0x80000000, %eax
				movl 	%eax, %cr0								# 将cr0的PG位置位，开启分页机制。前面应该还有一个开启分段标志的PE位我记着。
				ret

.align 2
.word 0
idt_descr:
				.word 	256*8-1									# 中断表限长
				.long		_idt										# 中断表（汇编/偏移）地址
.align 2
.word 0
gdt_descr:
				.word		256*8-1
				.long		_gdt
				
				.align 	3
_idt:		.fill	256, 8, 0

_gdt:		.quad	0x0000000000000000
				.quad 0x00c09a0000000fff				# 0x08，内核代码段最大长度16MB
				.quad 0x00c0920000000fff				# 0x10，内和数据段最大长度16MB
				.quad	0x0000000000000000
				.fill 252, 8, 0									# 想知道选择符0x10和0x17对应的描述符是在哪里设置的？