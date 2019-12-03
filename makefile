#
# if you want the ram-disk device, define this to be the size in blocks.
RAMDISK 		= #-DRAMDISK=512

AS86 				=as86 -0 -a
LD86 				=ld86 -0

AS 					=gas
LD					=gld

LDFLAGS			=-s -x -M

CC 					=gcc $(RAMDISK)

CFLAGS			=-Wall -0 -fstrength-reduce -fomit-frame-pointer \
-fcombine-regs -mstring-insns

CPP					=cpp -nostdinc -Iinclude

ROOT_DEV		=/dev/hd6			#指定生成内核映像文件image时所使用的默认根文件系统所在的设备，空着是build程序默认使用/dev/hd6
SWAP_DEV		=/dev/hd2			#交换分区所在的硬盘（磁盘）位置。

ARCHIVES		=kernel/kernel.o mm/mm.o fs/fs.o

# 块和字符设备库文件，.a表示该文件是归档文件，也即包含许多可执行二进制文件代码子程序集合的库文件，
# 通常由GNU的ar程序生成，ar是GNU的二进制文件处理程序，用于创建、修改以及从归档文件中抽取文件。
DRIVERS			=kernel/blk_drv/blk_drv.a kernel/chr_drv/chr_drv.a
MATH				=kernel/math/math.a
LIBS				=lib/lib.a

# 这里的第一个规则是指将所有的“.c”文件编译成为“.s”汇编程序。
# $<指的是第一个先决条件，这里即符合条件的“*.c”文件，也就是跟目标相同名字只是换了尾缀的“.s”文件。
.c.s:
						$(CC) $(CFLAGS) \
						-nostdinc -Iinclude -S -o $*.o $<
						
.s.o:
						$(AS) -c -o $*.o $<
						
.c.o:
						$(CC) $(CFLAGS) \
						-nostdinc -Iinclude -c -o $*.o $<

# all表示创建Makefile所知的最顶层目标，这里既是Image文件，它是引导启动盘映像文件bootimage，
# 若将其写入软盘就可以使用该软盘启动Linux系统了。
all: Image

Image: boot/bootsect boot/setup tools/system tools/build
				tools/build boot/bootsect boot/setup tools/system $(ROOT_DEV) \
							$(SWAP_DEV) > Image
			 sync

# disk由Image产生，dd复制输入文件Image到输出文件/dev/PS0中，每次读/写8192字节
disk: Image
				dd bs=8192 if=Image of=/dev/PS0

tools/build: tools/build.c
				$(CC) $(CFLAGS) \
				-o tools/build tools/build.c

# 这里只给出了目标和依赖，没有给出规则，默认表示利用上面给出的“.s.o”规则生成head.o目标文件。
boot/head.o: boot/head.s

tools/system: boot/head.o init/main.o \
							$(ARCHIEVE) $(DRIEVES) $(MATH) $(LIBS)
			$(LD) $(LDFLAGS) boot/head.o init/main.o \			# 此行开始是执行命令
			$(ARCHIEVE) \
			$(DRIVERS) \
			$(MATH) \
			$(LIBS) \
			-o tools/system > System.map										# 表示gld需要将连接映像重定位到System.map文件中。

kernel/math/math.a:
				(cd kernel/math; make)
				
kernel/blk_drv/blk_drv.a:
				(cd kernel/blk_drv; make)

kernel/chr_drv/chr_drv:
				(cd kernel/chr_drv; make)

kernel/kernel.o:
				(cd kernel; make)

mm/mm.o:
				(cd mm; make)
fs/fs.o:
				(cd fs; make)

lib/lib.o:
				(cd lib; make)

# 使用8086汇编和链接器对setup.s进行编译，生成setup.o文件，-s表示需去除目标文件中的符号信息。
boot/setup: boot/setup.s
				$(AS86) -o boot/setup.o boot/setup.s
				$(LD86) -s -o boot/setup boot/setup.o

# 执行预处理程序，替换*.S文件中的宏生成对应的*.s文件。
boot/setup.s: boot/setup.S include/linux/config.h
				$(CPP) -traditional boot/setup.S -o boot/setup.s

boot/bootsect.s: boot/bootsect.S include/linux/config.h
				$(CPP) -traditional boot/bootsect.S -o boot/bootsect.s

boot/bootsect: boot/bootsect.s
				$(AS86) -o boot/bootsect.o boot/bootsect.s
				$(LD86) -s -o boot/bootsect boot/bootsect.o

clean:
				rm -f Image.map tmp_make core boot/bootsect boot/setup \
							boot/bootsect.s boot/setup.s
				rm -f init/*.o tools/system tools/build boot/*.o
				(cd mm; make clean)
				(cd fs; make clean)
				(cd kernel; make clean)
				(cd lib; make clean)

backup: clean
					(cd ..; tar -cf - linux | compress - > backup.Z)
					sync

# 该目标或规则用于产生各文件之间的依赖关系。
dep:
				sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
				(for i in init/*.c;do echo -n "init/"$(CPP) -M $$i;done) >> tmp_make
				cp tmp_make Makefile
				(cd fs; make dep)
				(cd kernel; make dep)
				(cd mm; make dep)

