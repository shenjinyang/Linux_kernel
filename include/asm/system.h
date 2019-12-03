/* 设置门描述符宏
 * 根据参数中的中断或异常处理程序地址addr、门描述符类型type和特权级别dpl，设置位于gate_addr处的门描述符；
 * 参数：gate_addr：门描述符地址；type：门描述符类型；dlp：门描述符特权级别；addr：处理过程偏移地址注意此处的
 * 偏移地址都是针对内核代码段或内和数据段来说的。
 * %0 - （由dpl、type组成的类型标志字）；%1 - （描述符低4字节地址）；%2 - （描述符高4字节地址）；
 * %3 - eax（程序偏移地址addr）；%4 - edx（高字中含有段选择符）
 */
#define _set_gate(gate_addr, type, dpl, addr) \
__asm__ ("movw %%dx, %%ax\n\t" \		// 将edx中低16位的程序入口地址与eax中高16位的段选择符组合成描述符低4字节（eax）
		"movw %0, %%dx\n\t" \			// 将类型标志字与偏移高字组合成描述符高4字节（edx）
		"movl %%eax, %1\n\t" \
		"movl %%edx, %2" \
		: \
		: "i" ((short) (0x8000+(dpl<<13) +(type<<8))), \
		"o" (*((char *) (gate_addr))), \
		"o" (*(4+(char *) (gate_addr))), \
		"d" ((char *) (addr)), "a" (0x00080000))

#define set_trap_gate(n, addr) \			// 设置陷阱门函数，参数：n - 中断号；addr - 中断程序偏移地址
	_set_gate(&idt[n], 15, 0, addr)			// 中断描述符的类型是14，特权级别是0

#define set_system_gate(n, addr) \
	_set_gate(idt[n], 15, 3, addr)