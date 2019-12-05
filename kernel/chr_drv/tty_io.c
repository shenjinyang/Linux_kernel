#include <tty.h>

void tty_init(void)
{
	rs_init();
	con_init();
}

void chr_dev_init(void)
{
}
