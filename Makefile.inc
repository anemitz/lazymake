# -*-Mode: makefile;-*-

# Requires GNU Make 3.81 or greater.

.DEFAULT_GOAL := all

# A project wrapper may set PROJECT_ROOT before including this file. When
# Makefile.inc lives at the project root, its own location is the default.
PROJECT_ROOT ?= $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
PROJECT_ROOT := $(abspath $(PROJECT_ROOT))

# Resolve the host once. Recursive Makes inherit it instead of forking again.
HOST_OS ?= $(shell uname -s)
HOST_OS := $(HOST_OS)
HOST_ARCH ?= $(shell uname -m)
HOST_ARCH := $(HOST_ARCH)
export HOST_OS HOST_ARCH

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
# ../ cannot reach an object path: it would leave the target's directory, and
# targets sharing that source would then write one file.
object_path = $(BUILDDIR)/$(1)/$(subst ../,__/,$(2)).o
target_objects = $(foreach source,$(call target_sources,$(1)),$(call object_path,$(1),$(source)))

# Per-target archives, then matching groups in TARGET_GROUPS order. Recursive
# values expand here so $(PKGDIR) works when assigned with =. Static-library
# targets are not group members; they are not linked.
target_staticlibs = $(strip $($(1)_STATICLIBS) $(foreach group,$(TARGET_GROUPS),$(if $(filter $(1),$($(group)_TARGETS)),$($(group)_STATICLIBS))))

ALL_SOURCES := $(foreach target,$(ALL_TARGETS),$(call target_sources,$(target)))
ALL_OBJECTS := $(foreach target,$(ALL_TARGETS),$(call target_objects,$(target)))
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

EXTERNAL_NAME_COLLISION := $(filter $(EXTERNAL_STATICLIBS),$(BINS) $(DYNLIBS) $(TESTS) $(TARGET_GROUPS))
ifneq ($(strip $(EXTERNAL_NAME_COLLISION)),)
    $(error EXTERNAL_STATICLIBS name collides with a target or group: $(EXTERNAL_NAME_COLLISION))
endif

bin_path = $(PKGDIR)/bin/$(1)
dynlib_path = $(PKGDIR)/lib/$(1).so
staticlib_path = $(PKGDIR)/lib/$(1).a
test_path = $(BUILDDIR)/tests/$(1)
# Link and archive outputs keep their command records in the build tree.
command_record_path = $(patsubst %,$(BUILDDIR)/%.cmd,$(notdir $(1)))

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

# Everything in all finishes first, so no suite races a link. The second Make
# keeps going past a failed suite so the status reports all of them. Suites
# overlap under -j; tests within a component stay serial and in order.
CHECK_SUITES := lazymake-check-tests $(addprefix lazymake-check-dir-,$(CHECKDIRS))

check: all
ifneq ($(strip $(TESTS) $(CHECKDIRS)),)
	+@$(MAKE) -k lazymake-check-suites
endif

.PHONY: lazymake-check-suites $(CHECK_SUITES)
lazymake-check-suites: $(CHECK_SUITES)

lazymake-check-tests:
	@status=0; \
	$(foreach test,$(TESTS),$(CHECK_ENV) "$(call test_path,$(test))" || status=1;) \
	exit $$status

$(addprefix lazymake-check-dir-,$(CHECKDIRS)): lazymake-check-dir-%:
	+@$(MAKE) -C "$*" check

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
# in the object name prevents foo.c and foo.cc from colliding. Each output
# declares its command here and gets its recipe from the command records below.
define OBJECT_template
lazymake_command.$(call object_path,$(1),$(2)) = $(if $(filter %.c,$(2)),$$(CC) -c $$(LAZYMAKE_CPPFLAGS) $$(CPPFLAGS) $$(LAZYMAKE_CFLAGS) $$(CFLAGS) $$($(1)_CPPFLAGS) $$($(1)_CFLAGS) $(3),$$(CXX) -c $$(LAZYMAKE_CPPFLAGS) $$(CPPFLAGS) $$(LAZYMAKE_CXXFLAGS) $$(CXXFLAGS) $$($(1)_CPPFLAGS) $$($(1)_CXXFLAGS) $(3)) -MMD -MP -MF "$$@.d" -MT "$$@" "$(2)" -o "$$@"
$(call object_path,$(1),$(2)): $(2)
endef

define TARGET_OBJECTS_template
$(foreach source,$(call target_sources,$(1)),$(eval $(call OBJECT_template,$(1),$(source),$(2))))
endef

$(foreach target,$(BINS),$(eval $(call TARGET_OBJECTS_template,$(target),)))
$(foreach target,$(DYNLIBS),$(eval $(call TARGET_OBJECTS_template,$(target),-fPIC)))
$(foreach target,$(STATICLIBS),$(eval $(call TARGET_OBJECTS_template,$(target),)))
$(foreach target,$(TESTS),$(eval $(call TARGET_OBJECTS_template,$(target),)))

# Binaries and tests link the same way.
link_command = $(CXX) $(call target_objects,$(1)) $(LAZYMAKE_LDFLAGS) $(LDFLAGS) $($(1)_LDFLAGS) $(call target_staticlibs,$(1)) $(LDLIBS) $($(1)_LDLIBS) -o "$@"

define BIN_template
.PHONY: $(1)
$(1): $(call bin_path,$(1))

lazymake_command.$(call bin_path,$(1)) = $$(call link_command,$(1))
$(call bin_path,$(1)): $$(call target_objects,$(1)) $$(call target_staticlibs,$(1))
endef

define DYNLIB_template
.PHONY: $(1)
$(1): $(call dynlib_path,$(1))

lazymake_command.$(call dynlib_path,$(1)) = $$(CXX) $$(SHARED_LINK_FLAGS) $$(SHARED_INSTALL_NAME) -o "$$@" $$(call target_objects,$(1)) $$(LAZYMAKE_LDFLAGS) $$(LDFLAGS) $$($(1)_LDFLAGS) $$(call target_staticlibs,$(1)) $$(LDLIBS) $$($(1)_LDLIBS)
$(call dynlib_path,$(1)): $$(call target_objects,$(1)) $$(call target_staticlibs,$(1))
endef

# Rebuilding the archive from scratch drops members whose sources the target
# no longer lists.
define STATICLIB_template
.PHONY: $(1)
$(1): $(call staticlib_path,$(1))

lazymake_command.$(call staticlib_path,$(1)) = rm -f "$$@" && $$(AR) rcs "$$@" $$(call target_objects,$(1))
$(call staticlib_path,$(1)): $$(call target_objects,$(1))
endef

define TEST_template
.PHONY: $(1)
$(1): $(call test_path,$(1))

lazymake_command.$(call test_path,$(1)) = $$(call link_command,$(1))
$(call test_path,$(1)): $$(call target_objects,$(1)) $$(call target_staticlibs,$(1))
endef

$(foreach target,$(BINS),$(eval $(call BIN_template,$(target))))
$(foreach target,$(DYNLIBS),$(eval $(call DYNLIB_template,$(target))))
$(foreach target,$(STATICLIBS),$(eval $(call STATICLIB_template,$(target))))
$(foreach target,$(TESTS),$(eval $(call TEST_template,$(target))))

# Each output records the command of its last successful build. An output
# whose command no longer matches gains a phony prerequisite, so changed flags,
# sources, or link inputs rebuild it. Comparing during secondary expansion
# keeps -n, -q, and -B accurate. That expansion has no $<, and it cannot see a
# variable inherited from another target's target-specific assignment. The
# record holds the text the expansion saw, so such a variable goes uncompared
# rather than rebuilding on every run.
.SECONDEXPANSION:
.PHONY: lazymake-command-changed

# Empty only when the two texts are equal.
lazymake_differs = $(subst $(1),,$(2))$(subst $(2),,$(1))
lazymake_command_changed = $(eval lazymake_current.$@ := $$(strip $$(lazymake_command.$$@)))$(if $(call lazymake_differs,$(lazymake_current.$@),$(lazymake_built.$@)),lazymake-command-changed)

# A record is a makefile line, so it escapes $ and # in the command.
LAZYMAKE_HASH := \#
lazymake_record = lazymake_built.$@ := $(subst $(LAZYMAKE_HASH),$$(LAZYMAKE_HASH),$(subst $$,$$$$,$(lazymake_current.$@)))

# Run an output's command; once it succeeds, $(1) receives the record.
define lazymake_build
@mkdir -p "$(@D)"
$(lazymake_command.$@)
@printf '%s\n' '$(subst ','\'',$(lazymake_record))' $(1)
endef

# The compiler rewrites an object's dependency file on every compile, so the
# record joins that file rather than adding an include per object.
$(ALL_OBJECTS): $$(lazymake_command_changed)
	$(call lazymake_build,>>"$@.d")

$(PACKAGE_OUTPUTS) $(TEST_OUTPUTS): $$(lazymake_command_changed)
	$(call lazymake_build,>"$(call command_record_path,$@)")

-include $(addsuffix .d,$(ALL_OBJECTS)) $(call command_record_path,$(PACKAGE_OUTPUTS) $(TEST_OUTPUTS))

# Archives this component links. External refresh rules fire only for paths
# that appear here, so a shared table does not recurse from unrelated packages.
LINKED_STATICLIBS := $(sort $(foreach target,$(BINS) $(DYNLIBS) $(TESTS),$(call target_staticlibs,$(target))))

# Names this Makefile links but does not build. Membership in STATICLIBS is
# the owner: that Makefile already has a real archive recipe.
external_staticlib_used = $(if $(filter $(1),$(STATICLIBS)),,$(if $(filter $($(1)_PATH),$(LINKED_STATICLIBS)),$(1)))
USED_EXTERNAL_STATICLIBS := $(strip $(foreach lib,$(EXTERNAL_STATICLIBS),$(call external_staticlib_used,$(lib))))

$(foreach lib,$(USED_EXTERNAL_STATICLIBS),$(if $(strip $($(lib)_PATH)),,$(error EXTERNAL_STATICLIBS $(lib) has an empty _PATH)))
$(foreach lib,$(USED_EXTERNAL_STATICLIBS),$(if $(strip $($(lib)_DIR)),,$(error EXTERNAL_STATICLIBS $(lib) has an empty _DIR)))
$(foreach lib,$(USED_EXTERNAL_STATICLIBS),$(if $(filter /lib/% /bin/%,$($(lib)_PATH)),$(error EXTERNAL_STATICLIBS $(lib)_PATH looks like it expanded with an empty PKGDIR; assign _PATH with =)))

USED_EXTERNAL_PATHS := $(foreach lib,$(USED_EXTERNAL_STATICLIBS),$($(lib)_PATH))
$(foreach lib,$(USED_EXTERNAL_STATICLIBS),$(if $(filter-out 1,$(words $(filter $($(lib)_PATH),$(USED_EXTERNAL_PATHS)))),$(error duplicate EXTERNAL_STATICLIBS path: $($(lib)_PATH))))

# A phony proxy always asks the owning Makefile to check freshness. That
# Makefile and its dependency files know whether sources changed. The path
# recipe does not touch the archive, so a no-op refresh does not relink.
define EXTERNAL_STATICLIB_template
.PHONY: lazymake-external-$(1)
lazymake-external-$(1):
	+$$(MAKE) -C "$$($(1)_DIR)" $(1)

$$($(1)_PATH): lazymake-external-$(1)
	@test -f "$$@"
endef

$(foreach lib,$(USED_EXTERNAL_STATICLIBS),$(eval $(call EXTERNAL_STATICLIB_template,$(lib))))
