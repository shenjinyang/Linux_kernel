#define outb(value, port) \
__asm__("outb %%al, %%dx"::"a" (value), "d" (port))				// 将寄存器ax（value）的值写入到端口dx（port）中。

#define inb(port) ({ \
unsigned char _v; \
__asm__ volatile ("inb %%dx, %%al":"=a" (_v):"d"(port)); \		// 将端口port（dx）中的值写入到寄存器ax中并返回。
	_v; \
})

#define outb_p(value, port) \									// 同样是向端口写，区别是用了jmp跳转来起延时作用。
__asm__ ("outb %%al, %%dx\n" \
	"\tjmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:"::"a" (value), "d" (port))

#define inb_p(port) ({ \
unsigned char _v; \
__asm__ volatile ("inb %%dx, %%al\n" \
	"\tjmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1":"=a" (v):"d" (port)); \
_v; \
})
