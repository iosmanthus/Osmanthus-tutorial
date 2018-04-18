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
