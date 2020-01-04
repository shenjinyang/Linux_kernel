#include <linux/tty.h>
#include <asm/system.h>

/* 这些是setup程序在引导启动系统时设置的参数 */
#define ORIG_X			(*(unsigned char *)0x90000)			/* 初始光标列号 */
#define ORIG_Y			(*(unsigned char *)0x90001)			/* 初始光标行号 */
#define ORIG_VIDEO_PAGE		(*(unsigned short *)0x90004)	/* 初始显示页面 */
#define ORIG_VIDEO_MODE		((*(unsigned short *)0x90006) & 0xff)				/* 显示模式 */
#define ORIG_VIDEO_COLS 	(((*(unsigned short *)0x90006) & 0xff00) >> 8)		/* 屏幕列数 */
#define ORIG_VIDEO_LINES	((*(unsigned short *)0x9000e) & 0xff)				/* 屏幕行数 */
#define ORIG_VIDEO_EGA_AX	(*(unsigned short *)0x90008)	/* ? */
#define ORIG_VIDEO_EGA_BX	(*(unsigned short *)0x9000a)	/* 显存大小和色彩模式 */
#define ORIG_VIDEO_EGA_CX	(*(unsigned short *)0x9000c)	/* 显卡特性参数 */

/* 定义显示器单色/彩色显示模式类型符号常数 */
#define VIDEO_TYPE_MDA		0x10	/* 单色文本	*/
#define VIDEO_TYPE_CGA		0x11	/* CGA显示器 */
#define VIDEO_TYPE_EGAM		0x20	/* EGA/VGA 单色	*/
#define VIDEO_TYPE_EGAC		0x21	/* EGA/VGA 彩色	*/

#define NPAR 16						/* 转义字符序列中最大参数个数 */

int NR_CONSOLES = 0;				/* 系统实际支持的虚拟控制台数量 */

/* 键盘中断处理程序（keyboard.S） */
extern void keyboard_interrupt(void);

/* 本文件中使用的一些全局静态变量 */
static unsigned char video_type;				/* 显示类型 */
static unsigned long video_num_columns;			/* 屏幕文本列数 */
static unsigned long video_size_row;			/* 屏幕每行使用的字节数，一个字符两个字节，高字节为属性，低字节为字符 */
static unsigned long video_num_lines;			/* 屏幕文本行数 */
static unsigned long video_page;				/* 初始显示页面，也是写死在BIOS中的 */
static unsigned long video_mem_base;			/* 物理显示内存基地址 */
static unsigned long video_port_reg;			/* 显示控制寄存器索引端口 */
static unsigned long video_port_val;			/* 显示控制寄存器数据端口 */
static unsigned long video_mem_term;			/* 物理显存末端地址 */
static int can_do_color = 0;					/* 可否使用彩色标志，1 -- 可以，0 -- 不可以 */

/* 定义虚拟控制台结构，它包含一个虚拟控制台的当前所有信息 */
static struct {
	unsigned short	vc_video_erase_char;		/* 擦除字符的属性和字符（0x0720） */
	unsigned char	vc_attr;					/* 字符属性（即显示的所有字符都是这个属性） */
	unsigned char	vc_def_attr;				/* 默认字符属性 */
	int				vc_bold_attr;				/* 粗体字符属性 */
	unsigned long	vc_ques;					/* 问好属性 */
	unsigned long	vc_state;					/* 处理转义或控制序列的当前状态 */
	unsigned long	vc_restate;					/* 处理转义或控制序列的下一状态 */
	unsigned long	vc_checkin;					/* ？ */
	unsigned long	vc_origin;					/* Used for EGA/VGA fast scroll	*/
	unsigned long	vc_scr_end;					/* Used for EGA/VGA fast scroll	*/
	unsigned long	vc_pos;						/* 当前光标对应的显示内存位置 */
	unsigned long	vc_x,vc_y;					/* 当前光标列、行值 */
	unsigned long	vc_top,vc_bottom;			/* 滚动时顶行行号、底行行号 */
	unsigned long	vc_npar,vc_par[NPAR];		/* 转义序列参数个数和参数数组 */
	unsigned long	vc_video_mem_start;			/* 当前正在处理的虚拟控制台使用的显示内存开始处 */
	unsigned long	vc_video_mem_end;			/* 当前正在处理的虚拟控制台使用的显示内存结束处	*/
	unsigned int	vc_saved_x;					/* 保存的光标列号 */
	unsigned int	vc_saved_y;					/* 保存的光标行号 */
	unsigned int	vc_iscolor;					/* 彩色显示标志 */
	char *			vc_translate;				/* 使用的字符集 */
} vc_cons [MAX_CONSOLES];

/* 为了便于引用，以下定义当前正在处理控制台信息的符号，这样就不用间接引用结构体vc_cons中的成员了，
 * 其中currcons是使用vc_cons[]结构的函数参数中的当前虚拟终端号。
 */
#define origin		(vc_cons[currcons].vc_origin)
#define scr_end		(vc_cons[currcons].vc_scr_end)
#define pos			(vc_cons[currcons].vc_pos)
#define top			(vc_cons[currcons].vc_top)
#define bottom		(vc_cons[currcons].vc_bottom)
#define x			(vc_cons[currcons].vc_x)
#define y			(vc_cons[currcons].vc_y)
#define state		(vc_cons[currcons].vc_state)
#define restate		(vc_cons[currcons].vc_restate)
#define checkin		(vc_cons[currcons].vc_checkin)
#define npar		(vc_cons[currcons].vc_npar)
#define par			(vc_cons[currcons].vc_par)
#define ques		(vc_cons[currcons].vc_ques)
#define attr		(vc_cons[currcons].vc_attr)
#define saved_x		(vc_cons[currcons].vc_saved_x)
#define saved_y		(vc_cons[currcons].vc_saved_y)
#define translate	(vc_cons[currcons].vc_translate)
#define video_mem_start		(vc_cons[currcons].vc_video_mem_start)
#define video_mem_end		(vc_cons[currcons].vc_video_mem_end)
#define def_attr			(vc_cons[currcons].vc_def_attr)
#define video_erase_char  	(vc_cons[currcons].vc_video_erase_char)	
#define iscolor				(vc_cons[currcons].vc_iscolor)

int blankinterval = 0;		/* 设定的屏幕黑屏间隔时间 */
int blankcount = 0;			/* 黑屏时间计数 */

/* 定义使用的字符集 */
static char * translations[] = {
/* normal 7-bit ascii */
	" !\"#$%&'()*+,-./0123456789:;<=>?"
	"@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
	"`abcdefghijklmnopqrstuvwxyz{|}~ ",
/* vt100 graphics */
	" !\"#$%&'()*+,-./0123456789:;<=>?"
	"@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^ "
	"\004\261\007\007\007\007\370\361\007\007\275\267\326\323\327\304"
	"\304\304\304\304\307\266\320\322\272\363\362\343\\007\234\007 "
};

#define NORM_TRANS (translations[0])
#define GRAF_TRANS (translations[1])

/* 追踪光标当前位置
 * 参数：currcons -- 当前虚拟终端号
 *       new_x    -- 光标所在列号
 *       new_y    -- 光标所在行号
 * 光标的位置会被记录在显示内存中。
 */
static inline void gotoxy(int currcons, int new_x, unsigned int new_y)
{
	if (new_x > video_num_columns || new_y >= video_num_lines)	/* 光标不能超出屏幕 */
		return;

	x 	= new_x;
	y 	= new_y;
	pos = origin + y * video_size_row + (x << 1); 
}

/* 设置滚屏起始显示内存地址 */
static inline void set_origin(int currcons)
{
	/* 显卡EGA/VGA支持指定屏内行范围进行滚屏操作，而MDA单色显卡只支持整屏滚动操作（即翻页），
	 * 因此只有EGA/VGA显卡才需要设置滚屏起始行显示内存地址（起始行是origin对应的行），即显示
	 * 类型不是EGA/VGA彩色模式，也不是EGA/VGA单色模式，就直接返回。
	 */
	if (video_type != VIDEO_TYPE_EGAC && video_type != VIDEO_TYPE_EGAM)
		return;

	/* 只对前台控制台进行操作，即只有当前控制台currcons是前台控制台时，才设置。 */
	if (currcons != fg_console)
		return;

	cli();

	/* 选择显示控制器r12 */
	outb_p(12, video_port_reg);

	/* 向r12中写入数据滚屏起始地址高字节，先右移8位保留高字节，再右移1位表示除以2（1字符用2字节表示） */
	outb_p(0xff&((origin - video_mem_base)>>9), video_port_val);
	outb_p(13, video_port_reg);
	outb_p(0xff&((origin - video_mem_base)>>1), video_port_val);
	sti();
}

/* 设置显示光标
 * 根据光标对应显示内存位置pos，设置显示控制器光标的显示位置。
 */
static inline void set_cursor(int currcons)
{
	blankcount = blankinterval;

	/* 显示光标的控制台必须是当前控制台，因此若当前处理的控制台号currcons不是前台
	 * 控制台就立即返回，所以前台控制台就是当前正在操作的那个控制台。
	 */
	if (currcons != fg_console)
		return;
	cli();
	outb_p(14, video_port_reg);
	outb_p(0xff&((pos-video_mem_base)>>9), video_port_val);
	outb_p(15, video_port_reg);
	outb_p(0xff&((pos-video_mem_base)>>1), video_port_val);
	sti();
}

/* 这个枚举定义用于在下面 con_write()函数中解析转义序列或控制序列。
 * ESnormal - 初始进入状态，也是转义或控制序列处理完毕时的状态。
 * ESnormal - 表示处于初始正常状态。此时若接收到的是普通显示字符，则把字符直接显示
 * 在屏幕上；若接收到的是控制字符（例如回车字符），则对光标位置进行设置。
 * 当刚处理完一个转义或控制序列，程序也会返回到本状态。
 *
 * ESesc    - 表示接收到转义序列引导字符 ESC（0x1b = 033 = 27）；如果在此状态下接收
 * 到一个'['字符，则说明是转义序列引导码，于是跳转到 ESsquare 去处理。否则
 * 就把接收到的字符作为转义序列来处理。对于选择字符集转义序列'ESC (' 和'ESC )'，
 * 我们使用单独的状态 ESsetgraph 来处理；对于设备控制字符串序列'ESC P'，
 * 我们使用单独的状态 ESsetterm 来处理。
 *
 * ESsquare - 表示已经接收到一个控制序列引导码（'ESC ['），表示接收到的是一个控制序
 * 列。于是本状态对参数数组 par[]执行清零初始化工作。如果此时接收到的又是
 * 一个'['字符，则表示收到了'ESC [['序列。该序列是键盘功能键发出的序列，
 * 于是跳转到 Esfunckey 去处理。否则我们需要准备接收控制序列的参数，于是
 * 置状态 Esgetpars 并直接进入该状态去接收并保存序列的参数字符。
 *
 * ESgetpars - 该状态表示我们此时要接收控制序列的参数值。参数用十进制数表示，我们把
 * 接收到的数字字符转换成数值并保存到 par[]数组中。如果收到一个分号 ';'，
 * 则还是维持在本状态，并把接收到的参数值保存在数据 par[]下一项中。若不是
 * 数字字符或分号，说明已取得所有参数，那么就转移到状态 ESgotpars 去处理。
 *
 * ESgotpars - 表示我们已经接收到一个完整的控制序列。此时我们可以根据本状态接收到的结
 * 尾字符对相应控制序列进行处理。不过在处理之前，如果我们在 ESsquare 状态
 * 收到过 '?'，说明这个序列是终端设备私有序列。本内核不支持对这种序列的处
 * 理，于是我们直接恢复到 ESnormal 状态。否则就去执行相应控制序列，待序列
 * 处理完后就把状态恢复到 ESnormal。
 *
 * ESfunckey - 表示我们接收到了键盘上功能键发出的一个序列，我们不用显示。于是恢复到正
 * 常状态 ESnormal。
 *
 * ESsetterm - 表示处于设备控制字符串序列状态（DCS）。此时若收到字符 'S'，则恢复初始
 * 的显示字符属性。若收到的字符是'L'或'l'，则开启或关闭折行显示方式。
 *
 * ESsetgraph -表示收到设置字符集转移序列'ESC (' 或 'ESC )'。它们分别用于指定 G0 和 G1
 * 所用的字符集。此时若收到字符 '0'，则选择图形字符集作为 G0 和 G1，若收到
 * 的字符是 'B'，这选择普通 ASCII 字符集作为 G0 和 G1 的字符集。 
 */
enum { ESnormal, ESesc, ESsquare, ESgetpars, ESgotpars, ESfunckey, 
	ESsetterm, ESsetgraph };

/* 初始化控制台终端。
 * 读取setup.s程序保存的信息，用以确定显示器类型，并设置所有相关参数。
 */
void con_init(void)
{
	register unsigned char a;
	char * display_desc = "????";
	char * display_ptr;
	int currcons = 0;
	long base, term;
	long video_memory;

	video_num_columns 	= ORIG_VIDEO_COLS;
	video_size_row 		= video_num_columns * 2;
	video_num_lines 	= ORIG_VIDEO_LINES;
	video_page 			= ORIG_VIDEO_PAGE;
	video_erase_char 	= 0x0720;			/* 擦除字符（0x20是字符，0x07是属性） */
	blankcount 			= blankinterval;	/* 默认的黑屏间隔时间，在这里应该还都是0吧，没有被初始化呢 */

	if (ORIG_VIDEO_MODE == 7)				/* 显示模式为7表示为单色显示卡 */
	{
		video_mem_base 		= 0xb0000;		/* 设置单显映像内存起始地址 */
		video_port_reg 		= 0x3b4;		/* 设置单显索引寄存器端口 */
		video_port_val		= 0x3b5;		/* 设置单显数据寄存器端口 */

		/* 显示模式信息不等于0x10，说明是EGA单色，只支持0xb0000 ~ 0xb8000之间的显存，32KB */
		if ((ORIG_VIDEO_EGA_BX & 0XFF) != 0x10)
		{
			video_type 			= VIDEO_TYPE_EGAM;
			video_mem_term		= 0xb8000;
			display_desc 		= "EGAm";
		}

		/* 显示模式信息等于0x10，说明是MDA单色，只支持更小的内存，8KB */
		else
		{
			video_type 			= VIDEO_TYPE_MDA;
			video_mem_term		= 0xb2000;
			display_desc 		= "*MDA";
		}
	}

	/* 若显示模式不为7，说明是彩色显卡 */
	else
	{
		can_do_color		= 1;
		video_mem_base		= 0xb8000;
		video_port_reg		= 0x3d4;
		video_port_val		= 0x3d5;

		if ((ORIG_VIDEO_EGA_BX & 0xff) != 0x10)			/* 判断显卡类别，如果是EGA显卡，支持32KB */
		{
			video_type 			= VIDEO_TYPE_EGAC;
			video_mem_term		= 0xc0000;
			display_desc 		= "EGAc";
		}

		else											/* 如果是CGA显卡，只支持8KB */
		{
			video_type 			= VIDEO_TYPE_EGAM;
			video_mem_term		= 0xba000;
			display_desc 		= "*CGA";
		}
	}

	video_memory = video_mem_term - video_mem_base;		/* 计算显示内存大小 */

	/* 计算显示内存需要几页能显示完，其实我理解的也没错，但是让我们再仔细想想，一个控制台任意时刻
	 * 都只能显示1页终端图形界面，那么全部显示内存所支持的显示页数，也就是系统支持的最大虚拟控制台数了。
	 */
	NR_CONSOLES	= video_memory / (video_num_lines * video_size_row);

	if (NR_CONSOLES > MAX_CONSOLES)
		NR_CONSOLES = MAX_CONSOLES;
	if (!NR_CONSOLES)
		NR_CONSOLES = 1;

	video_memory /= NR_CONSOLES;		/* 每个虚拟控制台占用显示内存字节数 */

	/* 让用户知道我们正在使用哪类显示驱动程序 */
	display_ptr = ((char *)video_mem_base) + video_size_row - 8;
	while (*display_desc)
	{
		*display_ptr++ = *display_desc++;
		display_ptr++;
	}

	/* 初始化用于滚屏的变量 */
	base 		= origin = video_mem_start = video_mem_base;		/* 使用video_mem_base说明这里初始化的是0号虚拟控制台 */
	term 		= video_mem_end = base + video_memory;
	scr_end 	= video_mem_start + video_num_lines * video_size_row; 
	top 		= 0;
	bottom 		= video_num_lines;
	attr 		= 0x07;
	def_attr	= 0x07;
	restate 	= state = ESnormal;			/* 初始化转义字符序列操作的当前状态和下一状态 */
	checkin 	= 0;
	ques		= 0;
	iscolor		= 0;
	translate	= NORM_TRANS;
	vc_cons[0].vc_bold_attr = -1;			/* -1表示不适用粗体字符属性 */

	gotoxy(currcons, ORIG_X, ORIG_Y);

	/* 循环设置其余几个虚拟控制台终端，很巧妙，学着点 */
	for (currcons = 1; currcons < NR_CONSOLES; currcons++)
	{
		vc_cons[currcons] = vc_cons[0];
		origin 	= video_mem_start = (base += video_memory);
		scr_end	= origin + video_num_lines * video_size_row;
		video_mem_end = (term += video_memory);
		gotoxy(currcons, 0, 0);
	}

	update_screen();

	/*  */
	set_trap_gate(0x21, &keyboard_interrupt);
	outb_p(inb_p(0x21)&0xfd,0x21);
	a=inb_p(0x61);
	outb_p(a|0x80,0x61);
	outb_p(a,0x61);
}

/* 更新当前前台控制台
 * 把前台控制台换位fg_console指定的虚拟控制台，fg_console是设置的前台虚拟控制台号。
 * fg_console最开始被初始化为0，所以最开始应该是初始化虚拟控制台0。
 */
void update_screen()
{
	set_origin(fg_console);
	set_cursor(fg_console);
}