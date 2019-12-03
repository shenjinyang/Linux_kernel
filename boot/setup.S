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

INITSEG		= DEF_INITSEG										! 原来bootsect所在的段
SYSSEG 		= DEF_SYSSEG										! system在0x10000处
SETUPSEG 	= DEF_SETUPSEG									! 本程序所在的段地址

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

! Get memory size，使用BIOS中断0x15的第0x88号功能来取系统扩展内存大小并保存在0x90002处。

				mov 	ah, #0x88				
				int 	0x15
				mov 	[2], ax											! 返回值ax = 从0x100000（1M）处开始的扩展内存大小

! check for EGA/VGA and some config parameters

				mov 	ah, #0x12
				mov 	bl, #0x10
				int 	0x10												! BIOS中断0x10.检查显示内存信息等。
				mov 	[8], ax											! 0x90008 = ？
				mov 	[10], bx										! 0x9000A = 安装的显示内存；0x9000B = 显示状态（彩/单色）
				mov 	[12], cx										! 0x9000C = 显卡特性参数。
				mov 	ax, #0x5019
				cmp 	bl, #0x10
				je		novga
				call 	chsvga
novga:	mov 	[14], ax
				mov 	ah, #0x03
				xor		bh, bh
				int 	0x10
				mov 	[0], dx

! 取显示卡当前的显示模式。

				mov 	ah, #0x0f										! ah = 字符列数；al = 显示模式；bh = 当前显示页。
				int 	0x10
				mov 	[4], bx											! 0x90004(1 字)存放当前页；0x90006 存放显示模式；0x90007 存放字符列数。
				mov 	[6], ax

! 取第一个硬盘的信息

				mov 	ax, #0x0000
				mov 	ds, ax
				lds 	si, [4*0x41]								! 取中断向量0x41的值，即hd0参数表的地址 >> ds:si
				mov 	ax, #INISEG
				mov 	es, ax
				mov 	di, #0x0080									! 传输的目的地：es:di = 0x9000:0x0080 = 0x90080
				mov 	cx, #0x10										! 传输的字节数
				rep
				movsb

! Get hd1 data

				mov 	ax, #0x0000
				mov 	ds, ax
				lds 	si, [4*0x46]								! 取中断向量0x46的值，即hd1参数表的地址 >> ds:si
				mov 	ax, #INITSEG
				mov 	es, ax
				mov 	di, #0x0090
				mov 	cx, #0x10
				rep
				movsb

! 检查系统是否有第2个硬盘，如果没有则把第二个表清0

				mov 	ax, #0x01500
				mov 	dl, #0x81										! dl = 驱动号
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
				stosb															! 将al中的内容存储到es:di中去
is_disk1:

! 现在要进入到保护模式中了

				cli																! 首先关中断

! 首先将system转移到正确的位置

				mov		ax, #0x0000
				cld																! 清除方向标志，告诉程序si、di向前移动，std为向后移动
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

! 此后我们加载段描述符

end_move:
				mov 	ax, #SETUPSEG
				mov 	ds, ax
				lidt	idt_48
				lgdt 	gdt_48

! 开启A20地址线

				call 	empty_8042									! 测试8042状态寄存器，等待输入缓冲器空，为空才可以执行写命令
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

				mov 	al, 0x11										! 这段代码是对中断控制器做初始化操作。
				out 	#0x20, al										! 具体的细节需要查看芯片手册。
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
				jmpi	0, 8												! 保护模式的第一条寻址指令。

! This routine checks that the keyboard command queue is empty
! No timeout is used - if this hangs there is something wrong with
! the machine, and we probably couldn't proceed anyway.
empty_8042:
				.word 0x00eb, 0x00eb
				in 		al, 0x64
				test 	al, #2											! 只有当输入缓冲器为空时（键盘控制器状态寄存器位 1 = 0）才可以对其执行写命令
				jnz 	empty_8042
				ret

! Routine trying to recognize type of SVGA-board present (if any)
! and if it recognize one gives the choices of resolution it offers.
! If one is found the resolution chosen is given by al,ah (rows,cols).

chsvga: cld
				push 	ds 													! 保存 ds 值。将在 231 行（或 490 或 492 行）弹出。
				push 	cs 													! 把默认数据段设置成和代码段同一个段。
				pop 	ds
				mov 	ax, #0xc000
				mov 	es, ax 											! es 指向 0xc000 段。此处是 VGA 卡上的 ROM BIOS 区。
				lea 	si, msg1 										! ds:si 指向 msg1 字符串。
				call 	prtstr 											! 显示以 NULL 结尾的 msg1 字符串。
nokey:	in 		al, #0x60
				cmp 	al, #0x82
				jb 		nokey 											! 若小于 0x82，表示还没有按键松开。
				cmp 	al,#0xe0
				ja 		nokey 											! 若大于 0xe0，表示收到的是扩展扫描码前缀。
				cmp 	al, #0x9c 									! 若断开码是 0x9c，表示用户按下/松开了回车键，
				je 		svga 												! 于是程序跳转去检查系统是否具有 SVGA 模式。
				mov 	ax, #0x5019 								! 否则设置默认行列值 AL=25 行、AH=80 列。
				pop 	ds
				ret
svga: 	lea 	si, idati 									! Check ATI 'clues' ! 检查判断 ATI 显示卡的数据。
				mov 	di, #0x31 									! 特征串从 0xc000:0x0031 开始。
				mov 	cx, #0x09 									! 特征串有 9 个字节。
				repe
				cmpsb 														! 如果 9 个字节都相同，表示系统中有一块 ATI 牌显示卡。
				jne 	noati
				lea 	si, dscati 									! 把 dscati 的有效地址放入 si。
				lea 	di, moati
				lea 	cx,selmod
				jmp 	cx
noati: 	mov 	ax, #0x200f 								! Check Ahead 'clues'
				mov 	dx, #0x3ce 									! 数据端口指向主允许寄存器（0x0f?0x3ce 端口），
				out 	dx, ax 											! 并设置开启扩展寄存器标志（0x20?0x3cf 端口）。
				inc 	dx 													! 然后再读取该寄存器，检查该标志是否被设置上。
				in 		al, dx
				cmp 	al, #0x20 									! 如果读取值是 0x20，则表示是 Ahead A 显示卡。
				je 		isahed 											! 如果读取值是 0x21，则表示是 Ahead B 显示卡。
				cmp 	al, #0x21 									! 否则说明不是 Ahead 显示卡，于是跳转继续检测其余卡。
				jne 	noahed
isahed: lea 	si, dscahead
				lea 	di, moahead
				lea 	cx, selmod
				jmp 	cx
noahed: mov 	dx, #0x3c3 									! Check Chips & Tech. 'clues'
				in 		al, dx 											! 从 0x3c3 端口读取 VGA 允许寄存器值，添加上进入设置模式
				or 		al, #0x10 									! 标志（位 4）后再写回。
				out 	dx, al
				mov 	dx,	#0x104 									! 在设置模式时从全局标识端口 0x104 读取显示卡芯片标识值，
				in 		al, dx 											! 并暂时存放在 bl 寄存器中。
				mov 	bl,	al
				mov 	dx, #0x3c3 									! 然后把 0x3c3 端口中的进入设置模式标志复位。
				in 		al, dx
				and 	al, #0xef
				out  	dx,	al
				cmp 	bl, [idcandt] 							! 再把 bl 中标识值与位于 idcandt 处（第 596 行）的 Chips &
				jne 	nocant 											! Tech 的标识值 0xA5 作比较。如果不同则跳转比较下一种显卡。
				lea 	si, dsccandt
				lea 	di, mocandt
				lea 	cx, selmod
				jmp 	cx
nocant: mov 	dx, #0x3d4 									! Check Cirrus 'clues'
				mov 	al, #0x0c 									! 首先向 CRT 控制寄存器的索引寄存器端口 0x3d4 写入要访问
				out 	dx,	al 											! 的寄存器索引号 0x0c（对应显存起始地址高字节寄存器），
				inc 	dx 													! 然后从 0x3d5 端口读入显存起始地址高字节并暂存在 bl 中，
				in 		al,	dx 											! 再把显存起始地址高字节寄存器清零。
				mov 	bl,	al
				xor 	al,	al
				out 	dx,	al
				dec 	dx 													! 接着向 0x3d4 端口输出索引 0x1f，指出我们要在 0x3d5 端口
				mov 	al,	#0x1f 									! 访问读取“Eagle ID”寄存器内容。
				out 	dx,	al
				inc 	dx
				in 		al,	dx 											! 从 0x3d5 端口读取“Eagle ID”寄存器值，并暂存在 bh 中。
				mov 	bh,	al 											! 然后把该值高低 4 比特互换位置存放到 cl 中。再左移 8 位
				xor 	ah,	ah 											! 后放入 ch 中，而 cl 中放入数值 6。
				shl 	al,	#4
				mov 	cx,	ax
				mov 	al,	bh
				shr 	al,	#4
				add 	cx,	ax
				shl 	cx,	#8
				add 	cx,	#6 											! 最后把 cx 值存放入 ax 中。此时 ah 中是换位后的“Eagle
				mov 	ax,	cx 											! ID”值，al 中是索引号 6，对应定序/扩展寄存器。把 ah
				mov 	dx,	#0x3c4 									! 写到 0x3c4 端口索引的定序/扩展寄存器应该会导致 Cirrus
				out 	dx,	ax 											! 显示卡禁止扩展功能。
				inc 	dx
				in 		al,	dx 											! 如果扩展功能真的被禁止，那么此时读入的值应该为 0。
				and 	al,	al 											! 如果不为 0 则表示不是 Cirrus 显示卡，跳转继续检查其他卡。
				jnz 	nocirr
				mov 	al,	bh 											! 是 Cirrus 显示卡，则利用第 286 行保存在 bh 中的“Eagle
				out 	dx,	al 											! ID”原值再重新开启 Cirrus 卡扩展功能。此时读取的返回
				in 		al,	dx 											! 值应该为 1。若不是，则仍然说明不是 Cirrus 显示卡。
				cmp 	al,	#0x01
				jne 	nocirr
				call 	rst3d4 											! 恢复 CRT 控制器的显示起始地址高字节寄存器内容。
				lea 	si,	dsccirrus
				lea 	di,	mocirrus
				lea 	cx,	selmod
				jmp 	cx
rst3d4: mov 	dx,	#0x3d4
				mov 	al,	bl
				xor 	ah,	ah
				shl 	ax,	#8
				add 	ax,	#0x0c
				out 	dx,	ax 											! 注意，这是 word 输出！！ al ?0x3d4，ah ?0x3d5。
				ret
nocirr: call 	rst3d4 											! Check Everex 'clues'
				mov 	ax,	#0x7000 								! 设置 ax = 0x7000, bx=0x0000，调用 int 0x10。
				xor 	bx,	bx
				int 	0x10
				cmp 	al,	#0x70 									! 对于 Everes 显示卡，al 中应该返回值 0x70。
				jne 	noevrx
				shr 	dx,	#4 											! 忽律板修正号（位 3-0）。
				cmp 	dx,	#0x678 									! 板类型号是 0x678 表示是一块 Trident 显示卡，则跳转。
				je 		istrid
				cmp 	dx,	#0x236 									! 板类型号是 0x236 表示是一块 Trident 显示卡，则跳转。
				je 		istrid
				lea 	si,	dsceverex
				lea 	di,	moeverex
				lea 	cx,	selmod
				jmp 	cx
istrid: lea 	cx,	ev2tri 									! 是 Trident 类型的 Everex 显示卡，则跳转到 ev2tri 处理。
				jmp 	cx
noevrx: lea 	si,	idgenoa 								! Check Genoa 'clues'
				xor 	ax,	ax 											! 让 ds:si 指向第 597 行上的特征数字串。
				seg 	es
				mov 	al,	[0x37] 									! 取 VGA 卡上 BIOS 中 0x37 处的指针（它指向特征串）。
				mov 	di,	ax 											! 因此此时 es:di 指向特征数字串开始处。
				mov 	cx,	#0x04
				dec 	si
				dec 	di
l1: 		inc 	si 													! 然后循环比较这 4 个字节的特征数字串。
				inc 	di
				mov 	al,	(si)
				seg 	es
				and 	al,	(di)
				cmp 	al,	(si)
				loope l1
				cmp 	cx,	#0x00 									! 如果特征数字串完全相同，则表示是 Genoa 显示卡，
				jne 	nogen
				lea 	si,	dscgenoa
				lea 	di,	mogenoa
				lea 	cx,	selmod
				jmp 	cx
nogen: 	lea 	si, idparadise 							! Check Paradise 'clues'
				mov 	di, #0x7d 									! es:di 指向 VGA ROM BIOS 的 0xc000:0x007d 处，该处应该有
				mov 	cx,	#0x04 									! 4 个字符“VGA=”。
				repe
				cmpsb
				jne 	nopara 											! 若有不同的字符，表示不是 Paradise 显示卡，于是跳转。
				lea 	si,	dscparadise 						! 否则让 si 指向 Paradise 显示卡的可选行列值表，让 di 指
				lea 	di,	moparadise 							! 向扩展模式个数和模式号列表。然后跳转到 selmod 处去选
				lea 	cx,	selmod 									! 择想要使用的显示模式。
				jmp 	cx
nopara: mov 	dx, #0x3c4 									! Check Trident 'clues'
				mov 	al, #0x0e 									! 首先在端口 0x3c4 输出索引号 0x0e，索引模式控制寄存器 1。
				out 	dx,	al 											! 然后从 0x3c5 数据端口读入该寄存器原值，并暂存在 ah 中。
				inc 	dx
				in 		al,	dx
				xchg 	ah,	al
				mov 	al,	#0x00 									! 然后我们向该寄存器写入 0x00，再读取其值?al。
				out 	dx,	al 											! 写入 0x00 就相当于“原值”0x02 异或 0x02 后的写入值，
				in 		al,	dx 											! 因此若是 Trident 显示卡，则此后读入的值应该是 0x02。
				xchg 	al,	ah 											! 交换后，al=原模式控制寄存器 1 的值，ah=最后读取的值。
				mov 	bl, al 											! Strange thing ... in the book this wasn't
				and 	bl,	#0x02 									! necessary but it worked on my card which
				jz 		setb2 											! is a trident. Without it the screen goes
				and 	al,	#0xfd 									! blurred ...
				jmp 	clrb2 											!
setb2: 	or 		al,	#0x02 									!
clrb2: 	out 	dx,	al
				and 	ah,	#0x0f 									! 取 375 行最后读入值的页面个数字段（位 3--0），如果
				cmp 	ah,	#0x02 									! 该字段值等于 0x02，则表示是 Trident 显示卡。
				jne 	notrid
ev2tri: lea 	si,	dsctrident
				lea 	di,	motrident
				lea 	cx,	selmod
				jmp 	cx
notrid: mov 	dx,	#0x3cd 									! Check Tseng 'clues'
				in 		al,	dx 											! Could things be this simple ! :-)
				mov 	bl,	al 											! 先从 0x3cd 端口读取段选择寄存器原值，并保存在 bl 中。
				mov 	al,	#0x55 									! 然后我们向该寄存器中写入 0x55。再读入并放在 ah 中。
				out 	dx,	al
				in 		al,	dx
				mov 	ah,	al
				mov 	al,	bl 											! 接着恢复该寄存器的原值。
				out 	dx,	al
				cmp 	ah,	#0x55 									! 如果读取的就是我们写入的值，则表明是 Tseng 显示卡。
				jne 	notsen
				lea 	si,	dsctseng 								! 于是让 si 指向 Tseng 显示卡的可选行列值的列表，让 di
				lea 	di,	motseng 								! 指向对应扩展模式个数和模式号列表，然后跳转到 selmod
				lea 	cx,	selmod 									! 去执行模式选择操作。
				jmp 	cx
notsen: mov 	dx,	#0x3cc 									! Check Video7 'clues'
				in 		al,	dx
				mov 	dx,	#0x3b4 									! 先设置 dx 为单色显示 CRT 控制索引寄存器端口号 0x3b4。
				and 	al,	#0x01 									! 如果混合输出寄存器的位 0 等于 0（单色）则直接跳转，
				jz 		even7 											! 否则 dx 设置为彩色显示 CRT 控制索引寄存器端口号 0x3d4。
				mov 	dx,	#0x3d4
even7: 	mov 	al,	#0x0c 									! 设置寄存器索引号为 0x0c，对应显存起始地址高字节寄存器。
				out 	dx,	al
				inc 	dx
				in 		al,	dx 											! 读取显示内存起始地址高字节寄存器内容，并保存在 bl 中。
				mov 	bl,	al
				mov 	al,	#0x55 									! 然后在显存起始地址高字节寄存器中写入值 0x55，再读取出来。
				out 	dx,	al
				in 		al,	dx
				dec 	dx 													! 然后通过 CRTC 索引寄存器端口 0x3b4 或 0x3d4 选择索引号是
				mov 	al,	#0x1f 									! 0x1f 的 Video7 显示卡标识寄存器。该寄存器内容实际上就是
				out 	dx,	al 											! 显存起始地址高字节和 0xea 进行异或操作后的结果值。
				inc 	dx
				in 		al,	dx 											! 读取 Video7 显示卡标识寄存器值，并保存在 bh 中。
				mov 	bh,	al
				dec 	dx 													! 然后再选择显存起始地址高字节寄存器，恢复其原值。
				mov 	al,	#0x0c
				out 	dx,	al
				inc 	dx
				mov 	al,	bl
				out 	dx,	al
				mov 	al,	#0x55 									! 随后我们来验证“Video7 显示卡标识寄存器值就是显存起始
				xor 	al,	#0xea 									! 地址高字节和 0xea 进行异或操作后的结果值”。因此 0x55
				cmp 	al,	bh 											! 和 0xea 进行异或操作的结果就应该等于标识寄存器的测试值。
				jne 	novid7 											! 若不是 Video7 显示卡，则设置默认显示行列值（492 行）
				lea 	si,	dscvideo7
				lea		di,	movideo7
selmod: push 	si
				lea 	si,	msg2
				call 	prtstr
				xor 	cx,	cx
				mov 	cl,	(di) 										! 此时 cl 中是检查出的显示卡的扩展模式个数。
				pop		si
				push 	si
				pish 	cx
tbl: 		pop 	bx 													! bx = 显示卡的扩展模式总个数。
				push 	bx
				mov 	al,	bl
				sub 	al,	cl
				call 	dprnt 											! 以十进制格式显示 al 中的值。
				call 	spcing 											! 显示一个点再空 4 个空格。
				lodsw 														! 在 ax 中加载 si 指向的行列值，随后 si 指向下一个 word 值。
				xchg 	al,	ah 											! 交换位置后 al = 列数。
				call 	dprnt 											! 显示列数；
				xchg 	ah,	al 											! 此时 al 中是行数值。
				push 	ax
				mov 	al,	#0x78 									! 显示一个小“x”，即乘号。
				call 	prnt1
				pop 	ax 													! 此时 al 中是行数值。
				call 	dprnt 											! 显示行数。
				call 	docr 												! 回车换行。
				loop 	tbl 												! 再显示下一个行列值。cx 中扩展模式计数值递减 1。
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

! Prints <CR> + <LF> ! 显示回车+换行。

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
				.word 0, 0, 0, 0									! 第一个描述符，不用

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
				.word 512+gdt, 0x9									! 0x0009<<16+0x0200+gdt，这里用的是绝对物理地址

msg1: 				.ascii "Press <RETURN> to see SVGA-modes available or any other key to continue."
									db 0x0d, 0x0a, 0x0a, 0x00
msg2: 				.ascii "Mode: COLSxROWS:"
									db 0x0d, 0x0a, 0x0a, 0x00
msg3: 				.ascii "Choose mode by pressing the corresponding number."
									db 0x0d, 0x0a, 0x00

idati: 				.ascii 	"761295520"
idcandt: 			.byte 	0xa5 									! 标号 idcandt 意思是 ID of Chip AND Tech.
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

dscati: 			.word 0x8419, 0x842c 															! ATI 卡可设置列、行值。
dscahead: 		.word 0x842c, 0x8419, 0x841c, 0xa032, 0x5042 			! Ahead 卡可设置值。
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