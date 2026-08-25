# This example builds a static library in one component and refreshes it from
# another through EXTERNAL_STATICLIBS. The parent still orders the producer
# subdirectory before the consumer for a parallel full build.
SUBDIRS := lib app
app: lib

include ../../Makefile.inc
