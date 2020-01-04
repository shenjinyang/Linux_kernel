#ifndef _STRING_H_
#define _STRING_H_

extern inline int strncmp(const char * cs,const char * ct,int count)
{
register int __res __asm__("ax");
__asm__("cld\n"
	"1:\tdecl %3\n\t"
	"js 2f\n\t"
	"lodsb\n\t"
	"scasb\n\t"
	"jne 3f\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b\n"
	"2:\txorl %%eax,%%eax\n\t"
	"jmp 4f\n"
	"3:\tmovl $1,%%eax\n\t"
	"jl 4f\n\t"
	"negl %%eax\n"
	"4:"
	:"=a" (__res):"D" (cs),"S" (ct),"c" (count):"si","di","cx");
return __res;
}


/* 当在.h文件中包含内联函数时，如果不使用关键字static，若该头文件被多个c文件包含，那么就会
 * 产生函数重定义的编译问题，使用static之后可以实现在多个文件中调用一个相同名字的函数，防止
 * 命名空间的污染；这里使用关键字extern修饰inline同样可以避免上述问题，即表示该函数已经在别
 * 的文件中声明过了，不用再分配内存。函数本省可以多次声明。
 */
extern inline void * memcpy(void * dest,const void * src, int n)
{
__asm__("cld\n\t"
	"rep\n\t"
	"movsb"
	::"c" (n),"S" (src),"D" (dest)
	:"cx","si","di");
return dest;
}

extern inline void * memset(void * s,char c,int count)
{
__asm__("cld\n\t"
	"rep\n\t"
	"stosb"
	::"a" (c),"D" (s),"c" (count)
	:"cx","di");
return s;
}

#endif
