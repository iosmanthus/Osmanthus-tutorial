# Chapter 5 -- 设置GDT

接下来我们讨论如何引入GDT(全局段描述符表).

首先什么是GDT呢, 我们要解释这个概念, 首先回顾一下16位实模式下内存分段的概念: 在16位实模式下, 我们利用段寄存器存放段基址, 加上一个偏移地址来实现20位寻址. 在保护模式下, 我们也是这样做的, 但是这里我们段寄存器存放的不再是段基址, 而是GDT表中的索引, 我们通过查表找到我们的段基址, 接下来的地址引用就是段内的偏移地址. 举个例子, 假设说, 现在`ds`的值为`0x8`, 现在有:

```assembly
mov [0xb8000], 91
```

这个时候, `0xb8000`就应该这样被翻译成物理地址.

1. 以`ds`为索引, 在GDT表中找到对应的表项
2. 读出表项中的基址.
3. 以`0xb8000`为偏移量, 加上基址从而获得物理地址

 我们要在保护模式下设置GDT表实现平坦模式, 也就是我们这里将4GB内存看作一个段, 而不是若干个.

代码段, 数据段, 用户代码段, 用户数据段都是从`0x00000000`到`0xffffffff`.

一旦我们在内存中设置好了GDT表, 就要让我们的CPU去加载他. 为了让CPU加载GDT, 我们要将GDT的信息写入一块内存当中, 方便CPU加载. CPU加载GDT并不是将GDT整张表放进CPU内部, 而是通过一个48位的全局描述符表寄存器来保存GDT信息, 寻址时才从内存中读取GDT内容.

全局描述符表寄存器存储结构如下:

```
|47         16|15    0|
|			 |        |
| Base addr   | limit |
|		     |        |
```

第一段是GDT表的地址, 第二个是其大小

注意到, 如果我们还工作在16位实模式下, 我们的内核怎么可能加载到了物理内存1MB以上的位置, 而且还可以正常工作, 说明我们的GRUB已经给我们设置好了GDT, 并且已经跳转到了32位实模式, 但是为什么我们还要自己设置GDT呢.

有两个原因:

1. 学习一下....
2. 后边TSS任务切换需要用到GDT



好, 我们现在先给出GDT表项的定义:

```c
typedef struct _KSegmentDescritor {
  u16 limit_low;
  u16 base_low;
  u8 base_mid;
  u8 access;
  u8 granularity;
  u8 base_high;
} __attribute__((packed)) KSegmentDescritor;
```

很奇怪的内存分布吧, `limit`和`base`竟然是碎片化的. 据说是历史遗留问题, 不关心.

然后我们还得定义一下GDT指针

```c
typedef struct _KGDTPtr {
  u16 limit; // GDT actual size - 1
  u32 addr;  // GDT begin addr
} __attribute__((packed)) KGDTPtr;
```

接下来就可以来设置GDT

```c
#include <kgdt.h>
#include <kio.h>

static KSegmentDescritor gdt[GDT_ENTRY_CNT];

static void set_segment_descriptor( KSegmentDescritor *sd, u32 base, u32 limit,
                                    u8 access, u8 granularity );

void set_segment_descriptor( KSegmentDescritor *sd, u32 base, u32 limit,
                             u8 access, u8 granularity )
{
  sd->limit_low = limit & 0xffff;
  sd->base_low = base & 0xffff;
  sd->base_mid = ( base >> 16 ) & 0xff;
  sd->access = access;
  sd->granularity = ( granularity & 0xf0 ) | ( ( limit >> 16 ) & 0xf );
  sd->base_high = base >> 24;
}

KGDTPtr kinit_gdt()
{
  KGDTPtr gdt_ptr;
  gdt_ptr.addr = (u32)gdt;
  gdt_ptr.limit = sizeof( gdt ) - 1;
   
  // 接下来就是平坦模式的设置.

  // Null descritor
  set_segment_descriptor( &gdt[NULL_SEG_INDEX], 0x00000000, 0x00000000, 0x00,
                          0x00 );

  // Code segment descritor
  set_segment_descriptor( &gdt[CODE_SEG_INDEX], 0x00000000, 0xffffffff, 0x9a,
                          0xcf );

  // Data segment descritor
  set_segment_descriptor( &gdt[DATA_SEG_INDEX], 0x00000000, 0xffffffff, 0x92,
                          0xcf );

  // User code segment descritor
  set_segment_descriptor( &gdt[USER_CODE_SEG_INDEX], 0x00000000, 0xffffffff,
                          0xfa, 0xcf );

  // User data segment descritor
  set_segment_descriptor( &gdt[USER_DATA_SEG_INDEX], 0x00000000, 0xffffffff,
                          0xf2, 0xcf );

  return gdt_ptr; // 返回一个GDT指针
}
```

接下来我们要用汇编写一个GDT的加载函数.

```assembly
[global kload_gdt:function]
kload_gdt:
  mov eax, [esp + 4]
  lgdt [eax]

  mov ax, 0x10
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax
  mov ss, ax

  jmp 0x08:__long_jmp  ;; 远跳 刷新cs

__long_jmp:
  ret
.end
size kload_gdt __long_jmp.end - kload_gdt
```

这个函数原型就是`void kload_gdt(const KGDTPtr* gdt_ptr);`

接下来就很简单了

```c
#include <kdebug.h>
#include <kgdt.h>
#include <kio.h>
#include <kmultiboot.h>
#include <ktypes.h>

void hlt() {
  while (1)
    __asm__ volatile("hlt");
}

void kmain(KMultiBoot *mb) {
  KERNEL_BOOT_INFO = mb;

  KGDTPtr gdt_ptr = kinit_gdt();  // 获取设置好平坦模式的GDT表
  kload_gdt(&gdt_ptr);            // 加载

  kprintf("hello world?\n");

  hlt();
}
```

注意我们这个时候的项目结构发生了变化, 我们将系统初始化的部分放在`init`目录, `Makefile`也要改变

```
.
├── boot
│   └── boot.asm
├── driver
│   ├── kport.c
│   └── kvga.c
├── include
│   ├── karg.h
│   ├── kdebug.h
│   ├── kelf.h
│   ├── kgdt.h
│   ├── kio.h
│   ├── kmultiboot.h
│   ├── kport.h
│   ├── kstring.h
│   ├── ktypes.h
│   └── kvga.h
├── init
│   ├── kgdt.asm
│   └── kgdt.c
├── kernel
│   ├── kio.c
│   └── kmain.c
├── lib
│   ├── kdebug.c
│   ├── kelf.c
│   └── kstring.c
├── Makefile
├── README.md
└── scripts
    ├── gdbinit
    ├── grub.cfg
    └── link.ld
```



