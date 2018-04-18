# Chapter 3 -- VGA兼容模式驱动

上一章我们看到, 我们只要在将字符写进某块特定的内存, 就可以显示出该字符, 问题是, 我们这样去写字符, 屏幕光标不会移动, 当然也不会滚屏, 而且写字符串连个接口都没有, 这个是无法容忍的, 所以我们要给我们的内核写个VGA兼容模式驱动

写进一个字符很简单, 只要往显存里边写入字符就行了, 但是光标要移动. 要移动光标, 首先就要知道光标在哪, 怎么移动. 这些我们都要通过I/O指令完成. 我们先可以写出这样的一些基本接口, 给我们的内核用来跟外部设备通信.

当然, 我们还是在项目根目录里边添加`driver`还有`include`, 然后修改相应的`Makefile`, 我这里就不贴出来了, 给一个链接

这里用到了`gcc`的内联汇编, 可以先查阅相关文档, 如果不熟悉的话

[GCC inline ASM](https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html)

1. `kin`

   ```c

   #include <kport.h>

   u32 kin(u16 port, KDataSize size)
   {
     u8 db = 0;
     u16 dw = 0;
     u32 dl = 0;
     if (size == KBYTE) {
       __asm__ volatile("inb %%dx, %%al"
                        : "=a"(db)
                        : "d"(port));
       return db;
     } else if (size == KWORD) {
       __asm__ volatile("inw %%dx, %%ax"
                        : "=a"(dw)
                        : "d"(port));
       return dw;
     } else if (size == KDWORD) {
       __asm__ volatile("inl %%dx, %%eax"
                        : "=a"(dl)
                        : "d"(port));
       return dl;
     }
     return 0;
   }
   ```

`KDataSize`是定义在`<ktypes.h>`的一个枚举体, 自行定义就好, 这里其他用到的类型也都都定义在`<ktypes.h>`

都可以自行定义.

2. `kout`

   ```c
   void kout(u16 port, u32 data, KDataSize size)
   {
     if (size == KBYTE)
       __asm__ volatile("outb %%al,%%dx" ::"d"(port), "a"(data));
     else if (size == KWORD)
       __asm__ volatile("outw %%ax,%%dx" ::"d"(port), "a"(data));
     else if (size == KDWORD)
       __asm__ volatile("outl %%eax,%%dx" ::"d"(port), "a"(data));
   }
   ```

做好了上边两个接口, 我们就能够通过他们跟屏幕光标打交道了

注意这里我们面对的屏幕是80x24的, 所以一下的长是 80, 宽是 24

在`kvga.c`中, 我们可以写出这样的接口来获取光标位置

```c
#define CURSOR_CTRL_PORT 0x3d4 // Control port
#define CURSOR_DATA_PORT 0x3d5 // Data port
...
inline u32 get_cursor()
{
  u32 offset = 0;
  kout(CURSOR_CTRL_PORT, 0xe, KBYTE);  // 向光标发送控制指令, 告诉他我们要取光标坐标低八位
  offset = kin(CURSOR_DATA_PORT, KBYTE) << 8; // 将光标坐标高八位取回 加入到偏移量中

  kout(CURSOR_CTRL_PORT, 0xf, KBYTE);  // 低八位
  offset += kin(CURSOR_DATA_PORT, KBYTE);

  return offset << 1; // NOTE: 每个VGA显存单元两字节 一个是要显示的字符, 另外一个字符属性
}
```

接下来就是设置光标位置

```c
inline void set_cursor(u32 offset)
{
  offset >>= 1;
  kout(CURSOR_CTRL_PORT, 0xe, KBYTE);         // Open higher byte
  kout(CURSOR_DATA_PORT, offset >> 8, KBYTE); // Write

  kout(CURSOR_CTRL_PORT, 0xf, KBYTE);             // Open lower byte
  kout(CURSOR_DATA_PORT, (offset & 0xff), KBYTE); // Write
}
```

做好这两个接口我们就已经完成一大半了.

现在就是处理字符的打印逻辑, 我们这里将所有接口通用的操作抽象出来, 成为一个本地的函数`write`

```c
u32 write(char ch, VgaTextAtrr bg, VgaTextAtrr fg, u32 offset)
{
  u32 x = get_pos_x(offset), y = get_pos_y(offset);

  if (x >= HEIGHT || y >= WIDTH) {
    kvga_scroll();
    offset = get_offset(HEIGHT - 1, 0);
  }

  if (ch == '\n')
    return get_offset(x + 1, 0);

  if (ch == '\t') {  // 按八对齐 实现制表符控制
    u32 align = TAB_WIDTH << 1;
    return offset / align * align + align;
  }
  
  // 这里可以添加更多的控制字符控制逻辑.
  // 如果太多, 我们可以通过设计一个函数表, 根据控制字符跳转到相应的处理函数(or label)
   
    
  VGA[offset] = ch;
  VGA[offset + 1] = ((u8)bg << 4) + (u8)fg;

  return offset + 2;
}
```

这个函数的作用很关键, 每次写入一个字符, 我们就返回下一个写入位置, 当然也处理各种控制字符. 这样就把尽量多的细节隐藏了起来, 我们在以后也会用到类似的方法. 问题是滚屏怎么实现呢

给出下边的实现.

```c
void kvga_scroll()
{
  typedef u16(*VideoMem)[WIDTH];
  VideoMem v = (VideoMem)VGA;

  for (i32 i = 0; i < HEIGHT - 1; ++i)
    for (i32 j = 0; j < WIDTH; ++j)
      v[i][j] = v[i + 1][j];

  for (i32 j = 0; j < WIDTH; ++j)
    v[HEIGHT - 1][j] = 0;
}
```

其实很简单, 滚屏就是将第一行抛弃, 将剩余内容向上移一行, 最后一行清空.

好了, 问题是字符属性怎么定义呢, 简单查阅相关资料就能知道, 我这里贴出我的定义

```c
typedef enum _VgaTextAtrr {
  VGA_BLACK = 0,
  VGA_BLUE,
  VGA_GREEN,
  VGA_CYAN,
  VGA_RED,
  VGA_MAGENTA,
  VGA_BROWN,
  VGA_LIGHT_GREY,
  VGA_DARK_GREY,
  VGA_LIGHT_BLUE,
  VGA_LIGHT_GREEN,
  VGA_LIGHT_CYAN,
  VGA_LIGHT_RED,
  VGA_LIGHT_MAGENTA,
  VGA_LIGHT_BROWN, // Yellow
  VGA_WHITE
} VgaTextAtrr;
```



接下来, 各位基本上就不用看我这章剩下的内容了, 想必用发散的思维设计剩下的接口和实现会更加有意思, 但是我还是把剩下的重要实现列举出来.

这里我只给出`kvga_cputs`的实现, 也就是带颜色输出字符串的实现, 其他的接口都可以通过这个实现.

```c
i32 kvga_cputs(const char *str, VgaTextAtrr bg, VgaTextAtrr fg) {
  u32 offset = get_cursor();
  i32 cnt = 0;
  while (*str) {
    cnt++;
    offset = write(*str++, bg, fg, offset); // 获取下一个写入位置
    set_cursor(offset); // 更新光标
  }
  return cnt;
}
```

接下来我给出`kvga_puts("hello world\n")`的测试结果

