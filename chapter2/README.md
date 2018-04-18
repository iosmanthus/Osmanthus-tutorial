# Chapter 2 -- 用GRUB引导我们的内核

免得说得太难懂, 我这里尽量讲的通俗.

很多的`Linux`发行版默认使用GRUB引导内核. 一开机, 弹出来的黑白框, 那就是GRUB的引导界面, 可以通过移动光标选定我们要启动的操作系统, 按下回车, GRUB就会引导我们的操作系统内核启动. (折腾过Linux的应该都不陌生)

注意, 我这里省去了计算机引导很多细节的描述, 我这样做是为了隐藏引导器实现的细节, 把他当作一个黑匣子. 我们要面对的是操作系统, 而不是引导操作系统的`Bootloader`

然而, 我们要使用GRUB来引导我们内核启动, 我们的内核就要符合GRUB所给出的标准也就是`multiboot`标准.

那么问题来了, 怎么才能让我们的内核符合`multiboot`标准呢, 很简单只要给我们的内核文件头做一些手脚, 加上特定的文件头, GRUB在计算机启动的时候就会自动识别出我们的内核, 然后引导内核启动.

要做到这点, 我们需要用汇编给我们的内核编写一个小的引导接口, 注意下边这段代码在后边引入分页机制的开发中是会做比较大的改动的, 所以要加深这段代码的理解, 不然到了后边, 改起来就会很费力

```asm
[bits 32] ;; 以下是32位代码
MULTIBOOT_HEADER_MAGIC  equ     0x1badb002 ;; 魔数
MULTIBOOT_PAGE_ALIGN    equ     1 << 0     ;; 所有引导模块页(4KB)对齐
MULTIBOOT_MEMORY_INFO   equ     1 << 1     ;; 将内存信息写到MultiBoot结构体中

;; 将上两行属性结合起来
MULTIBOOT_HEADER_FLAGS  equ     MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO

;; 校验码
MULTIBOOT_CHECKSUM      equ     -(MULTIBOOT_HEADER_MAGIC+MULTIBOOT_HEADER_FLAGS)

;; 内核临时栈大小
KERNEL_STACK_SIZE       equ     0x400

;; 内核虚拟起始地址 我们先写在这里 暂时不用管他们的作用
;; KERNEL_VM_OFFSET        equ     0xC0000000


;; section .init.text 这个注释以后可以去掉, 我们暂时用.text去代替

section .text

dd MULTIBOOT_HEADER_MAGIC   ;;
dd MULTIBOOT_HEADER_FLAGS   ;;		将定义好的信息写进文件头
dd MULTIBOOT_CHECKSUM       ;;

[global __start:function] 	  ;; 内核入口 :function是yasm的修饰符
[extern kmain]				 ;; 内核主函数

__start:
    cli

    mov esp, __kernel_stack_top ;; 设置好内核的临时栈顶
    and esp, 0xfffffff0 ;; 按16字节对齐
    mov ebp , 0x0       ;; 作为以后backtrace的终止条件

    push ebx    ;; Grub将内存信息写到了一块内存中, 将内存的首地址放在了ebx 我们作为参数传给内核主函数
    call kmain  ;; 调用内核主函数 这里我们假定主函数永远不会返回 如果有强迫症 可以将下边注释去掉
                ;; 但是正确性在启用分页之后不保证.
    
    ;;__stop:
    ;;  hlt
    ;;  jmp __stop:
    
.end
size __start __start.end - __start  ;; 指定函数__start的大小

;; section .init.data 同上.init.text
section .data
; Temporary kernel stack
__kernel_stack_base:
  times KERNEL_STACK_SIZE db 0 ; 1KB 内核临时栈空间
__kernel_stack_top:
```

很简单的一小段汇编代码, 这段代码就可以将我们内核引导启动了, 但是问题来了, `kmain`我们还没有写呢.

```c
typedef struct _KMultiBoot
{
    ;
} KMultiBoot; // 暂时不做详细定义

void hlt()
{
    while(1)
	__asm__ volatile ("hlt");
}

void kmain(KMultiBoot *mb)
{
    char *vga = (char *)0xb8000; // VGA 显存首地址
    const char *str = "hello world";
    while(*str) {
        *vga++=*str++; // 打印hello world ...
        *vga++=0x0f;   // 黑底白字
    }
    hlt();
}
```

逻辑上这两段代码就能够打印我们的提示信息了, 但是怎么编译, 怎么链接呢.

当然, 我这里可以直接给出所需的命令, 但是我们如果每次我们修改上边的代码都要敲入重复的命令, 开发效率就会变得极慢, 当新的模块加进来时, 问题更加难以处理, 我们这里引入`GNU make` 帮我们解决这些问题.

使用`GNU make`需要提供一个`Makefile`文件, 这个文件说明构建项目的规则, 而`GNU make`帮我们做的就是根据规则, 自动的构建项目. 我现在提供一个样板作为当前项目的`Makefile`

```makefile
OS_NAME=Osmanthus
KERNEL_NAME=osmanthus
ISO_DIR=isodir
ISO_NAME=${KERNEL_NAME}.iso

BUILD=build
SCRIPTS=scripts
BOOT=boot
KERNEL=kernel
INCLUDE=include

ASM=yasm
CC=gcc
LD=ld

CFLAGS=-c -m32 -Wextra -Wall \
		-nostdinc -ffreestanding -fno-builtin -fno-stack-protector -fno-pie \
		-Xassembler --32 \
		-I${INCLUDE}

RFLAGS=-DNDEBUG -O0
DFLAGS=-g -ggdb -gstabs+

LDFLAGS = -T $(SCRIPTS)/link.ld -m elf_i386 -nostdlib
ASFLAGS = -f elf

C_SOURCES=${wildcard ${KERNEL}/*.c}

C_OBJ=$(addprefix ${BUILD}/,${C_SOURCES:.c=_c.o})

ASM_SOURCES=${wildcard ${BOOT}/*.asm}

ASM_OBJ=$(addprefix ${BUILD}/,${ASM_SOURCES:.asm=_asm.o})

.PHONY: all
all : CFLAGS+=${DFLAGS}
all :  ${BUILD} ${KERNEL_NAME}
	@cp ${BUILD}/${KERNEL_NAME} ${BUILD}/${ISO_DIR}/boot/
	@sed 's,os-name,${OS_NAME},g; s,kernel-name,${KERNEL_NAME},g' \
		${SCRIPTS}/grub.cfg > ${BUILD}/${ISO_DIR}/boot/grub/grub.cfg
	@echo -e "\033[0;31mMakeing Kernel ISO \033[0m"
	@grub-mkrescue -o ${BUILD}/${ISO_NAME} ${BUILD}/${ISO_DIR} > /dev/null 2>&1

${BUILD}:
	@echo -e "\033[0;34mBuilding 'build' directory\033[0m"
	@mkdir -p $@/${ISO_DIR}/boot/grub \
		$@/${BOOT} \
		$@/${KERNEL}


${KERNEL_NAME} : ${ASM_OBJ} ${C_OBJ}
	@echo -e "\033[0;34mLinking kernel \033[0m"
	@${LD} ${LDFLAGS} $^ -o ${BUILD}/$@

${BUILD}/%_asm.o : %.asm
	@echo -e "\033[0;32mAssembling module :\t\033[0m" $<
	@${ASM} ${ASFLAGS} $< -o $@

${BUILD}/%_c.o : %.c
	@echo -e "\033[0;32mCompiling module :\t\033[0m" $<
	@${CC} ${CFLAGS} $< -o $@

.PHONY: run
run: CFLAGS += ${RFLAGS}
run:
	@make CFLAGS="${CFLAGS}" --no-print-directory
	@echo -e "\033[0;34mStarting QEMU\033[0m"
	@qemu-system-i386 \
		${BUILD}/${ISO_NAME} > /dev/null 2>&1

.PHONY: release
release: CFLAGS += ${RFLAGS}
release:
	@echo -e "\033[0;34mMaking release version\033[0m"
	@make CFLAGS="${CFLAGS}" --no-print-directory

.PHONY: debug
debug: CFLAGS += ${DFLAGS}
debug:
	@echo -e "\033[0;34mMaking debug version\033[0m"
	@make CFLAGS="${CFLAGS}" --no-print-directory
.PHONY: clean
clean:
	@echo -e "\033[0;34mCleaning \033[0m"
	@rm -rf \
		build/*/*.o
	@echo -e "\033[0;31mFinished\033[0m"
```

但是, 熟悉`Makefile`的人都知道, 我们汇编引导接口`boot.asm`还有`kmain.c`是需要放在特定的目录的

现在我们给出当前进度的目录结构:

``` text
.
├── boot
│   └── boot.asm
├── kernel
│   └── kmain.c
├── Makefile
└── scripts
    ├── grub.cfg
    └── link.ld
```

还有一个问题就是, `link.ld` 和 `grub.cfg`怎么来, 很简单, 我这里给出一个样本

`link.ld`很重要 这个是给`ld`用的链接脚本, 他直接决定了我们内核在内存中的位置

```
ENTRY(__start)
SECTIONS
{
PROVIDE( __kernel_begin_addr = . );
.text :
{
  *(.text)
  . = ALIGN(4096);
}
.data :
{
  *(.data)
    *(.rodata)
    . = ALIGN(4096);
}
.bss :
{
  *(.bss)
  . = ALIGN(4096);
}
.stab :
{
  *(.stab)
  . = ALIGN(4096);
}
.stabstr :
{
  *(.stabstr)
  . = ALIGN(4096);
}
PROVIDE( __kernel_end_addr = . );

/DISCARD/ : { *(.comment) *(.eh_frame) }
}
```

接下来是`grub.cfg`这个是用来描述grub启动入口的

```
menuentry "os-name" {
  multiboot /boot/kernel-name
}
```

键入`make debug -B && make run`你就可以看到屏幕上出现`hello world`了

![截图](https://github.com/iosmanthus/Osmanthus-tutorial/blob/master/etc/Screenshot%20from%202018-04-18%2009-27-34.png)