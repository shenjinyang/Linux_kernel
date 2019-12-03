#ifndef _STDARG_H
#define _STDARG_H

typedef char *va_list;

/* 下面给出了类型为TYPE的arg参数列表所要求的空间容量。
 * TYPE也可以是使用该类型的一个表达式。 */
// 下面定义了取整后TYPE类型的字节长度值，是int长度（4字节）的倍数。
#define __va_rounded_size(TYPE)\
	(((sizeof (TYPE) + sizeof (int) - 1) / sizeof (int)) * sizeof (int))

#ifndef __sparc__
#define va_start(AP, LASTARG)				\
	(AP = (char *) &(LASTARG) + __va_rounded_size (LASTARG))
#else
#define va_start(AP, LASTARG)				\
	(__builtin_saveregs ()					\
	AP = ((char *) &(LASTARG) + __va_rounded_size(LASTARG)))
#endif

void va_end(va_list);			// 声明函数va_end，具体定义在gunlib中。
#define va_end(AP)
