/* 厂商 GCC 启动文件调用 entry；此处进入本工程 main，不更改厂商原文件。 */
extern int main(void);
int entry(void)
{
    return main();
}

/* 使用 -nostartfiles 时由本工程提供空的 C 运行库初始化/退出钩子。
 * 标准 __libc_init_array 仍由 newlib 提供，以保留正常的初始化数组处理。
 */
void _init(void) {}
void _fini(void) {}
