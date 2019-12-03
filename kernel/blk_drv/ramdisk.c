#define MAJOR_NR 1

char *rd_start;
int rd_length = 0;

void do_rd_request(void)
{
}

long rd_init(long mem_start, int length)
{
	int i;
	char *cp;

	blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;		// 注册请求函数
	rd_start 	= (char *)mem_start;					// 将主内存起始位置赋值给全局变量rd_start
	rd_length 	= length;								// 将虚拟磁盘长度（字节）赋值给变量rd_length
	cp = rd_start;
	for (i = 0; i < length; i++)
		*cp++ = '\0';
	return (length);
}
