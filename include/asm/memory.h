/* 内存块复制，从源地址src处开始复制n个字节到目的地址dest处。
 * 参数：dest - 目的地址
 *       src  - 源地址
 *       n    - 复制字节数
 *       %0   - "D"，表示寄存器edi
 *       %1   - "S"，表示寄存器esi
 *       %2   - "c"，表示寄存器ecx
 * 这里有一点需要强调，edi与esi寄存器需要分别于段选择子寄存器ds
 * es连用，memcpy宏（或内联函数）在被内核进程调用时，ds = es = 内核数据段；
 * 在被应用进程调用时，ds = es = 用户数据段，只有在这种前提下，memcpy才能
 * 正常工作。
 */
#define memcpy(dest, src, n) ({ \
void * _res = dest; \
__asm__ ("cld;" \
		  "rep;" \
		  "movsb" \
		  :
		  :"D" ((long)(_res)),"S" ((long)(src)),"c" ((long) (n)) \
		  :"di", "si", "cx"); \
_res; \
})