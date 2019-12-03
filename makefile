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

ROOT_DEV		=/dev/hd6			#ָ�������ں�ӳ���ļ�imageʱ��ʹ�õ�Ĭ�ϸ��ļ�ϵͳ���ڵ��豸��������build����Ĭ��ʹ��/dev/hd6
SWAP_DEV		=/dev/hd2			#�����������ڵ�Ӳ�̣����̣�λ�á�

ARCHIVES		=kernel/kernel.o mm/mm.o fs/fs.o

# ����ַ��豸���ļ���.a��ʾ���ļ��ǹ鵵�ļ���Ҳ����������ִ�ж������ļ������ӳ��򼯺ϵĿ��ļ���
# ͨ����GNU��ar�������ɣ�ar��GNU�Ķ������ļ�����������ڴ������޸��Լ��ӹ鵵�ļ��г�ȡ�ļ���
DRIVERS			=kernel/blk_drv/blk_drv.a kernel/chr_drv/chr_drv.a
MATH				=kernel/math/math.a
LIBS				=lib/lib.a

# ����ĵ�һ��������ָ�����еġ�.c���ļ������Ϊ��.s��������
# $<ָ���ǵ�һ���Ⱦ����������Ｔ���������ġ�*.c���ļ���Ҳ���Ǹ�Ŀ����ͬ����ֻ�ǻ���β׺�ġ�.s���ļ���
.c.s:
						$(CC) $(CFLAGS) \
						-nostdinc -Iinclude -S -o $*.o $<
						
.s.o:
						$(AS) -c -o $*.o $<
						
.c.o:
						$(CC) $(CFLAGS) \
						-nostdinc -Iinclude -c -o $*.o $<

# all��ʾ����Makefile��֪�����Ŀ�꣬�������Image�ļ�����������������ӳ���ļ�bootimage��
# ������д�����̾Ϳ���ʹ�ø���������Linuxϵͳ�ˡ�
all: Image

Image: boot/bootsect boot/setup tools/system tools/build
				tools/build boot/bootsect boot/setup tools/system $(ROOT_DEV) \
							$(SWAP_DEV) > Image
			 sync

# disk��Image������dd���������ļ�Image������ļ�/dev/PS0�У�ÿ�ζ�/д8192�ֽ�
disk: Image
				dd bs=8192 if=Image of=/dev/PS0

tools/build: tools/build.c
				$(CC) $(CFLAGS) \
				-o tools/build tools/build.c

# ����ֻ������Ŀ���������û�и�������Ĭ�ϱ�ʾ������������ġ�.s.o����������head.oĿ���ļ���
boot/head.o: boot/head.s

tools/system: boot/head.o init/main.o \
							$(ARCHIEVE) $(DRIEVES) $(MATH) $(LIBS)
			$(LD) $(LDFLAGS) boot/head.o init/main.o \			# ���п�ʼ��ִ������
			$(ARCHIEVE) \
			$(DRIVERS) \
			$(MATH) \
			$(LIBS) \
			-o tools/system > System.map										# ��ʾgld��Ҫ������ӳ���ض�λ��System.map�ļ��С�

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

# ʹ��8086������������setup.s���б��룬����setup.o�ļ���-s��ʾ��ȥ��Ŀ���ļ��еķ�����Ϣ��
boot/setup: boot/setup.s
				$(AS86) -o boot/setup.o boot/setup.s
				$(LD86) -s -o boot/setup boot/setup.o

# ִ��Ԥ��������滻*.S�ļ��еĺ����ɶ�Ӧ��*.s�ļ���
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

# ��Ŀ���������ڲ������ļ�֮���������ϵ��
dep:
				sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
				(for i in init/*.c;do echo -n "init/"$(CPP) -M $$i;done) >> tmp_make
				cp tmp_make Makefile
				(cd fs; make dep)
				(cd kernel; make dep)
				(cd mm; make dep)

