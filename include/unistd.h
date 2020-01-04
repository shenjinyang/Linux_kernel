#ifndef __UNISTD_H
#define __UNISTD_H

#ifdef __LIBRARY__

#define __NR_fork 2
#define __NR_pause 29

#define _syscall0(type, name) \			/* 不带参数的系统调用宏函数 */
type name(void) \
{
	long __res; \
	__asm__ volatile ("int $0x80" \		/* 以eax为调用号来调用系统中断0x80 */
		: "=a" (__res) \				/* 返回值为eax，输出值存在变量__res中 */
		: "0" (__NR_##name)); \			/* 输入为系统中断调用号__NR_name，使用“0”表示输入寄存器与对应位置的输出寄存器相同为eax */
	if (__res >= 0) \					/* 如果返回值>=0 */
		return (type) __res; \			/* 则直接返回该值 */
	errno = -__res;						/* 否则设置出错号 */
	return -1; \						/* 并返回-1 */
}

#define _syscall1(type, name, atype, a) \		/* 带1个参数的系统调用宏函数 */
type name(atype a) \
{
	long __res; \
	__asm__ volatile ("int $0x80" \
		: "=a" (__res) \
		: "0" (__NR_##name), "b" ((long)(a))); \
	if (__res >= 0) \
		return (type) __res; \
	errno = -__res; \
	return -1; \
}

#endif /* __LIBRARY__ */

extern int errno;

int fork(void);

#endif