! 
! SYS_SIZE is the number of clicks (16 bytes) to be loaded.
! 0x3000 is 0x30000 bytes = 196kB, more than enough for current 
! versions of linux
!
#include <linux/config.h>
SYSSIZE = DEF_SYSSIZE
!
!     bootsect.s (C) 1991 Linus Torvalds
!     modified by Drew Eckhardt
!
! bootsect.s is loaded at 0x7c00 by the bios-startup routines, and moves
! iself out of the way to address 0x90000, and jumps there.
!
! It then loads 'setup' directly after itself (0x90200), and the system
! at 0x10000, using BIOS interrupts.
!
! NOTE! currently system is at most 8*65536 bytes long. This should be no
! problem, even in the future. I want to keep it simple. This 512 kB
! kernel size should be enough, especially as this doesn't contain the
! buffer cache as in minix
!
! The loader has been made as simple as possible, and continuos
! read errors will result in a unbreakable loop. Reboot by hand. It
! loads pretty fast by getting whole sectors at a time whenever possible.
!
.globl begtext, begdata, begbss, endtext, enddata, endbss
.text
begtext:
.data
begdata:
.bss
begbss:
.text

SETUPLEN 	= 4									! nr of setup-sectors
BOOTSEG 	= 0x7c0							! original adress of boot-sector
INITSEG		= DEF_INITSEG				! we move boot here(0x9000) - out of the way
SETUPSEG	= DEF_SETUPSEG			! setup start here(0x9200)
SYSSEG		= DEF_SYSSEG				! system loaded at 0x10000(65536)
ENDSEG		= SYSSEG + SYSSIZE	! where to stop load

! ROOT_DEV & SWAP_DEV are now written by "build".
ROOT_DEV = 0									! 根文件系统设备使用与系统引导时同样的设备；
SWAP_DEV = 0									! 交换设备使用与系统引导时同样的设备；

entry start										! entry是伪指令，迫使链接器在生成的执行程序中包含指定的标识符或标号，这里是程序的开始执行点。
start:
				mov 	ax, #BOOTSEG		! 将ds段寄存器设置为0x7c0；
				mov 	ds, ax
				mov 	ax, #INITSEG		! INITSEG = 0x9000
				mov 	es, ax
				mov 	cx #256					! 设置移动计数值=256字（512字节）
				sub 	si, si					! 源地址：0x7c00:0x0000
				sub 	di, di					! 目的地址：0x9000:0x0000
				rep
				movw
				jmpi 	go, INITSEG			! 段间跳转，这里INITSEG指出跳转到的段地址，标号go是段内偏移地址。

go:			mov 	ax, cs
				mov 	dx, #0xfef4
				
				mov 	ax, ds
				mov 	es, ax
				push 	ax							! 引导程序初始获得控制权时，ss = 0x00，sp = 0xfffe，所以栈在开始时就是存在的。
				
				mov 	ss, ax
				mov 	sp, dx
/*
 * Many BIOS's default disk parameter tables will not
 * recognize multi-sector reads beyond the maximum sector number
 * specified in the default diskette parameter tables - this may
 * mean 7 sectors in some cases.
 *
 * Since single sector reads are slow and out of the question,
 * we must take care of this by creating new parameter tables
 * (for the first disk) in RAM. We will set the maximum sector
 * count to 18 - the most we will encounter on an HD 1.44.
 *
 * High doesn't hurt. Low does.
 *
 * Segments are as follows: ds=es=ss=cs - INITSEG,
 * fs = 0, gs = parameter table segment
 */
 
 
 				push 	#0							! set fs = 0。
 				pop 	fs
 				mov 	bx, #0x78				! fs:bx is parameter table adress
 				seg 	fs							! 伪指令seg表示其下一条语句的操作数在fs段中
 				lgs		si, (bx)				! fs--->gs, bx--->si; so (gs:si) is source
 				
 				mov 	di, dx					! (es:di) is destination! 为什么就指定了寄存器es了呢？
 				mov 	cx, #6					! copy 12 bytes
 				cld										! 清方向标志，复制时指针递增。
 				
 				rep
 				seg 	gs
 				movw
 				
 				mov 	di, dx					! 之所以在93行后再一次赋值，是因为在copy过程中，di的值增加了，所以要恢复到原来的dx中的值
 				movb 	4(di), *18			! 18--->(es:di+4)，表示修改每磁道的最大扇区数为18。
 				
 				seg 	fs
 				mov 	(bx), di				! (di)--->(fs:bx)，即让中断向量0x1E的值指向新表，也就是0x1E处存放的地址变成新表的地址
 				seg 	fs
 				mov 	2(bx), es				! (es)--->(fs:bx+2)
 				
 				pop 	ax							! ax中是65行的压栈值0x9000。
 				mov 	fs, ax
 				mov 	gs, ax
 				
 				xor 	ah, ah					! 复位软盘控制器，让其采用新参数
 				xor 	dl, dl					! dl = 0，第一个软驱。
 				int 	0x13						! BIOS中断，取磁盘参数表中当前启动引导盘的参数，113和114行应该都是给这个中断用的。

! load the setup-sectors directly after the bootblock.
! Note that 'es' is already set up.

load_setup:
				xor 	dx, dx									! dh = 磁头号 = 0，dl = 驱动器号 = 0
				mov 	cx, #0x0002							! ch = 磁道（柱面）的低8位，cl = 开始扇区（位0~5），磁道号高2位（位6~7）
				mov 	bx, #0x0200							! address = 512, in INITSEG
				mov 	ax, #0x0200+SETUPLEN		! ah = 0x02 - 读磁盘扇区到内存；al = 需要读出的扇区数量；
				int 	0x13										! 开始读。
				jnc 	ok_load_setup						! 读完继续执行
				
				push 	ax											! 显示出错信息，出错码入栈，推测出错码存放在了ax中，毕竟ax用来存返回值。
				call 	print_nl								! 屏幕光标回车
				mov 	bp, sp 									! ss:bp指向欲显示的字，sp不能随便改动，但是可以通过bp来访问堆栈中的值。
				call 	print_hex 							! 显示16进制值。
				pop 	ax
				
				xor 	dl, dl
				xor 	ah, ah
				int 	0x13
				j			load_setup

ok_load_setup:

! Get disk drive parameters, specifically nr of sectors/track

				xor 	dl, dl									! 这段代码用BIOS的0x13中断的功能8来取磁盘驱动器的参数
				mov 	ah, #0x08
				int 	0x13
				xor 	ch, ch
				seg 	cs
				mov 	sectors, cx
				mov 	ax, #INITSEG
				mov 	es, ax

! Print some inane message

				mov 	ah, #0x03								! BIOS中断0x10，功能号ah = 0x03，读光标位置，并返回光标位置到dx中。
				xor 	bh, bh									! 输入bh = 页号。
				int 	0x10
				
				mov 	cx, #9
				mov 	bx, #0x0007
				mov 	bp, #msg1
				mov 	ax, #0x1301
				int 	0x10

! ok, we've written the message, now
! we want to load the system (at 0x10000)

				mov 	ax, #SYSSEG							! SYSSEG = 0x010000
				mov 	es, ax
				call 	read_it
				call 	kill_motor							! 关闭驱动马达，？
				call 	print_nl								! 光标回车换行。

! After that we check which root-device to use. If the device is
! defined (!= 0), nothing is done and the given device is used.
! Otherwise, either /dev/PS0 (2,28) or /dev/at0 (2,8), depending
! on the number of sectors that the BIOS reports currently.

				seg 	cs
				mov 	ax, root_dev						! 取508、509字节处的根设备号
				or 		ax, ax									! 判断根设备是否已定义，为0则没有定义，需要根据BIOS报告的每磁道扇区数来确定使用
				jne 	root_defined						! 哪个设备
				seg 	cs
				mov 	bx, sectors
				mov 	ax, #0x0208
				cmp 	bx, #15
				je 		root_defined
				mov 	ax, #0x021c
				cmp 	ax, #18
				je		root_defined
undef_root:
				jmp 	undef_root
root_defined:
				seg 	cs
				mov 	root_dev, ax

! after that (everyting loaded), we jump to
! the setup-routine loaded directly after
! the bootblock:

				jmp 	0, SETUPSEG							! 这就结束了。

! This routine loads the system at address 0x10000, making sure
! no 64kB boundaries are crossed. We try to load it as fast as
! possible, loading whole tracks whenever we can.

! in: es - starting address segment (normally 0x1000)

sread: 	.word 1+SETUPLEN							! 当前磁道中已读扇区数
head:		.word 0												! 当前磁头号
track:	.word 0												! 当前磁道号

read_it:
				mov 	ax, es									! test的两个操作数，如果对应的比特位都为1，则结果的对应比特位为1，反之为0；
				test 	ax, #0x0fff							! 如test 0x1000, 0x0fff，那么结果就是0x0000，ZF被置位，所以下一条jne不成立。 
die:		jne 	die
				xor 	bx, bx									! bx为段内偏移。
rp_read:
				mov 	ax, es
				cmp 	ax, #ENDSEG							! es后面肯定会更新的，即拷贝到哪里es就更新为哪里，如果最后es已经到了结束的地方，那么就返回。
				jb 		ok1_read								! 如果还没有到，那么就跳转到ok1_read继续读。
				ret
ok1_read:
				seg 	cs
				mov 	ax, sectors							! sectors是标号，即汇编地址，存储每磁道的扇区数，那么上一行的seg cs就是指将段寄存器指认为cs，就是0x9000。
				sub 	ax, sread								! 减去已经读了的扇区数。
				mov 	cx, ax									! 计数
				shl 	cx, #9									! 左移9位即乘512，将扇区数换算成字节数，加上bx中存储的当前偏移值，即表示此次读操作后，段内共读入的字节数。
				add 	cx, bx
				jnc 	ok2_read								! 若没有超过64KB（即一个段的长度），则可以跳转到ok2_read处执行，因为cx是16位寄存器，最大即表示64KB，
				je 		ok2_read								! 如果无进位，即jnc成立，所以跳转，顺序向下执行。当然刚好等于64KB也转移。
				xor 	ax, ax									! 我们可以先算出64KB的段中还有多少字节可以用来读入，再将其转换成为扇区数存在ax中。
				sub 	ax, bx
				shr 	ax, #9
ok2_read:
				call 	read_track							! 读当前磁道上指定开始扇区（cl）和需读扇区数目（al）的数据。
				mov 	cx, ax									! cx中存储的是本次读操作打算要读取的扇区数；
				add 	ax, sread								! 加上已经读取的扇区数；
				seg 	cs
				cmp 	ax, sectors							! 与总共要读的扇区数相比，非0即跳转
				jne		ok3_read
				mov 	ax, #1
				sub 	ax, head								! head = 0（初始值）；
				jne		ok4_read								! 如果是0磁头，则再去读1磁头面上的扇区数据，非0即跳转。
				inc 	track										! 否则读下一磁道。
ok4_read:
				mov 	head, ax
				xor 	ax, ax									! 清空当前磁道已读扇区数。
ok3_read:
				mov 	sread, ax								! 保存当前磁道已读扇区数。
				shl 	cx, #9
				add 	bx, cx									! 如果小于64KB的边界值，则跳转到rp_read继续读数据。
				jnc 	rp_read
				mov 	ax, es
				add 	ah, #0x10								! 将es中的段基址增加64KB，落实到代码就是将其高8位加0x10，从而定位到下一个段中。
				mov 	es, ax
				xor 	bx, bx
				jmp 	rp_read

read_track:
				pusha													! 压入所有的寄存器（push all）
				pusha													! 为调用显示中断压入所有寄存器值，什么鬼？
				mov 	ax, #0xe2e
				mov 	bx, #7
				int 	0x10										! 调用BIOS中断0x10，功能0x0e，光标前移一位置，显示一个“.”
				popa
				
				mov 	dx, track
				mov 	cx, sread
				inc 	cx
				mov 	ch, dl
				mov 	dx, head
				mov 	dh, dl
				and 	dx, #0x0100
				mov 	ah, #2
				
				push 	dx
				push 	cx
				push	bx
				push 	ax
				
				int 	0x13
				jc 		bad_rt
				add 	sp, #8
				popa
				ret

bad_rt:	push 	ax
				call 	print_all
				
				
				xor 	ah, ah
				xor 	dl, dl
				int 	0x13
				
				
				add 	sp, #10
				popa
				jmp 	read_track

/*
 * print_all is for debugging purposes.
 * It will print out all of the registers. The assumption is that this is
 * called from a routine, with a stack frame like
 * dx
 * cx
 * bx
 * ax
 * error
 * ret <- sp
 *
 */
 
 print_all:
 				mov 	cx, #5
 				mpv 	bp, sp
 
 print_loop:
 				push 	cx
 				call 	print_nl
 				jae 	no_reg									! 若CF = 0则不显示寄存器名，于是跳转。
 				
 				mov 	ax, #0xe05 + 0x41 -1		! ah =功能号（0x0e）；al =字符（0x05 + 0x41 -1）。
 				sub 	al, cl
 				int 	0x10
 				
 				mov 	al, #0x58
 				int 	0x10
 				
 				mov 	al, #0x3a
 				int 	0x10

no_reg:
				add 	bp, #2
				call 	print_hex
				pop 	cx
				loop	print_loop
				ret

print_nl:
				mov 	ax, #0xe0d
				int 	0x10
				mov 	al, #0xa
				int 	0x10
				ret

/*
 * print_hex is for debugging purposes, and prints the word
 * pointed to by ss:bp in hexadecmial.
 */
 
print_hex:
 				mov 	ax, #4
 				mov 	dx, (bp)
print_digit:
				rol 	dx, #4
 				mov 	ah, #x0e
 				mov 	al, dl
 				and 	al, #0xf
 				add 	al, #0x30
 				cmp		al, #0x39
 				jbe 	good_digit
 				add 	al, #0x41 - 0x30 - 0xa

good_digit:
				int 	0x10
				loop 	print_digit
				ret

/*
 * This procedure turns off the floppy drive motor, so
 * that we enter the kernel in a known state, and
 * don't have to worry about it later.
 */

kill_motor:
				push 	dx
				mov 	dx, #0x3f2
				xor 	al, al
				outb
				pop 	dx
				ret

sectors:
				.word 	0

msg1:
				.byte		13, 10
				.ascii	"Loading"

.org 506
swap_dev:
				.word 	SWAP_DEV
root_dev:
				.word		ROOT_DEV
boot_flag:
				.word 	0XAA55

.text
endtext:
.data
enddata:
.bss
endbss:
