#include "Greeter.h"

#include "util.h"

#ifndef MIXED_EXAMPLE
#error "mixed_CXXFLAGS did not reach the C++ compile line"
#endif

std::string greeting()
{
    return std::string("hello from ") + util_subject();
}
