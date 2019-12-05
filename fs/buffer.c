#include <linux/sched.h>

/* 这个end是在外部文件定义的，实际上是在编译过程中由链接器指定的，
 * 其值代表内核模块末端，同时也是高速缓冲区的起始位置。
 */
extern int end;

/* 定义系统缓冲区中含有的缓冲块数 */
int NR_BUFFERS = 0;

struct buffer_head * start_buffer = (struct buffer_head *) &end;

/* 定义hash列表指针数组，数组成员为指针 */
struct buffer_head * hash_table[NR_HASH];

/* 用来指向第一个缓冲块头部 */
static struct buffer_head * free_list;

void buffer_init(long buffer_end)
{
	struct buffer_head * h = start_buffer;
	void * b;									// 实际缓冲区高端位置
	int i;

	if (buffer_end == 1<<20)					// 如果高速缓冲区结束位置为1MB
		b = (void *) (640*1024);
	else
		b = (void *) buffer_end;
	while ((b -= BLOCK_SIZE) >= ((void *) (h + 1)))
	{
		h->b_dev = 0;
		h->b_dirt = 0;
		h->b_count = 0;
		h->b_lock = 0;
		h->b_uptodate = 0;
		h->b_wait = NULL;
		h->b_next = NULL;
		h->b_prev = NULL;
		h->b_data = (char *) b;
		h->b_prev_free = h - 1;			// 指向链表中前一项，这里对于第一个缓冲块，其前一项已经不属于高速缓冲区的内容了，所以后面有处理
		h->b_next_free = h + 1;			// 指向链表中的下一项
		h++;
		NR_BUFFERS++;
		if (b == (void *) 0x100000)		// 如果b递减到等于1MB，则跳过384KB，为啥？
			b = (void *) 0xA0000;		// 让b指向地址0xA0000（640KB）处。
	}
	h--;								// 让h指向最后一个缓冲块头；
	free_list = start_buffer;			// 让空闲链表头指向第一个缓冲块头
	free_list->b_prev_free = h;			// 空闲链表头中的b_prev_free成员指向最后一个缓冲块头
	h->b_next_free = free_list;			// 让最后一个链表头的b_next_free成员指向第一个缓冲块头，从而形成了环形链表。
	for (i = 0; i < NR_HASH; i++)
	{
		hash_table[i] = NULL;
	}
}
