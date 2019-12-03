typedef struct desc_struct {
	unsigned long a, b;				// 段描述符结构体，用两个4字节长度的long类型变量组成8字节的描述符表项
}desc_table[256];

extern unsigned long pg_dir[1024];	// 页目录表，在boot/head.s中定义，其位于为线性地址0，同时也是物理地址0，每个表项4字节，一共1024项
extern desc_table idt, gdt;			// idt和gdt都使用了extern关键字，表明其是在别的文件中定义的，这个文件就是boot/head.s。
