.code32
SYSWRITE = 4 # sys_write()系统调用号。
.globl mywrite, myadd
.text
mywrite:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	movl 8(%ebp), %ebx # 取调用者第 1 个参数：文件描述符 fd。
	movl 12(%ebp), %ecx # 取第 2 个参数：缓冲区指针。
	movl 16(%ebp), %edx # 取第 3 个参数：显示字符数。
	movl $SYSWRITE,%eax # %eax 中放入系统调用号 4。
	int $0x80 # 执行系统调用。
	popl %ebx
	movl %ebp, %esp
	popl %ebp
	ret
myadd:
	pushl %ebp
	movl %esp, %ebp
	movl 8(%ebp), %eax # 取第 1 个参数 a。
	movl 12(%ebp), %edx # 取第 2 个参数 b。
	xorl %ecx, %ecx # %ecx 为 0 表示计算溢出。
	addl %eax, %edx # 执行加法运算。
	jo 1f # 若溢出则跳转。
	movl 16(%ebp), %eax # 取第 3 个参数的指针。
	movl %edx, (%eax) # 把计算结果放入指针所指位置处。
	incl %ecx # 没有发生溢出，于是设置无溢出返回值。
1: 	movl %ecx, %eax # %eax 中是函数返回值。
	movl %ebp, %esp
	popl %ebp
	ret
