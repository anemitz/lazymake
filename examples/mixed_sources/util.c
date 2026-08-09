#include "util.h"

#ifndef MIXED_C_EXAMPLE
#error "mixed_CFLAGS did not reach the C compile line"
#endif

const char *util_subject(void)
{
    return "mixed sources";
}
