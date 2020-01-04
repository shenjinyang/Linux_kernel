#ifndef _TERMIOS_H
#define _TERMIOS_H

#include <sys/types.h>

/* POSIX的termios结构 */
#define NCCS 17						/* termios结构中控制字符数组长度 */
struct termios {
	tcflag_t c_iflag;				/* 输入模式标志 */
	tcflag_t c_oflag;				/* 输出模式标志 */
	tcflag_t c_cflag;				/* 控制模式标志 */
	tcflag_t c_lflag;				/* 本地模式标志 */
	cc_t c_line;					/* 线路规划（速率） */
	cc_t c_cc[NCCS];				/* 控制字符数组 */
};

#endif