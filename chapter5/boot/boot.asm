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
.end

size __start __start.end - __start  ;; 指定函数__start的大小

;; section .init.data 同上.init.text
section .data
; Temporary kernel stack
__kernel_stack_base:
  times KERNEL_STACK_SIZE db 0 ; 1KB 内核临时栈空间
__kernel_stack_top:
