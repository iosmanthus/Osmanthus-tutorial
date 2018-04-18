# Chapter0 -- 前言

本教程旨在通过逐步解析的方式, 帮助读者构建起一个简易内核

我们的目标是:

1. 通过GRUB引导
2. VGA兼容模式驱动
3. 格式打印函数 && 内核调试函数
4. GDT表的设置
5. IDT表的设置
6. 中断管理
7. 定时器编程
8. 物理内存管理
9. 虚拟内存管理
10. 内核堆管理
11. 内核级多线程


不用觉得这些有多高大上, 写这篇东西的人只是一个大二的小菜鸡, 哈哈

这篇教程大部分是参考了[x86架构操作系统内核的实现](http://wiki.0xffffff.org),  当然也有一些改进和除虫的地方, 具体细节的阐述方法也不一样. 无论如何, 我都十分佩服作者刘欢能够写出这么优质的博客.

当然还有[JamesM's kernel development tutorials](http://www.jamesmolloy.co.uk/tutorial_html/), 说实话这个我没参考多少, 看上去挺好, 深入进去就发现, 实在是太简陋了, [OSDEV的这篇Wiki](https://wiki.osdev.org/James_Molloy's_Tutorial_Known_Bugs)抓出了他一堆BUG和奇奇怪怪的问题, 但是还是有一定作用的.