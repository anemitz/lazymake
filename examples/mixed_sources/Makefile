# This example builds one binary from .cc, .cpp, .cxx, and .c sources and sets
# per-target compile flags.
BINS := mixed
mixed_SOURCES := Main.cc Greeter.cpp Helper.cxx util.c
mixed_CXXFLAGS := -DMIXED_EXAMPLE
mixed_CFLAGS := -DMIXED_C_EXAMPLE

include ../../Makefile.inc
