#include <string.h>

extern unsigned int _bss_start, _bss_end;

extern int main(void);

void __attribute__((noreturn)) call_start_cpu0(void)
{
    memset(&_bss_start, 0, (&_bss_end - &_bss_start) * sizeof(_bss_start));

    main();

    while (1) { }
}

void __attribute__((noreturn)) call_start_cpu1(void)
{
    while (1) { }
}
