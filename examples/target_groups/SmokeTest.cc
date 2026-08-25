#include <stdio.h>

extern int core_value(void);

int main(void)
{
    if (core_value() != 42)
        return 1;
    puts("target groups passed");
    return 0;
}
