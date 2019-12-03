!
! setup.s (C) 1991 Linus Torvalds
!
! setup.s is responsible for getting the system data from the BIOS,
! and putting them into the appropriate places in system memory.
! both setup.s and system has been loaded by the bootblock.
!
! This code asks the bios for memory/disk/other parameters, and
! puts them in a "safe" place: 0x90000-0x901FF, ie where the
! boot-block used to be. It is then up to the protected mode
! system to read them from there before the area is overwritten
! for buffer-blocks.
!

! NOTE! These had better be the same as in bootsect.s!
#include <linux/config.h>

INITSEG		= DEF_INITSEG										! ԭ��bootsect���ڵĶ�
SYSSEG 		= DEF_SYSSEG										! system��0x10000��
SETUPSEG 	= DEF_SETUPSEG									! ���������ڵĶε�ַ

.globl begtext, begdata, begbss, endtext, enddata, endbss
.text
begtext
.data
begdata
.bss
begbss
.text

entry start
start:

! ok, the read went well so we get current cursor position and save it for
! posterity.

				mov 	ax, #INITSEG
				mov 	ds, ax

! Get memory size��ʹ��BIOS�ж�0x15�ĵ�0x88�Ź�����ȡϵͳ��չ�ڴ��С��������0x90002����

				mov 	ah, #0x88				
				int 	0x15
				mov 	[2], ax											! ����ֵax = ��0x100000��1M������ʼ����չ�ڴ��С

! check for EGA/VGA and some config parameters

				mov 	ah, #0x12
				mov 	bl, #0x10
				int 	0x10												! BIOS�ж�0x10.�����ʾ�ڴ���Ϣ�ȡ�
				mov 	[8], ax											! 0x90008 = ��
				mov 	[10], bx										! 0x9000A = ��װ����ʾ�ڴ棻0x9000B = ��ʾ״̬����/��ɫ��
				mov 	[12], cx										! 0x9000C = �Կ����Բ�����
				mov 	ax, #0x5019
				cmp 	bl, #0x10
				je		novga
				call 	chsvga
novga:	mov 	[14], ax
				mov 	ah, #0x03
				xor		bh, bh
				int 	0x10
				mov 	[0], dx

! ȡ��ʾ����ǰ����ʾģʽ��

				mov 	ah, #0x0f										! ah = �ַ�������al = ��ʾģʽ��bh = ��ǰ��ʾҳ��
				int 	0x10
				mov 	[4], bx											! 0x90004(1 ��)��ŵ�ǰҳ��0x90006 �����ʾģʽ��0x90007 ����ַ�������
				mov 	[6], ax

! ȡ��һ��Ӳ�̵���Ϣ

				mov 	ax, #0x0000
				mov 	ds, ax
				lds 	si, [4*0x41]								! ȡ�ж�����0x41��ֵ����hd0�������ĵ�ַ >> ds:si
				mov 	ax, #INISEG
				mov 	es, ax
				mov 	di, #0x0080									! �����Ŀ�ĵأ�es:di = 0x9000:0x0080 = 0x90080
				mov 	cx, #0x10										! ������ֽ���
				rep
				movsb

! Get hd1 data

				mov 	ax, #0x0000
				mov 	ds, ax
				lds 	si, [4*0x46]								! ȡ�ж�����0x46��ֵ����hd1�������ĵ�ַ >> ds:si
				mov 	ax, #INITSEG
				mov 	es, ax
				mov 	di, #0x0090
				mov 	cx, #0x10
				rep
				movsb

! ���ϵͳ�Ƿ��е�2��Ӳ�̣����û����ѵڶ�������0

				mov 	ax, #0x01500
				mov 	dl, #0x81										! dl = ������
				int 	0x13
				jc 		no_disk1
				cmp 	ah, #3
				je 		is_disk1
no_disk1:
				mov 	ax, #INITSEG
				mov 	es, ax
				mov 	di, #0x0090
				mov 	cx, #0x10
				mov 	ax, #0x00
				rep
				stosb															! ��al�е����ݴ洢��es:di��ȥ
is_disk1:

! ����Ҫ���뵽����ģʽ����

				cli																! ���ȹ��ж�

! ���Ƚ�systemת�Ƶ���ȷ��λ��

				mov		ax, #0x0000
				cld																! ��������־�����߳���si��di��ǰ�ƶ���stdΪ����ƶ�
do_move:
				mov 	es, ax
				add 	ax, #0x1000
				cmp 	ax, #0x9000
				jz		end_move
				mov 	ds, ax
				sub 	di, di
				sub 	si, si
				mov 	cx, #0x8000
				rep
				movsw
				jmp 	do_move

! �˺����Ǽ��ض�������

end_move:
				mov 	ax, #SETUPSEG
				mov 	ds, ax
				lidt	idt_48
				lgdt 	gdt_48

! ����A20��ַ��

				call 	empty_8042									! ����8042״̬�Ĵ������ȴ����뻺�����գ�Ϊ�ղſ���ִ��д����
				mov 	al, #0xD1
				out 	#0x64, al
				call 	empty_8042
				mov 	al, #0xDF
				out 	#0x60, al
				call 	empty_8042

! well, that went ok, I hope. Now we have to reprogram the interrupts :-(
! we put them right after the intel-reserved hardware interrupts, at
! int 0x20-0x2F. There they won't mess up anything. Sadly IBM really
! messed this up with the original PC, and they haven't been able to
! rectify it afterwards. Thus the bios puts interrupts at 0x08-0x0f,
! which is used for the internal hardware interrupts as well. We just
! have to reprogram the 8259's, and it isn't fun.

				mov 	al, 0x11										! ��δ����Ƕ��жϿ���������ʼ��������
				out 	#0x20, al										! �����ϸ����Ҫ�鿴оƬ�ֲᡣ
				.word 0x00eb, 0x00eb
				out 	#0xA0, al
				.word 0x00eb, 0x00eb
				mov 	al, #0x20
				out 	#0x21, al
				.word 0x00eb, 0x00eb
				mov 	al, #0x28
				out 	#0xA1, al
				.word 0x00eb, 0x00eb
				mov 	al, #0x04
				out 	#0x21, al
				.word 0x00eb, 0x00eb
				mov 	al, #0x02
				out 	#0xA1, al
				.word 0x00eb, 0x00eb
				mov 	al, #0x01
				out 	#0x21, al
				.word 0x00eb, 0x00eb
				out 	#0xA1, al
				.word 0x00eb, 0x00eb
				mov 	al, #0xFF
				out 	#0x21, al
				.word 0x00eb, 0x00eb
				out 	#0xA1, al

! well, that certainly wasn't fun :-(. Hopefully it works, and we don't
! need no steenking BIOS anyway (except for the initial loading :-).
! The BIOS-routine wants lots of unnecessary data, and it's less
! "interesting" anyway. This is how REAL programmers do it.
!
! Well, now's the time to actually move into protected mode. To make
! things as simple as possible, we do no register set-up or anything,
! we let the gnu-compiled 32-bit programs do that. We just jump to
! absolute address 0x00000, in 32-bit protected mode.

				mov 	ax, #0x0001
				lmsw	ax
				jmpi	0, 8												! ����ģʽ�ĵ�һ��Ѱַָ�

! This routine checks that the keyboard command queue is empty
! No timeout is used - if this hangs there is something wrong with
! the machine, and we probably couldn't proceed anyway.
empty_8042:
				.word 0x00eb, 0x00eb
				in 		al, 0x64
				test 	al, #2											! ֻ�е����뻺����Ϊ��ʱ�����̿�����״̬�Ĵ���λ 1 = 0���ſ��Զ���ִ��д����
				jnz 	empty_8042
				ret

! Routine trying to recognize type of SVGA-board present (if any)
! and if it recognize one gives the choices of resolution it offers.
! If one is found the resolution chosen is given by al,ah (rows,cols).

chsvga: cld
				push 	ds 													! ���� ds ֵ������ 231 �У��� 490 �� 492 �У�������
				push 	cs 													! ��Ĭ�����ݶ����óɺʹ����ͬһ���Ρ�
				pop 	ds
				mov 	ax, #0xc000
				mov 	es, ax 											! es ָ�� 0xc000 �Ρ��˴��� VGA ���ϵ� ROM BIOS ����
				lea 	si, msg1 										! ds:si ָ�� msg1 �ַ�����
				call 	prtstr 											! ��ʾ�� NULL ��β�� msg1 �ַ�����
nokey:	in 		al, #0x60
				cmp 	al, #0x82
				jb 		nokey 											! ��С�� 0x82����ʾ��û�а����ɿ���
				cmp 	al,#0xe0
				ja 		nokey 											! ������ 0xe0����ʾ�յ�������չɨ����ǰ׺��
				cmp 	al, #0x9c 									! ���Ͽ����� 0x9c����ʾ�û�����/�ɿ��˻س�����
				je 		svga 												! ���ǳ�����תȥ���ϵͳ�Ƿ���� SVGA ģʽ��
				mov 	ax, #0x5019 								! ��������Ĭ������ֵ AL=25 �С�AH=80 �С�
				pop 	ds
				ret
svga: 	lea 	si, idati 									! Check ATI 'clues' ! ����ж� ATI ��ʾ�������ݡ�
				mov 	di, #0x31 									! �������� 0xc000:0x0031 ��ʼ��
				mov 	cx, #0x09 									! �������� 9 ���ֽڡ�
				repe
				cmpsb 														! ��� 9 ���ֽڶ���ͬ����ʾϵͳ����һ�� ATI ����ʾ����
				jne 	noati
				lea 	si, dscati 									! �� dscati ����Ч��ַ���� si��
				lea 	di, moati
				lea 	cx,selmod
				jmp 	cx
noati: 	mov 	ax, #0x200f 								! Check Ahead 'clues'
				mov 	dx, #0x3ce 									! ���ݶ˿�ָ���������Ĵ�����0x0f?0x3ce �˿ڣ���
				out 	dx, ax 											! �����ÿ�����չ�Ĵ�����־��0x20?0x3cf �˿ڣ���
				inc 	dx 													! Ȼ���ٶ�ȡ�üĴ��������ñ�־�Ƿ������ϡ�
				in 		al, dx
				cmp 	al, #0x20 									! �����ȡֵ�� 0x20�����ʾ�� Ahead A ��ʾ����
				je 		isahed 											! �����ȡֵ�� 0x21�����ʾ�� Ahead B ��ʾ����
				cmp 	al, #0x21 									! ����˵������ Ahead ��ʾ����������ת����������࿨��
				jne 	noahed
isahed: lea 	si, dscahead
				lea 	di, moahead
				lea 	cx, selmod
				jmp 	cx
noahed: mov 	dx, #0x3c3 									! Check Chips & Tech. 'clues'
				in 		al, dx 											! �� 0x3c3 �˿ڶ�ȡ VGA �����Ĵ���ֵ�������Ͻ�������ģʽ
				or 		al, #0x10 									! ��־��λ 4������д�ء�
				out 	dx, al
				mov 	dx,	#0x104 									! ������ģʽʱ��ȫ�ֱ�ʶ�˿� 0x104 ��ȡ��ʾ��оƬ��ʶֵ��
				in 		al, dx 											! ����ʱ����� bl �Ĵ����С�
				mov 	bl,	al
				mov 	dx, #0x3c3 									! Ȼ��� 0x3c3 �˿��еĽ�������ģʽ��־��λ��
				in 		al, dx
				and 	al, #0xef
				out  	dx,	al
				cmp 	bl, [idcandt] 							! �ٰ� bl �б�ʶֵ��λ�� idcandt ������ 596 �У��� Chips &
				jne 	nocant 											! Tech �ı�ʶֵ 0xA5 ���Ƚϡ������ͬ����ת�Ƚ���һ���Կ���
				lea 	si, dsccandt
				lea 	di, mocandt
				lea 	cx, selmod
				jmp 	cx
nocant: mov 	dx, #0x3d4 									! Check Cirrus 'clues'
				mov 	al, #0x0c 									! ������ CRT ���ƼĴ����������Ĵ����˿� 0x3d4 д��Ҫ����
				out 	dx,	al 											! �ļĴ��������� 0x0c����Ӧ�Դ���ʼ��ַ���ֽڼĴ�������
				inc 	dx 													! Ȼ��� 0x3d5 �˿ڶ����Դ���ʼ��ַ���ֽڲ��ݴ��� bl �У�
				in 		al,	dx 											! �ٰ��Դ���ʼ��ַ���ֽڼĴ������㡣
				mov 	bl,	al
				xor 	al,	al
				out 	dx,	al
				dec 	dx 													! ������ 0x3d4 �˿�������� 0x1f��ָ������Ҫ�� 0x3d5 �˿�
				mov 	al,	#0x1f 									! ���ʶ�ȡ��Eagle ID���Ĵ������ݡ�
				out 	dx,	al
				inc 	dx
				in 		al,	dx 											! �� 0x3d5 �˿ڶ�ȡ��Eagle ID���Ĵ���ֵ�����ݴ��� bh �С�
				mov 	bh,	al 											! Ȼ��Ѹ�ֵ�ߵ� 4 ���ػ���λ�ô�ŵ� cl �С������� 8 λ
				xor 	ah,	ah 											! ����� ch �У��� cl �з�����ֵ 6��
				shl 	al,	#4
				mov 	cx,	ax
				mov 	al,	bh
				shr 	al,	#4
				add 	cx,	ax
				shl 	cx,	#8
				add 	cx,	#6 											! ���� cx ֵ����� ax �С���ʱ ah ���ǻ�λ��ġ�Eagle
				mov 	ax,	cx 											! ID��ֵ��al ���������� 6����Ӧ����/��չ�Ĵ������� ah
				mov 	dx,	#0x3c4 									! д�� 0x3c4 �˿������Ķ���/��չ�Ĵ���Ӧ�ûᵼ�� Cirrus
				out 	dx,	ax 											! ��ʾ����ֹ��չ���ܡ�
				inc 	dx
				in 		al,	dx 											! �����չ������ı���ֹ����ô��ʱ�����ֵӦ��Ϊ 0��
				and 	al,	al 											! �����Ϊ 0 ���ʾ���� Cirrus ��ʾ������ת���������������
				jnz 	nocirr
				mov 	al,	bh 											! �� Cirrus ��ʾ���������õ� 286 �б����� bh �еġ�Eagle
				out 	dx,	al 											! ID��ԭֵ�����¿��� Cirrus ����չ���ܡ���ʱ��ȡ�ķ���
				in 		al,	dx 											! ֵӦ��Ϊ 1�������ǣ�����Ȼ˵������ Cirrus ��ʾ����
				cmp 	al,	#0x01
				jne 	nocirr
				call 	rst3d4 											! �ָ� CRT ����������ʾ��ʼ��ַ���ֽڼĴ������ݡ�
				lea 	si,	dsccirrus
				lea 	di,	mocirrus
				lea 	cx,	selmod
				jmp 	cx
rst3d4: mov 	dx,	#0x3d4
				mov 	al,	bl
				xor 	ah,	ah
				shl 	ax,	#8
				add 	ax,	#0x0c
				out 	dx,	ax 											! ע�⣬���� word ������� al ?0x3d4��ah ?0x3d5��
				ret
nocirr: call 	rst3d4 											! Check Everex 'clues'
				mov 	ax,	#0x7000 								! ���� ax = 0x7000, bx=0x0000������ int 0x10��
				xor 	bx,	bx
				int 	0x10
				cmp 	al,	#0x70 									! ���� Everes ��ʾ����al ��Ӧ�÷���ֵ 0x70��
				jne 	noevrx
				shr 	dx,	#4 											! ���ɰ������ţ�λ 3-0����
				cmp 	dx,	#0x678 									! �����ͺ��� 0x678 ��ʾ��һ�� Trident ��ʾ��������ת��
				je 		istrid
				cmp 	dx,	#0x236 									! �����ͺ��� 0x236 ��ʾ��һ�� Trident ��ʾ��������ת��
				je 		istrid
				lea 	si,	dsceverex
				lea 	di,	moeverex
				lea 	cx,	selmod
				jmp 	cx
istrid: lea 	cx,	ev2tri 									! �� Trident ���͵� Everex ��ʾ��������ת�� ev2tri ������
				jmp 	cx
noevrx: lea 	si,	idgenoa 								! Check Genoa 'clues'
				xor 	ax,	ax 											! �� ds:si ָ��� 597 ���ϵ��������ִ���
				seg 	es
				mov 	al,	[0x37] 									! ȡ VGA ���� BIOS �� 0x37 ����ָ�루��ָ������������
				mov 	di,	ax 											! ��˴�ʱ es:di ָ���������ִ���ʼ����
				mov 	cx,	#0x04
				dec 	si
				dec 	di
l1: 		inc 	si 													! Ȼ��ѭ���Ƚ��� 4 ���ֽڵ��������ִ���
				inc 	di
				mov 	al,	(si)
				seg 	es
				and 	al,	(di)
				cmp 	al,	(si)
				loope l1
				cmp 	cx,	#0x00 									! ����������ִ���ȫ��ͬ�����ʾ�� Genoa ��ʾ����
				jne 	nogen
				lea 	si,	dscgenoa
				lea 	di,	mogenoa
				lea 	cx,	selmod
				jmp 	cx
nogen: 	lea 	si, idparadise 							! Check Paradise 'clues'
				mov 	di, #0x7d 									! es:di ָ�� VGA ROM BIOS �� 0xc000:0x007d �����ô�Ӧ����
				mov 	cx,	#0x04 									! 4 ���ַ���VGA=����
				repe
				cmpsb
				jne 	nopara 											! ���в�ͬ���ַ�����ʾ���� Paradise ��ʾ����������ת��
				lea 	si,	dscparadise 						! ������ si ָ�� Paradise ��ʾ���Ŀ�ѡ����ֵ������ di ָ
				lea 	di,	moparadise 							! ����չģʽ������ģʽ���б���Ȼ����ת�� selmod ��ȥѡ
				lea 	cx,	selmod 									! ����Ҫʹ�õ���ʾģʽ��
				jmp 	cx
nopara: mov 	dx, #0x3c4 									! Check Trident 'clues'
				mov 	al, #0x0e 									! �����ڶ˿� 0x3c4 ��������� 0x0e������ģʽ���ƼĴ��� 1��
				out 	dx,	al 											! Ȼ��� 0x3c5 ���ݶ˿ڶ���üĴ���ԭֵ�����ݴ��� ah �С�
				inc 	dx
				in 		al,	dx
				xchg 	ah,	al
				mov 	al,	#0x00 									! Ȼ��������üĴ���д�� 0x00���ٶ�ȡ��ֵ?al��
				out 	dx,	al 											! д�� 0x00 ���൱�ڡ�ԭֵ��0x02 ��� 0x02 ���д��ֵ��
				in 		al,	dx 											! ������� Trident ��ʾ������˺�����ֵӦ���� 0x02��
				xchg 	al,	ah 											! ������al=ԭģʽ���ƼĴ��� 1 ��ֵ��ah=����ȡ��ֵ��
				mov 	bl, al 											! Strange thing ... in the book this wasn't
				and 	bl,	#0x02 									! necessary but it worked on my card which
				jz 		setb2 											! is a trident. Without it the screen goes
				and 	al,	#0xfd 									! blurred ...
				jmp 	clrb2 											!
setb2: 	or 		al,	#0x02 									!
clrb2: 	out 	dx,	al
				and 	ah,	#0x0f 									! ȡ 375 ��������ֵ��ҳ������ֶΣ�λ 3--0�������
				cmp 	ah,	#0x02 									! ���ֶ�ֵ���� 0x02�����ʾ�� Trident ��ʾ����
				jne 	notrid
ev2tri: lea 	si,	dsctrident
				lea 	di,	motrident
				lea 	cx,	selmod
				jmp 	cx
notrid: mov 	dx,	#0x3cd 									! Check Tseng 'clues'
				in 		al,	dx 											! Could things be this simple ! :-)
				mov 	bl,	al 											! �ȴ� 0x3cd �˿ڶ�ȡ��ѡ��Ĵ���ԭֵ���������� bl �С�
				mov 	al,	#0x55 									! Ȼ��������üĴ�����д�� 0x55���ٶ��벢���� ah �С�
				out 	dx,	al
				in 		al,	dx
				mov 	ah,	al
				mov 	al,	bl 											! ���Żָ��üĴ�����ԭֵ��
				out 	dx,	al
				cmp 	ah,	#0x55 									! �����ȡ�ľ�������д���ֵ��������� Tseng ��ʾ����
				jne 	notsen
				lea 	si,	dsctseng 								! ������ si ָ�� Tseng ��ʾ���Ŀ�ѡ����ֵ���б����� di
				lea 	di,	motseng 								! ָ���Ӧ��չģʽ������ģʽ���б���Ȼ����ת�� selmod
				lea 	cx,	selmod 									! ȥִ��ģʽѡ�������
				jmp 	cx
notsen: mov 	dx,	#0x3cc 									! Check Video7 'clues'
				in 		al,	dx
				mov 	dx,	#0x3b4 									! ������ dx Ϊ��ɫ��ʾ CRT ���������Ĵ����˿ں� 0x3b4��
				and 	al,	#0x01 									! ����������Ĵ�����λ 0 ���� 0����ɫ����ֱ����ת��
				jz 		even7 											! ���� dx ����Ϊ��ɫ��ʾ CRT ���������Ĵ����˿ں� 0x3d4��
				mov 	dx,	#0x3d4
even7: 	mov 	al,	#0x0c 									! ���üĴ���������Ϊ 0x0c����Ӧ�Դ���ʼ��ַ���ֽڼĴ�����
				out 	dx,	al
				inc 	dx
				in 		al,	dx 											! ��ȡ��ʾ�ڴ���ʼ��ַ���ֽڼĴ������ݣ��������� bl �С�
				mov 	bl,	al
				mov 	al,	#0x55 									! Ȼ�����Դ���ʼ��ַ���ֽڼĴ�����д��ֵ 0x55���ٶ�ȡ������
				out 	dx,	al
				in 		al,	dx
				dec 	dx 													! Ȼ��ͨ�� CRTC �����Ĵ����˿� 0x3b4 �� 0x3d4 ѡ����������
				mov 	al,	#0x1f 									! 0x1f �� Video7 ��ʾ����ʶ�Ĵ������üĴ�������ʵ���Ͼ���
				out 	dx,	al 											! �Դ���ʼ��ַ���ֽں� 0xea ������������Ľ��ֵ��
				inc 	dx
				in 		al,	dx 											! ��ȡ Video7 ��ʾ����ʶ�Ĵ���ֵ���������� bh �С�
				mov 	bh,	al
				dec 	dx 													! Ȼ����ѡ���Դ���ʼ��ַ���ֽڼĴ������ָ���ԭֵ��
				mov 	al,	#0x0c
				out 	dx,	al
				inc 	dx
				mov 	al,	bl
				out 	dx,	al
				mov 	al,	#0x55 									! �����������֤��Video7 ��ʾ����ʶ�Ĵ���ֵ�����Դ���ʼ
				xor 	al,	#0xea 									! ��ַ���ֽں� 0xea ������������Ľ��ֵ������� 0x55
				cmp 	al,	bh 											! �� 0xea �����������Ľ����Ӧ�õ��ڱ�ʶ�Ĵ����Ĳ���ֵ��
				jne 	novid7 											! ������ Video7 ��ʾ����������Ĭ����ʾ����ֵ��492 �У�
				lea 	si,	dscvideo7
				lea		di,	movideo7
selmod: push 	si
				lea 	si,	msg2
				call 	prtstr
				xor 	cx,	cx
				mov 	cl,	(di) 										! ��ʱ cl ���Ǽ�������ʾ������չģʽ������
				pop		si
				push 	si
				pish 	cx
tbl: 		pop 	bx 													! bx = ��ʾ������չģʽ�ܸ�����
				push 	bx
				mov 	al,	bl
				sub 	al,	cl
				call 	dprnt 											! ��ʮ���Ƹ�ʽ��ʾ al �е�ֵ��
				call 	spcing 											! ��ʾһ�����ٿ� 4 ���ո�
				lodsw 														! �� ax �м��� si ָ�������ֵ����� si ָ����һ�� word ֵ��
				xchg 	al,	ah 											! ����λ�ú� al = ������
				call 	dprnt 											! ��ʾ������
				xchg 	ah,	al 											! ��ʱ al ��������ֵ��
				push 	ax
				mov 	al,	#0x78 									! ��ʾһ��С��x�������˺š�
				call 	prnt1
				pop 	ax 													! ��ʱ al ��������ֵ��
				call 	dprnt 											! ��ʾ������
				call 	docr 												! �س����С�
				loop 	tbl 												! ����ʾ��һ������ֵ��cx ����չģʽ����ֵ�ݼ� 1��
				pop 	cx
				call 	docr
				lea 	si, msg3
				call 	prtstr
				pop 	si
				add 	cl, #0x80
nonum:	in 		al, #0x60
				cmp 	al, #0x82
				jb 		nonum
				cmp 	al, #0x8b
				je 		zero
				cmp 	al, cl
				ja 		nonum
				jmp 	nozero
zero:		sub 	al, #0x0a
nozero:	sub 	al, #0x80
				dec 	al
				xor		ah, ah
				add 	di, ax
				inc 	di
				push 	ax
				mov 	al, (di)
				int 	0x10
				pop 	ax
				shl 	ax. #1
				add 	si, ax
				lodsw
				pop 	ds
				ret
novid7:	pop 	ds
				mov 	ax, #0x5019
				ret

! Routine that 'tabs' to next col.

spcing:	mov 	al, #0x2c
				call 	prnt1
				mov 	al, #0x20
				call 	prnt1
				mov 	al, #0x20
				call 	prnt1
				mov 	al, #0x20
				call 	prnt1
				mov 	al, #0x20
				call 	prnt1
				ret

! Routine to print asciiz-string at DS:SI

prtstr:	lodsb
				and 	al, al
				jz		fin
				call 	prnt1
				jmp 	prtstr
fin:		ret

! Routine to print a decimal value on screen, the value to be
! printed is put in al (i.e 0-255).

dprnt:	push 	ax
				push 	cx
				mov 	ah, #0x00
				mov 	cl, #0x0a
				idiv	cl
				cmp		al, #0x09
				jbc		lt100
				call 	dprnt
				jmp		skip10
lt100:	add 	al, #0x30
				call 	prnt1
skip10:	mov 	al, ah
				add 	al, #0x30
				call 	prnt1
				pop 	cx
				pop 	ax
				ret

! Part of above routine, this one just prints ascii al

prnt1:	push 	ax
				push 	cx
				mov 	bh, #0x00
				mov 	cx, #0x01
				mov 	ah, #0x0e
				int 	0x10
				pop 	cx
				pop 	ax
				ret

! Prints <CR> + <LF> ! ��ʾ�س�+���С�

docr:		push 	ax
				push 	cx
				mov 	bh, #0x00
				mov 	ah, #0x0e
				mov 	al, #0x0a
				mov 	cx, #0x01
				int 	0x10
				mov 	al, #0x0d
				int 	0x10
				pop 	cx
				pop		ax
				ret

gdt:
				.word 0, 0, 0, 0									! ��һ��������������

				.word	0x07FF											! 
				.word 0x0000
				.word 0x9A00
				.word 0x00C0

				.word 0x07FF
				.word 0x0000
				.word 0x9200
				.word 0x00C0

idt_48:
				.word	0
				.word 0, 0

gdt_48:
				.word 0x800
				.word 512+gdt, 0x9									! 0x0009<<16+0x0200+gdt�������õ��Ǿ���������ַ

msg1: 				.ascii "Press <RETURN> to see SVGA-modes available or any other key to continue."
									db 0x0d, 0x0a, 0x0a, 0x00
msg2: 				.ascii "Mode: COLSxROWS:"
									db 0x0d, 0x0a, 0x0a, 0x00
msg3: 				.ascii "Choose mode by pressing the corresponding number."
									db 0x0d, 0x0a, 0x00

idati: 				.ascii 	"761295520"
idcandt: 			.byte 	0xa5 									! ��� idcandt ��˼�� ID of Chip AND Tech.
idgenoa: 			.byte 	0x77, 0x00, 0x66, 0x99
idparadise: 	.ascii 	"VGA="

! Manufacturer: 		Numofmodes: 		Mode:

moati: 							.byte 0x02, 		0x23, 0x33
moahead: 						.byte 0x05, 		0x22, 0x23, 0x24, 0x2f, 0x34
mocandt: 						.byte 0x02, 		0x60, 0x61
mocirrus: 					.byte 0x04, 		0x1f, 0x20, 0x22, 0x31
moeverex: 					.byte 0x0a, 		0x03, 0x04, 0x07, 0x08, 0x0a, 0x0b, 0x16, 0x18, 0x21, 0x40
mogenoa: 						.byte 0x0a, 		0x58, 0x5a, 0x60, 0x61, 0x62, 0x63, 0x64, 0x72, 0x74, 0x78
moparadise: 				.byte 0x02, 		0x55, 0x54
motrident: 					.byte 0x07, 		0x50, 0x51, 0x52, 0x57, 0x58, 0x59, 0x5a
motseng: 						.byte 0x05, 		0x26, 0x2a, 0x23, 0x24, 0x22
movideo7: 					.byte 0x06, 		0x40, 0x43, 0x44, 0x41, 0x42, 0x45

! 									msb = Cols 			lsb = Rows:

dscati: 			.word 0x8419, 0x842c 															! ATI ���������С���ֵ��
dscahead: 		.word 0x842c, 0x8419, 0x841c, 0xa032, 0x5042 			! Ahead ��������ֵ��
dsccandt: 		.word 0x8419, 0x8432
dsccirrus: 		.word 0x8419, 0x842c, 0x841e, 0x6425
dsceverex: 		.word 0x5022, 0x503c, 0x642b, 0x644b, 0x8419, 0x842c, 0x501e, 0x641b, 0xa040, 0x841e
dscgenoa: 		.word 0x5020, 0x642a, 0x8419, 0x841d, 0x8420, 0x842c, 0x843c, 0x503c, 0x5042, 0x644b
dscparadise: 	.word 0x8419, 0x842b
dsctrident: 	.word 0x501e, 0x502b, 0x503c, 0x8419, 0x841e, 0x842b, 0x843c
dsctseng: 		.word 0x503c, 0x6428, 0x8419, 0x841c, 0x842c
dscvideo7: 		.word 0x502b, 0x503c, 0x643c, 0x8419, 0x842c, 0x841c

.text
endtext:
.data
enddata:
.bss
endbss: