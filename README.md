# Chapter 4 -- 库函数, 调试函数 和 格式化打印函数

现在我们的内核已经能够启动并且输出任意字符串了. 但是一些基本的库函数还是缺失的, 为了后续的开发, 我们要对其中一些较为常用的函数进行实现. 我这里就不给出具体实现了, 考验各位C功底的时候到了(说得我很厉害的样子哈哈)

给出相关的函数声明吧:

```c
#ifndef _KSTRING_H_
#define _KSTRING_H_

#include <ktypes.h>

void* kmemcpy(void* dst, const void* src, u32 n);

void* kmemset(void* s, u8 c, u32 n);
void* kbzero(void* s, u32 n);

i32 kstrcmp(const char* s1, const char* s2);
i32 kstrncmp(const char* s1, const char* s2, u32 n);

char* kstrcpy(char* dst, const char* src);
char* kstrncpy(char* dst, const char* src, u32 n);

char* kstrcat(char* dest, const char* src);
char* kstrncat(char* dest, const char* src, u32 n);

u32 kstrlen(const char* s);

#endif /* ifndef _KSTRING_H_ */
```

我这里声明是有问题的, 在OSDev的wiki上说上边的函数最好要跟标准库的声明相同, 这样编译器可以在编译的时候利用这些函数进行优化, 这里我也不管了, 先让车跑起来再说, 跑得快慢那都是后话了.



现在我们要实现一下格式化打印函数, 也就是类似与C标准库里边的`printf,sprintf`, 我们可以称他为`kprintf`

但是, 这个函数显然比较复杂, 各种条件判断. 稍有不小心, 就会编写出有问题的代码, 这个时候为了除虫, 我们要通过GDB来调试我们的内核.

我们已经做出了`osmanthus.iso`, 存放在`build`目录里边, 这个时候我们可以通过`qemu`启动他但是不直接运行系统, 而是根据调试器指令来运行.

可以键入

```bash
qemu-system-i386 -s -S -cdrom build/osmanthus.iso
```

这个时候就可以让我们的`gdb`进行远程调试了.

启动`gdb`之后键入

```
set architecture i386:intel
symbol-file build/osmanthus
target remote :1234
b kmain
c
```

这时候你就可以看到

![](https://github.com/iosmanthus/Osmanthus-tutorial/blob/master/etc/Screenshot%20from%202018-04-20%2009-33-54.png)

之后我们就可以着手实现我们的格式化打印函数了.

首先, 我们给出相关的函数原型.

```c
i32 kvsprintf(char *s, const char *fmt, kva_list args);
i32 kprintf(const char *fmt, ...);
```

这里就引出了另外一个问题, 那就是可变参函数的问题. 怎么让我们的内核支持这一点呢

我们需要引入在`x86`环境下的可变参函数宏. 这里可以先查阅`x86`下函数调用的规范, 然后再写出这样的宏. 这里就直接给出

```c
#ifndef _KARG_H_
#define _KARG_H_

// Various arguments macro for i386

#include <ktypes.h>

typedef char* kva_list;


// 这里用到了内存对齐的宏, 可以将二进制写出来, 看看这个宏是怎么工作的.
#define kva_start(ap, last) \
  (ap = (kva_list)(((kva_list)&last) + ((sizeof(last) + 3) & ~3))) 

#define kva_arg(ap, type)                    \
  (*(type*)(ap += ((sizeof(type) + 3) & ~3), \
      ap - ((sizeof(type) + 3) & ~3)))

#define kva_copy(dst, src) (dst) = (src)

#define kva_end(ap) (ap = (kva_list)NULL)

#endif /* ifndef _KARG_H_ */

```

上边这个宏的用法就和标准库一样了, 这里我就不再赘述了, 可以去Google一下.

接下来的格式化函数挺复杂, 我不在这里赘述, 核心是`i32 kvsprintf(char *s, const char *fmt, kva_list args);`这个函数是最重要的, 只要实现了他, 我们就可以利用这个函数实现其余的格式化打印函数. 这个可以参考`linux 0.01`的代码 或者本章仓库里边的代码.

接下来的问题可能会有点棘手, 我们要给我们的内核添加一个`backtrace`函数, 这个函数的作用就是一旦被调用, 他就打印出函数调用栈, 这个函数在内核的调试中是十分管用的, 一旦出现问题, 我们就可以很快的定位到问题发生的位置, 另外一个就是, 我们还可以通过调试器中的`backtrace`实现同样的功能.

获取函数调用栈, 我们要做到以下两点

1. 从当前位置开始, 向上返回, 当然这不是真返回, 而是通过向上遍历的方法实现逻辑上的返回
2. 返回的同时打印函数名字.

我们首先解决函数名字获取的问题.

我们内核是一个`ELF`格式的文件, 当GRUB加载我们的内核的时候, 他会将我们内核文件的一些`ElF`信息写到内存中的一个`KMultiBoot`结构体中.

现在我们就要知道`KMultiBoot`的具体定义.

```c
#ifndef _KMULTIBOOT_H_
#define _KMULTIBOOT_H_

#include <ktypes.h>

typedef struct _KMultiBoot {
  u32 flags;
  u32 mem_lower;
  u32 mem_upper;

  u32 boot_device;
  u32 cmdline;
  u32 mods_count;
  u32 mods_addr;

  /* 这里是我们关心的内核ELF section_header 信息 */  
  u32 num;   // 一共多少项信息
  u32 size;  // 每项大小
  u32 addr;  // section_header数组首地址
  u32 shndx; // 这个是一个基于section_header地址的索引, 指向一个字符串表, 表中描述各表的字符串信息.

  // Memory info 
  // 这个是以后我们关心的另外一个点
  u32 mmap_length;
  u32 mmap_addr;

  u32 drives_length;
  u32 drives_addr;
  u32 config_table;
  u32 boot_loader_name;
  u32 apm_table;
  u32 vbe_control_info;
  u32 vbe_mode_info;
  u32 vbe_mode;
  u32 vbe_interface_seg;
  u32 vbe_interface_off;
  u32 vbe_interface_len;
} __attribute__((packed)) KMultiBoot; // 这个前缀要求结构体内部无空隙
```

看到这里不懂, 没事我们继续往下走, 待会就可能懂了.

我们先不给出下边代码某些类型的定义, 因为给出来可能并不有助于我们理

先看完下边代码的注释 可能我们可以更好的理解上边`KMultiBoot`结构体中我们所讨论的几个成员

```c
#include <kelf.h>
#include <kstring.h>
#include <kvmm.h>

#define ELF32_ST_TYPE(i) ((i)&0xf)

// Get kernel elf info
KELF kget_kernel_elf_info(const KMultiBoot* mb)
{
  // 从这里我们可以看出, 这里的`addr`就是我们内核ELF文件section_header数组的首地址
  KELFSectionHeader* begin = (KELFSectionHeader*)mb->addr;
  KELF elf;

  // 获取存放各个sections名字的section的首地址. 
  u32 shstrtab = begin[mb->shndx].addr; // Section name array
  // Loop through all sections

  // 遍历全部的section, 如果section的名字是".strtab", 将相关信息存放在`elf`中;
  // 如果名字是".symtab", 将相关信息存放在`elf`中.
  // 不需要知道这段代码抛开KELF的具体表示, 也可以理解 就是将内核的.strtab 和 .symtab具体信息提取出来
  // 存放在`elf`中, 函数再将得到的`elf`扔回给调用者.
  // 从代码中我们也可以推断, strtab
  const KELFSectionHeader* end = &begin[mb->num];
  for (const KELFSectionHeader* sh = begin; sh < end; ++sh) {
      
    // 这里可以推断, sh->name只是一个索引
    const char* section_name = (const char*)(shstrtab + sh->name);

    if (kstrcmp(section_name, ".strtab") == 0) {
      elf.strtab = (const char*)(sh->addr);
      elf.strtabsz = sh->size;
      continue;
    } else if (kstrcmp(section_name, ".symtab") == 0) {
      elf.symtab = (KELFSymbol*)(sh->addr);
      elf.symtabsz = sh->size;
    }
  }
  return elf;
}
```

我们这时候再给出上边未定义`KELFSectionHeader`的具体定义. 具体的成员含义就很明显了.

```c
// ELF section header
typedef struct _KELFSectionHeader {
  u32 name;
  u32 type;
  u32 flags;
  u32 addr;
  u32 offset;
  u32 size;
  u32 link;
  u32 info;
  u32 addralign;
  u32 entsize;
} __attribute__((packed)) KELFSectionHeader;
```

接下来问题是`KELF`的定义了, 其实他的定义很简单, 我们在上边用到什么就定义什么就好了.

```c
// ELF info
typedef struct _KELF {
  KELFSymbol* symtab;
  u32 symtabsz;
  const char* strtab;
  u32 strtabsz;
} KELF;
```

问题是, 上边结构体的`strtab`成员为什么是一个字符串的指针呢, 既然说是存放字符串的表, 	为什么不是一个指向字符串指针的指针呢, 也就是`const char **`, 接下里我们看看这个搜索函数, 看看能不能看出一些门道.	

这个时候我们要给出每个`Symbol`个体的结构定义

```c
// ELF Symbol
typedef struct _KELFSymbol {
  u32 name;
  u32 value;
  u32 size;
  u8 info;
  u8 other;
  u16 shndx;
} __attribute__((packed)) KELFSymbol;
```

```c
// Return the name of the function whose address range includes 'addr'家
// 我们搜索一个函数, 很简单, 遍历传入`elf`的symtab
const char* ksearch_function(u32 addr, const KELF* elf)
{
  if (elf) {
    const KELFSymbol* end = &elf->symtab[elf->symtabsz / sizeof(KELFSymbol)];

    for (const KELFSymbol* symbol = elf->symtab; symbol < end; ++symbol)
      // 先判断当前的Symbol是不是函数
      if (ELF32_ST_TYPE(symbol->info) != 0x2)
        continue;
      // 如果传入地址在symbol的内存空间中 返回该symbol的名字.
      // 这个时候我们就能解释之前提出的问题了.
      // .strtab的内存分布是这样的
      /*
      |----------|---------|_____
      |   name1  |  name2  |...
      |----------|---------|______
      也就是每个字符串是连续存储的 这也就是为什么`KELF.strtab`不是一个二级指针.
      */
      else if (addr >= symbol->value && addr < symbol->value + symbol->size)
        return (const char*)(elf->strtab + symbol->name);
  }
  return NULL;
}
```

注意, 这个时候我们只是完成了一个根据地址查找函数名的函数.

我们是要打印函数调用栈, 单单上边的函数是不够的, 我们还得向上遍历对吧. 遍历的时候, 很容易就可以获得遍历到的函数名字.

结合起来就可以实现我们的功能了.

那怎么遍历呢, 我们还需要进一步熟悉`x86`的函数调用过程.

我们看看`kmain.c`的反汇编代码

```assembly
00000006 <kmain>:
   6:	55                   	push   %ebp
   7:	89 e5                	mov    %esp,%ebp
   9:	83 ec 08             	sub    $0x8,%esp
   c:	83 ec 0c             	sub    $0xc,%esp
   f:	68 00 00 00 00       	push   $0x0
  14:	e8 fc ff ff ff       	call   15 <kmain+0xf>
  19:	83 c4 10             	add    $0x10,%esp
  1c:	e8 fc ff ff ff       	call   1d <kmain+0x17>
  21:	90                   	nop
  22:	c9                   	leave  
  23:	c3                   	ret    
```

关注一下前两行代码

```assembly
push   %ebp
mov    %esp,%ebp
```

每次调用一个函数, 我们就做这两步, 在没有返回前这部分的栈是不会发生变化的.

假定某个函数`a`调用了`b`, `b`做完上述的两步

也就是

```c
void b()
{
    ;
}
void a()
{
    b();
}
```

这个时候栈的结构是这样的和

```
从b函数返回到函数a后执行的代码地址
a的ebp <-- esp, b的ebp 产生这样的结果是因为我们执行了上边的两行代码
```

现在我们可以看到 (`%ebp+4)`其实是一个上层函数的地址, 这样通过这个地址我们就可以用上边的搜索函数来查找到对应的名字.

接下来的事情就很简单了, 我们只要接续向上遍历, 也就是把现在的`%ebp`的值更新一下, 变成`a`的`%ebp`就实现了向上迭代, 简单吧.

那真的有这么简单么, 循环的终止条件是什么? 对的就是我们在`boot.asm`中设置的`ebp`为0,这就是循环终止的条件

我们就可以得出这样的代码:

```c
void kstack_trace()
{
  env_init();
  u32 *ebp, *eip;

  // Get %ebp
  __asm__ volatile("mov %%ebp,%0"
                   : "=r"(ebp));

  // %ebp was initiated in boot/boot.asm with 0x0
  while (ebp) {
    /*
     * | -------- |
     * |   ...    |  < ---- ebp_prev
     * | -------- |
     * |    .     |
     * |    .     |
     * |    .     |
     * | -------- |
     * |    &$    | < ---- call function `x` ($ is the addr of next instruction)
     * | -------- |
     * | ebp_prev | < ---- push ebp; mov ebp, esp | <= esp , ebp
     * | -------- |
     */
    eip = ebp + 1; // (void *)eip == (void *)ebp + 4
    kprintf("\t[%p]:\t%s\n", *eip - 5, ksearch_function(*eip - 5, pkelf));
    ebp = (u32*)*ebp;
  }
}

```

对于 `env_init();`这个的作用就是调用`kget_kernel_elf_info`初始化`pkelf`, 具体实现可以参考仓库里的代码, 这里不再赘述. 

注意 我们在`kmain`中要将传入的参数也就是传入的`KMultiBoot`指针全局化, 也就是将他的值复制给一个全局变量.

接下来我们还要实现几个调试相关的函数, 但是相比起来就简单很多了

```c
void kpanic(const char* msg)
{
  kprintf("****Fatal error eccured****\n"
          "****Kernel in panic****\n");
  if (msg)
    kprintf("%s\n", msg);
  kstack_trace();
  while (1)
    ;
}

void __kassert(const char* file, int line, const char* func, const char* expr)
{
  kcprintf(VGA_BLACK, VGA_LIGHT_BROWN,
      "In file %s: %d function %s asserts '%s' failed\n", file, line,
      func, expr);
  kpanic(NULL);
}
```

注意第二个函数我们不直接使用, 而是将其打包在一个宏中.

```c
#ifdef NDEBUG
#define kassert(cond) (void)0
#else
void __kassert(const char* file, int line, const char* func,
    const char* expr);

#define kassert(cond)                                 \
  do {                                                \
    if (!(cond)) {                                    \
      __kassert(__FILE__, __LINE__, __func__, #cond); \
    }                                                 \
  } while (0)
#endif
```

和标准库的实现是大体上一致的, 所以具体用法也不再赘述.

记得编译的时候要修改`Makefile`, 把添加的目录放进`Makefile`相应变量中

下边给出我们本章的测试代码.

```c
#include <kdebug.h>
#include <kmultiboot.h>
#include <ktypes.h>
#include <kvga.h>

void hlt() {
  kstack_trace();
  kpanic("Error?!\n");
  while (1)
    __asm__ volatile("hlt");
}

void kmain(KMultiBoot *mb) {
  KERNEL_BOOT_INFO = mb;   // 初始化全局KMultiBoot指针
  hlt();
}
```

最后的截图

![](https://github.com/iosmanthus/Osmanthus-tutorial/blob/master/etc/Screenshot%20from%202018-04-21%2009-15-08.png)