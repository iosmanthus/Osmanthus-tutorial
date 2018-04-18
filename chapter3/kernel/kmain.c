#include <ktypes.h>
#include <kvga.h>
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
    kvga_puts("hello world");
    hlt();
}
