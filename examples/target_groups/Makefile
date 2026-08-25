# This example links one static library into a named group of consumers and
# leaves an unrelated binary unlinked from it.
BINS := app extra
TESTS := SmokeTest
STATICLIBS := libcore

TARGET_GROUPS := core
core_TARGETS := app SmokeTest
core_STATICLIBS = $(PKGDIR)/lib/libcore.a

include ../../Makefile.inc
