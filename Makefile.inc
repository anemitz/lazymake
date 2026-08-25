# -*-Mode: makefile;-*-

# Requires GNU Make 3.81 or greater.

.DEFAULT_GOAL := all

# A project wrapper may set PROJECT_ROOT before including this file. When
# Makefile.inc lives at the project root, its own location is the default.
PROJECT_ROOT ?= $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
PROJECT_ROOT := $(abspath $(PROJECT_ROOT))

HOST_OS ?= $(shell uname -s)
HOST_ARCH ?= $(shell uname -m)
BUILD_VARIANT ?= release

BUILD_ROOT ?= $(PROJECT_ROOT)/build/$(HOST_OS)-$(HOST_ARCH)/$(BUILD_VARIANT)
PACKAGE_ROOT ?= $(PROJECT_ROOT)/package/$(HOST_OS)-$(HOST_ARCH)/$(BUILD_VARIANT)

ifeq ($(CURDIR),$(PROJECT_ROOT))
    COMPONENT := root
else
    COMPONENT := $(patsubst $(PROJECT_ROOT)/%,%,$(CURDIR))
endif

BUILDDIR := $(BUILD_ROOT)/$(COMPONENT)
PKGDIR := $(PACKAGE_ROOT)

ifeq ($(origin CC),default)
    CC := gcc
endif
ifeq ($(origin CXX),default)
    CXX := g++
endif

# LazyMake defaults precede project flags so normal CPPFLAGS, CFLAGS,
# CXXFLAGS, and LDFLAGS can extend or override them.
LAZYMAKE_CPPFLAGS := -pedantic -Wall
LAZYMAKE_CFLAGS := -g -O3
LAZYMAKE_CXXFLAGS := -g -O3 -std=c++17

SHARED_LINK_FLAGS := -shared
SHARED_INSTALL_NAME :=
ifeq ($(HOST_OS),Darwin)
    SHARED_LINK_FLAGS := -dynamiclib
    SHARED_INSTALL_NAME = -install_name @rpath/$(@F)
endif

ALL_TARGETS := $(BINS) $(DYNLIBS) $(STATICLIBS) $(TESTS)

# A target named foo builds foo.cc unless it declares foo_SOURCES.
target_sources = $(if $(strip $($(1)_SOURCES)),$($(1)_SOURCES),$(1).cc)
target_objects = $(foreach source,$(call target_sources,$(1)),$(BUILDDIR)/$(1)/$(source).o)

# Per-target archives, then matching groups in TARGET_GROUPS order. Recursive
# values expand here so $(PKGDIR) works when assigned with =. Static-library
# targets are not group members; they are not linked.
target_staticlibs = $(strip $($(1)_STATICLIBS) $(foreach group,$(TARGET_GROUPS),$(if $(filter $(1),$($(group)_TARGETS)),$($(group)_STATICLIBS))))

ALL_SOURCES := $(foreach target,$(ALL_TARGETS),$(call target_sources,$(target)))
UNSUPPORTED_SOURCES := $(filter-out %.c %.cc %.cpp %.cxx,$(ALL_SOURCES))
ifneq ($(strip $(UNSUPPORTED_SOURCES)),)
    $(error unsupported source extension: $(UNSUPPORTED_SOURCES))
endif

# Group names share the <name>_STATICLIBS form with targets, so a collision
# would make one variable mean two things.
GROUP_NAME_COLLISION := $(filter $(TARGET_GROUPS),$(ALL_TARGETS))
ifneq ($(strip $(GROUP_NAME_COLLISION)),)
    $(error TARGET_GROUPS name collides with a target: $(GROUP_NAME_COLLISION))
endif

UNKNOWN_GROUP_TARGETS := $(filter-out $(BINS) $(DYNLIBS) $(TESTS),$(foreach group,$(TARGET_GROUPS),$($(group)_TARGETS)))
ifneq ($(strip $(UNKNOWN_GROUP_TARGETS)),)
    $(error TARGET_GROUPS member is not a binary, shared library, or test: $(UNKNOWN_GROUP_TARGETS))
endif

bin_path = $(PKGDIR)/bin/$(1)
dynlib_path = $(PKGDIR)/lib/$(1).so
staticlib_path = $(PKGDIR)/lib/$(1).a
test_path = $(BUILDDIR)/tests/$(1)

BIN_OUTPUTS := $(foreach target,$(BINS),$(call bin_path,$(target)))
DYNLIB_OUTPUTS := $(foreach target,$(DYNLIBS),$(call dynlib_path,$(target)))
STATICLIB_OUTPUTS := $(foreach target,$(STATICLIBS),$(call staticlib_path,$(target)))
TEST_OUTPUTS := $(foreach target,$(TESTS),$(call test_path,$(target)))
PACKAGE_OUTPUTS := $(BIN_OUTPUTS) $(DYNLIB_OUTPUTS) $(STATICLIB_OUTPUTS)

.PHONY: all package resources check clean distclean
all: package $(TEST_OUTPUTS)

# package means that every production artifact, resource, and component is
# complete. Tests belong to check and never enter the package tree.
package: $(PACKAGE_OUTPUTS) resources $(SUBDIRS)

resources:
ifneq ($(strip $(RESOURCES)),)
	@mkdir -p "$(PKGDIR)"
	@status=0; for resource in $(RESOURCES); do \
		cp -R "$$resource" "$(PKGDIR)/" || status=1; \
	done; exit $$status
endif

check: all
ifneq ($(strip $(CHECKDIRS)),)
	@status=0; \
	$(foreach test,$(TESTS),$(CHECK_ENV) "$(call test_path,$(test))" || status=1;) \
	$(foreach dir,$(CHECKDIRS),$(MAKE) -C "$(dir)" check || status=1;) \
	exit $$status
else
    ifneq ($(strip $(TESTS)),)
	@status=0; \
	$(foreach test,$(TESTS),$(CHECK_ENV) "$(call test_path,$(test))" || status=1;) \
	exit $$status
    endif
endif

ifeq ($(CURDIR),$(PROJECT_ROOT))
    CLEAN_ROOT := $(BUILD_ROOT)
else
    CLEAN_ROOT := $(BUILDDIR)
endif

clean:
	@rm -rf "$(CLEAN_ROOT)"

distclean:
	@rm -rf "$(PROJECT_ROOT)/build" "$(PROJECT_ROOT)/package"

.PHONY: $(SUBDIRS)
$(SUBDIRS):
	+@$(MAKE) -C "$@" package

# Compile one object for each target/source pair. Keeping the source extension
# in the object name prevents foo.c and foo.cc from colliding.
define OBJECT_template
$(BUILDDIR)/$(1)/$(2).o: $(2)
	@mkdir -p "$$(@D)"
	$(if $(filter %.c,$(2)),$$(CC) -c $$(LAZYMAKE_CPPFLAGS) $$(CPPFLAGS) $$(LAZYMAKE_CFLAGS) $$(CFLAGS) $$($(1)_CPPFLAGS) $$($(1)_CFLAGS) $(3),$$(CXX) -c $$(LAZYMAKE_CPPFLAGS) $$(CPPFLAGS) $$(LAZYMAKE_CXXFLAGS) $$(CXXFLAGS) $$($(1)_CPPFLAGS) $$($(1)_CXXFLAGS) $(3)) -MMD -MP -MF "$$@.d" -MT "$$@" "$$<" -o "$$@"
endef

define TARGET_OBJECTS_template
$(foreach source,$(call target_sources,$(1)),$(eval $(call OBJECT_template,$(1),$(source),$(2))))
endef

$(foreach target,$(BINS),$(eval $(call TARGET_OBJECTS_template,$(target),)))
$(foreach target,$(DYNLIBS),$(eval $(call TARGET_OBJECTS_template,$(target),-fPIC)))
$(foreach target,$(STATICLIBS),$(eval $(call TARGET_OBJECTS_template,$(target),)))
$(foreach target,$(TESTS),$(eval $(call TARGET_OBJECTS_template,$(target),)))

-include $(addsuffix .d,$(foreach target,$(ALL_TARGETS),$(call target_objects,$(target))))

define BIN_template
.PHONY: $(1)
$(1): $(call bin_path,$(1))

$(call bin_path,$(1)): $$(call target_objects,$(1)) $$(call target_staticlibs,$(1))
	@mkdir -p "$$(@D)"
	$$(CXX) $$(call target_objects,$(1)) $$(LAZYMAKE_LDFLAGS) $$(LDFLAGS) $$($(1)_LDFLAGS) $$(call target_staticlibs,$(1)) $$(LDLIBS) $$($(1)_LDLIBS) -o "$$@"
endef

define DYNLIB_template
.PHONY: $(1)
$(1): $(call dynlib_path,$(1))

$(call dynlib_path,$(1)): $$(call target_objects,$(1)) $$(call target_staticlibs,$(1))
	@mkdir -p "$$(@D)"
	$$(CXX) $$(SHARED_LINK_FLAGS) $$(SHARED_INSTALL_NAME) -o "$$@" $$(call target_objects,$(1)) $$(LAZYMAKE_LDFLAGS) $$(LDFLAGS) $$($(1)_LDFLAGS) $$(call target_staticlibs,$(1)) $$(LDLIBS) $$($(1)_LDLIBS)
endef

define STATICLIB_template
.PHONY: $(1)
$(1): $(call staticlib_path,$(1))

$(call staticlib_path,$(1)): $$(call target_objects,$(1))
	@mkdir -p "$$(@D)"
	@rm -f "$$@"
	$$(AR) rcs "$$@" $$(call target_objects,$(1))
endef

define TEST_template
.PHONY: $(1)
$(1): $(call test_path,$(1))

$(call test_path,$(1)): $$(call target_objects,$(1)) $$(call target_staticlibs,$(1))
	@mkdir -p "$$(@D)"
	$$(CXX) $$(call target_objects,$(1)) $$(LAZYMAKE_LDFLAGS) $$(LDFLAGS) $$($(1)_LDFLAGS) $$(call target_staticlibs,$(1)) $$(LDLIBS) $$($(1)_LDLIBS) -o "$$@"
endef

$(foreach target,$(BINS),$(eval $(call BIN_template,$(target))))
$(foreach target,$(DYNLIBS),$(eval $(call DYNLIB_template,$(target))))
$(foreach target,$(STATICLIBS),$(eval $(call STATICLIB_template,$(target))))
$(foreach target,$(TESTS),$(eval $(call TEST_template,$(target))))
