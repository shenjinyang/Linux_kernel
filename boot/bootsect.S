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
ROOT_DEV = 0									! ���ļ�ϵͳ�豸ʹ����ϵͳ����ʱͬ�����豸��
SWAP_DEV = 0									! �����豸ʹ����ϵͳ����ʱͬ�����豸��

entry start										! entry��αָ���ʹ�����������ɵ�ִ�г����а���ָ���ı�ʶ�����ţ������ǳ���Ŀ�ʼִ�е㡣
start:
				mov 	ax, #BOOTSEG		! ��ds�μĴ�������Ϊ0x7c0��
				mov 	ds, ax
				mov 	ax, #INITSEG		! INITSEG = 0x9000
				mov 	es, ax
				mov 	cx #256					! �����ƶ�����ֵ=256�֣�512�ֽڣ�
				sub 	si, si					! Դ��ַ��0x7c00:0x0000
				sub 	di, di					! Ŀ�ĵ�ַ��0x9000:0x0000
				rep
				movw
				jmpi 	go, INITSEG			! �μ���ת������INITSEGָ����ת���Ķε�ַ�����go�Ƕ���ƫ�Ƶ�ַ��

go:			mov 	ax, cs
				mov 	dx, #0xfef4
				
				mov 	ax, ds
				mov 	es, ax
				push 	ax							! ���������ʼ��ÿ���Ȩʱ��ss = 0x00��sp = 0xfffe������ջ�ڿ�ʼʱ���Ǵ��ڵġ�
				
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
 
 
 				push 	#0							! set fs = 0��
 				pop 	fs
 				mov 	bx, #0x78				! fs:bx is parameter table adress
 				seg 	fs							! αָ��seg��ʾ����һ�����Ĳ�������fs����
 				lgs		si, (bx)				! fs--->gs, bx--->si; so (gs:si) is source
 				
 				mov 	di, dx					! (es:di) is destination! Ϊʲô��ָ���˼Ĵ���es���أ�
 				mov 	cx, #6					! copy 12 bytes
 				cld										! �巽���־������ʱָ�������
 				
 				rep
 				seg 	gs
 				movw
 				
 				mov 	di, dx					! ֮������93�к���һ�θ�ֵ������Ϊ��copy�����У�di��ֵ�����ˣ�����Ҫ�ָ���ԭ����dx�е�ֵ
 				movb 	4(di), *18			! 18--->(es:di+4)����ʾ�޸�ÿ�ŵ������������Ϊ18��
 				
 				seg 	fs
 				mov 	(bx), di				! (di)--->(fs:bx)�������ж�����0x1E��ֵָ���±���Ҳ����0x1E����ŵĵ�ַ����±��ĵ�ַ
 				seg 	fs
 				mov 	2(bx), es				! (es)--->(fs:bx+2)
 				
 				pop 	ax							! ax����65�е�ѹջֵ0x9000��
 				mov 	fs, ax
 				mov 	gs, ax
 				
 				xor 	ah, ah					! ��λ���̿���������������²���
 				xor 	dl, dl					! dl = 0����һ��������
 				int 	0x13						! BIOS�жϣ�ȡ���̲������е�ǰ���������̵Ĳ�����113��114��Ӧ�ö��Ǹ�����ж��õġ�

! load the setup-sectors directly after the bootblock.
! Note that 'es' is already set up.

load_setup:
				xor 	dx, dx									! dh = ��ͷ�� = 0��dl = �������� = 0
				mov 	cx, #0x0002							! ch = �ŵ������棩�ĵ�8λ��cl = ��ʼ������λ0~5�����ŵ��Ÿ�2λ��λ6~7��
				mov 	bx, #0x0200							! address = 512, in INITSEG
				mov 	ax, #0x0200+SETUPLEN		! ah = 0x02 - �������������ڴ棻al = ��Ҫ����������������
				int 	0x13										! ��ʼ����
				jnc 	ok_load_setup						! �������ִ��
				
				push 	ax											! ��ʾ������Ϣ����������ջ���Ʋ������������ax�У��Ͼ�ax�����淵��ֵ��
				call 	print_nl								! ��Ļ���س�
				mov 	bp, sp 									! ss:bpָ������ʾ���֣�sp�������Ķ������ǿ���ͨ��bp�����ʶ�ջ�е�ֵ��
				call 	print_hex 							! ��ʾ16����ֵ��
				pop 	ax
				
				xor 	dl, dl
				xor 	ah, ah
				int 	0x13
				j			load_setup

ok_load_setup:

! Get disk drive parameters, specifically nr of sectors/track

				xor 	dl, dl									! ��δ�����BIOS��0x13�жϵĹ���8��ȡ�����������Ĳ���
				mov 	ah, #0x08
				int 	0x13
				xor 	ch, ch
				seg 	cs
				mov 	sectors, cx
				mov 	ax, #INITSEG
				mov 	es, ax

! Print some inane message

				mov 	ah, #0x03								! BIOS�ж�0x10�����ܺ�ah = 0x03�������λ�ã������ع��λ�õ�dx�С�
				xor 	bh, bh									! ����bh = ҳ�š�
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
				call 	kill_motor							! �ر����������
				call 	print_nl								! ���س����С�

! After that we check which root-device to use. If the device is
! defined (!= 0), nothing is done and the given device is used.
! Otherwise, either /dev/PS0 (2,28) or /dev/at0 (2,8), depending
! on the number of sectors that the BIOS reports currently.

				seg 	cs
				mov 	ax, root_dev						! ȡ508��509�ֽڴ��ĸ��豸��
				or 		ax, ax									! �жϸ��豸�Ƿ��Ѷ��壬Ϊ0��û�ж��壬��Ҫ����BIOS�����ÿ�ŵ���������ȷ��ʹ��
				jne 	root_defined						! �ĸ��豸
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

				jmp 	0, SETUPSEG							! ��ͽ����ˡ�

! This routine loads the system at address 0x10000, making sure
! no 64kB boundaries are crossed. We try to load it as fast as
! possible, loading whole tracks whenever we can.

! in: es - starting address segment (normally 0x1000)

sread: 	.word 1+SETUPLEN							! ��ǰ�ŵ����Ѷ�������
head:		.word 0												! ��ǰ��ͷ��
track:	.word 0												! ��ǰ�ŵ���

read_it:
				mov 	ax, es									! test�������������������Ӧ�ı���λ��Ϊ1�������Ķ�Ӧ����λΪ1����֮Ϊ0��
				test 	ax, #0x0fff							! ��test 0x1000, 0x0fff����ô�������0x0000��ZF����λ��������һ��jne�������� 
die:		jne 	die
				xor 	bx, bx									! bxΪ����ƫ�ơ�
rp_read:
				mov 	ax, es
				cmp 	ax, #ENDSEG							! es����϶�����µģ�������������es�͸���Ϊ���������es�Ѿ����˽����ĵط�����ô�ͷ��ء�
				jb 		ok1_read								! �����û�е�����ô����ת��ok1_read��������
				ret
ok1_read:
				seg 	cs
				mov 	ax, sectors							! sectors�Ǳ�ţ�������ַ���洢ÿ�ŵ�������������ô��һ�е�seg cs����ָ���μĴ���ָ��Ϊcs������0x9000��
				sub 	ax, sread								! ��ȥ�Ѿ����˵���������
				mov 	cx, ax									! ����
				shl 	cx, #9									! ����9λ����512����������������ֽ���������bx�д洢�ĵ�ǰƫ��ֵ������ʾ�˴ζ������󣬶��ڹ�������ֽ�����
				add 	cx, bx
				jnc 	ok2_read								! ��û�г���64KB����һ���εĳ��ȣ����������ת��ok2_read��ִ�У���Ϊcx��16λ�Ĵ�������󼴱�ʾ64KB��
				je 		ok2_read								! ����޽�λ����jnc������������ת��˳������ִ�С���Ȼ�պõ���64KBҲת�ơ�
				xor 	ax, ax									! ���ǿ��������64KB�Ķ��л��ж����ֽڿ����������룬�ٽ���ת����Ϊ����������ax�С�
				sub 	ax, bx
				shr 	ax, #9
ok2_read:
				call 	read_track							! ����ǰ�ŵ���ָ����ʼ������cl�������������Ŀ��al�������ݡ�
				mov 	cx, ax									! cx�д洢���Ǳ��ζ���������Ҫ��ȡ����������
				add 	ax, sread								! �����Ѿ���ȡ����������
				seg 	cs
				cmp 	ax, sectors							! ���ܹ�Ҫ������������ȣ���0����ת
				jne		ok3_read
				mov 	ax, #1
				sub 	ax, head								! head = 0����ʼֵ����
				jne		ok4_read								! �����0��ͷ������ȥ��1��ͷ���ϵ��������ݣ���0����ת��
				inc 	track										! �������һ�ŵ���
ok4_read:
				mov 	head, ax
				xor 	ax, ax									! ��յ�ǰ�ŵ��Ѷ���������
ok3_read:
				mov 	sread, ax								! ���浱ǰ�ŵ��Ѷ���������
				shl 	cx, #9
				add 	bx, cx									! ���С��64KB�ı߽�ֵ������ת��rp_read���������ݡ�
				jnc 	rp_read
				mov 	ax, es
				add 	ah, #0x10								! ��es�еĶλ�ַ����64KB����ʵ��������ǽ����8λ��0x10���Ӷ���λ����һ�����С�
				mov 	es, ax
				xor 	bx, bx
				jmp 	rp_read

read_track:
				pusha													! ѹ�����еļĴ�����push all��
				pusha													! Ϊ������ʾ�ж�ѹ�����мĴ���ֵ��ʲô����
				mov 	ax, #0xe2e
				mov 	bx, #7
				int 	0x10										! ����BIOS�ж�0x10������0x0e�����ǰ��һλ�ã���ʾһ����.��
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
 				jae 	no_reg									! ��CF = 0����ʾ�Ĵ�������������ת��
 				
 				mov 	ax, #0xe05 + 0x41 -1		! ah =���ܺţ�0x0e����al =�ַ���0x05 + 0x41 -1����
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