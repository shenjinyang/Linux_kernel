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
_pg_dir:														# ҳĿ¼������������ⲻ����һ�����ô������system�ĵ�һ����ţ���������0x0������
startup_32:
				movl 	$0x10, %eax
				mov 	%ax, %ds
				mov 	%ax, %es
				mov 	%ax, %fs
				mov 	%ax, %gs
				lss		_stack_start, %esp		# ��_stack_startͬʱ��ss��%esp�Ĵ�����ֵ������ss���֣�%esp����
				call 	setup_idt
				call 	setup_gdt
				movl	$0x10, %eax
				movl 	%ax, %ds							# ��Ϊ�Ѿ����¼�����GDT�����Ը����μĴ���Ҫ���¼���
				movl 	%ax, %es
				movl 	%ax, %fs
				movl 	%ax, %gs
				lss 	_stack_start, %esp
				xorl 	%eax, %eax						# ͨ���Ƚ�0x000000��0x100000�ڴ洦��ֵ�Ƿ���ͬ������ַ��A20�Ƿ���
1:			incl 	%eax
				movl 	%eax, 0x000000
				cmpl	%eax, 0x100000
				je 		1b										# ����ͬ��һֱ�Ƚ���ȥ������ѭ����
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
				jmp		after_page_table			# ��ת��135��

/*
 * We depend on ET to be correct. This checks for 287/387.
 */
check_x87:
				fninit											# ��Э������������ʼ������
				fstsw	%ax										# ȡЭ������״̬�ֵ�ax�Ĵ�����
				cmpb	$0, %al								# ��ʼ����״̬��Ӧ��Ϊ0������˵��Э�����������ڡ�
				je		1f										# ��ǰ��ת�����1�����ǲ��ǻص����A20��ַ�ߵĵط�ȥ�ˣ�
				movl	%cr0, %eax						# ��������дcr0
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
				lea		ignore_int, %edx				# ��ignore_idt����Ч��ַ��ƫ��ֵ���ŵ�edx�Ĵ���
				movl 	$0x00080000, %eax				# 0x0008 ---> eax�ĸ�16λ
				movw	%dx, %ax
				movw	$0x8E00, %dx						# �ж���������������

				lea		_idt, %edi							# _idt���ж���������ĵ�ַ
				mov 	$256, %ecx
rp_sidt:
				movl 	%eax, (%edi)
				movl 	%esx, 4(%edi)
				addl 	$8, %edi
				dec		%ecx
				jne		rp_sidt
				lidt	idt_descr								# �����ж���������Ĵ���ֵ
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

.org 0x5000														# ��������Ĵ�����ƫ��0x5000��ʼ��
/*
 * tmp_floppy_area is used by the floppy-driver when DMA cannot
 * reach to a buffer-block. It needs to be aligned, so that it isn't
 * on a 64kB border.
 */
_tmp_floppy_area:
				.fill 	1024, 1, 0						# ����1024�ÿ��1�ֽڣ������ֵ0.

after_page_tables:
				pushl	$0											# ��δ�����Ϊ������main������׼��������
				pushl	$0											# main�����Ϳ������Ϊ��һ����������x86����ģʽ����˵��
				pushl	$0											# ��������ջ��0����main����������������envp��argv��argc��
				pushl	$L6											# ����ģ����ǵ���main����ʱ�����ص�ַ��ջ�Ĳ�������main����
				pushl	$_main									# ��ķ���ʱ���ͻ᷵�ص�L6�������ִ����ȥ������ѭ������ô�ǲ���main�Ͳ�Ӧ�÷����أ�
				jmp		setup_paging						# ��main�ĵ�ַ��ջ����setup_paging����ʱ�ͻᵯ�������Ӷ�ȥִ��main������
L6:
				jmp		L6


/* This is the default interrupt "handler" :-) */
int_msg:
				.asciz "Unknow interrupt\n\r"	# �����ַ�����δ֪�ַ������س����У���
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
				pushl	$int_msg								# $����Ҫ��������ջ�Ĳ����ǵ�ַ�������ַ���Unkn��
				call 	_printk									# �ú�����/kernel/printk.c�У�����������ھͿ���ʹ�ã����ļ��������á�
				popl	%eax
				pop		%fs
				pop		%es
				pop		%ds
				popl	%edx
				popl	%ecx
				popl	%eax
				iret													# �жϷ��أ����жϵ���ʱ��ջ��CPU��־�Ĵ���CFLAGSֵҲ������


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
				cld;rep;stosl										# eax���ݴ�ŵ�es:edi��ָ�ڴ�λ�ô�����edi��4��
				movl 	$pg0+7, pg_dir
				movl 	$pg1+7, pg_dir+4
				movl 	$pg2+7, pg_dir+8
				movl 	$pg3+7, pg_dir+12
				movl	$pg3+4092, %edi
				movl	$0xfff007, %eax
				std															# ����λ��λ��ediֵ�ݼ���4�ֽڣ�
1:			stosl														# ��eax�Ĵ����е�����ת�浽es:edi�У�����λ��λ���������λ�ݼ�
				subl	$0x1000, %eax
				jge		1b
				xorl	%eax, %eax
				movl	%eax, %cr3
				movl	%cr0, %eax
				orl		$0x80000000, %eax
				movl 	%eax, %cr0								# ��cr0��PGλ��λ��������ҳ���ơ�ǰ��Ӧ�û���һ�������ֶα�־��PEλ�Ҽ��š�
				ret

.align 2
.word 0
idt_descr:
				.word 	256*8-1									# �жϱ��޳�
				.long		_idt										# �жϱ����/ƫ�ƣ���ַ
.align 2
.word 0
gdt_descr:
				.word		256*8-1
				.long		_gdt
				
				.align 	3
_idt:		.fill	256, 8, 0

_gdt:		.quad	0x0000000000000000
				.quad 0x00c09a0000000fff				# 0x08���ں˴������󳤶�16MB
				.quad 0x00c0920000000fff				# 0x10���ں����ݶ���󳤶�16MB
				.quad	0x0000000000000000
				.fill 252, 8, 0									# ��֪��ѡ���0x10��0x17��Ӧ�������������������õģ�