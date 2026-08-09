#ifndef MIXED_EXAMPLE
#error "mixed_CXXFLAGS did not reach the .cxx compile line"
#endif

// Artifact of the .cxx pattern rule; linked into the mixed binary.
int mixed_cxx_units(void)
{
    return 1;
}
