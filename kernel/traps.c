/* 下面声明所有将被在trap_init函数中挂载的地址，函数的具体定义在kernel/asm.s中 */
void divide_error(void);
void debug(void);
void nmi(void);
void int3(void);
void overflow(void);
void bounds(void);
void invalid_op(void);
void device_not_available(void);
void double_fault(void);
void coprocessor_segment_overrun(void);
void invalid_TSS(void);
void segment_not_present(void);
void stack_segment(void);
void general_protection(void);
void page_fault(void);
void coprocessor_error(void);
void reserved(void);
void parallel_interrupt(void);
void irq13(void);


void trap_init(void)
{
	int i;

	set_trap_gate(0, &divide_error);		// 门描述符宏，负责设置门描述符，参数n为中断号，&divide_error为中断处理函数的入口偏移地址（相对内核代码）
	set_trap_gate(1, &debug);
	set_trap_gate(2, &nmi);
	set_system_gate(3, &int3);				// 设置门描述符，与trap的区别在于特权级别不同，为3（为什么是3呢）。
	set_system_gate(4,&overflow);
	set_system_gate(5,&bounds);
	set_trap_gate(6,&invalid_op);
	set_trap_gate(7,&device_not_available);
	set_trap_gate(8,&double_fault);
	set_trap_gate(9,&coprocessor_segment_overrun);
	set_trap_gate(10,&invalid_TSS);
	set_trap_gate(11,&segment_not_present);
	set_trap_gate(12,&stack_segment);
	set_trap_gate(13,&general_protection);
	set_trap_gate(14,&page_fault);
	set_trap_gate(15,&reserved);
	set_trap_gate(16,&coprocessor_error);
	for (i = 17; i<48; i++)
		set_trap_gate(i, &reserved);
	set_trap_gate(45, &irq13);					// 设置协处理器int45陷阱门描述符
	outb_p(inb_p(0x21)&0xfb, 0x21);				// 允许8259A主芯片的IRQ2中断请求信号
	outb(inb_p(0xA1)&0xdf, 0xA1);				// 允许8259A从芯片的IRQ13中断请求信号
	set_trap_gate(39, &parallel_interrupt);		// 设置并行口1的中断0x27陷阱门描述符
}
