#!/bin/sh

set -eu

ROOT=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
MAKE=${MAKE:-make}
HELLO_HEADER=$ROOT/examples/main_deliverable/app/Hello.h
FIXTURE=

cleanup()
{
    if [ -f "$HELLO_HEADER.orig" ]; then
        mv "$HELLO_HEADER.orig" "$HELLO_HEADER"
    fi
    "$MAKE" -C "$ROOT" -f Makefile.inc distclean >/dev/null
    if [ -n "$FIXTURE" ] && [ -d "$FIXTURE" ]; then
        rm -rf "$FIXTURE"
    fi
}

pass()
{
    printf 'ok - %s\n' "$1"
}

fail()
{
    printf 'not ok - %s\n' "$1" >&2
    exit 1
}

assert_equal()
{
    actual=$1
    expected=$2
    label=$3

    if [ "$actual" = "$expected" ]; then
        pass "$label"
    else
        printf 'expected: %s\nactual:   %s\n' "$expected" "$actual" >&2
        fail "$label"
    fi
}

assert_contains()
{
    value=$1
    expected=$2
    label=$3

    case $value in
        *"$expected"*) pass "$label" ;;
        *) fail "$label (missing $expected in $value)" ;;
    esac
}

assert_not_contains()
{
    value=$1
    unexpected=$2
    label=$3

    case $value in
        *"$unexpected"*) fail "$label (found $unexpected in $value)" ;;
        *) pass "$label" ;;
    esac
}

config_value()
{
    component=$1
    key=$2
    shift 2

    printf '%s\n' \
        '.PHONY: _lazymake_test_value' \
        '_lazymake_test_value:' \
        "	@printf '%s\\n' '\$($key)'" |
        "$MAKE" -C "$component" --no-print-directory \
            -f Makefile -f - "$@" _lazymake_test_value
}

trap cleanup EXIT HUP INT TERM
cleanup

printf 'Testing native %s/%s\n' "$(uname -s)" "$(uname -m)"

"$MAKE" -C "$ROOT/examples/main_deliverable"
"$MAKE" -C "$ROOT/examples/shared_lib"
"$MAKE" -C "$ROOT/examples/mixed_sources"
test_output=$("$MAKE" -C "$ROOT/examples/test_suite" check)
printf '%s\n' "$test_output"

package=$(config_value "$ROOT/examples/main_deliverable/app" PKGDIR)
build_root=$(config_value "$ROOT/examples/main_deliverable/app" BUILD_ROOT)
hello_build=$(config_value "$ROOT/examples/main_deliverable/app" BUILDDIR)

[ -x "$package/bin/hello" ] || fail 'binary is packaged'
pass 'binary is packaged'
[ -f "$package/lib/libecho.so" ] || fail 'shared library is packaged'
pass 'shared library is packaged'
[ -x "$package/bin/mixed" ] || fail 'mixed-source binary is packaged'
pass 'mixed-source binary is packaged'
assert_equal "$("$package/bin/hello")" 'hello from lazymake' 'packaged binary runs'
assert_equal "$("$package/bin/mixed")" 'hello from mixed sources' 'mixed binary runs'
assert_contains "$test_output" 'test suite passed' 'check runs declared tests'

smoke_build=$(config_value "$ROOT/examples/test_suite" BUILDDIR)
[ -x "$smoke_build/tests/SmokeTest" ] || fail 'test executable stays in the build tree'
pass 'test executable stays in the build tree'
[ ! -e "$package/bin/SmokeTest" ] || fail 'test executable is excluded from the package'
pass 'test executable is excluded from the package'

expected_root=$ROOT/build/$(uname -s)-$(uname -m)/release
assert_equal "$build_root" "$expected_root" 'build root records host and variant'
assert_equal \
    "$hello_build" \
    "$expected_root/examples/main_deliverable/app" \
    'component build state is centrally namespaced'

second_build=$("$MAKE" -C "$ROOT/examples/shared_lib")
assert_not_contains "$second_build" ' -c ' 'unchanged build does not compile'
assert_not_contains "$second_build" ' -o ' 'unchanged build does not link'
assert_not_contains "$second_build" 'cp ' 'direct package outputs need no copy'

sleep 1
cp "$HELLO_HEADER" "$HELLO_HEADER.orig"
printf '#define HELLO_GREETING "rebuilt after header edit"\n' >"$HELLO_HEADER"
"$MAKE" -C "$ROOT/examples/main_deliverable"
mv "$HELLO_HEADER.orig" "$HELLO_HEADER"
assert_equal \
    "$("$package/bin/hello")" \
    'rebuilt after header edit' \
    'header edits rebuild dependent package outputs'

mixed_dryrun=$("$MAKE" -C "$ROOT/examples/mixed_sources" -n -B)
greeter_compile=$(printf '%s\n' "$mixed_dryrun" | awk '/Greeter\.cpp/{print; exit}')
util_compile=$(printf '%s\n' "$mixed_dryrun" | awk '/util\.c/{print; exit}')
assert_contains "$greeter_compile" '-DMIXED_EXAMPLE' 'target CXXFLAGS reach C++ sources'
assert_not_contains "$greeter_compile" '-DMIXED_C_EXAMPLE' 'C++ sources exclude target CFLAGS'
assert_contains "$util_compile" '-DMIXED_C_EXAMPLE' 'target CFLAGS reach C sources'
assert_not_contains "$util_compile" '-DMIXED_EXAMPLE' 'C sources exclude target CXXFLAGS'

debug_dir=$(config_value "$ROOT/examples/main_deliverable/app" BUILDDIR BUILD_VARIANT=debug)
assert_contains "$debug_dir" '/debug/' 'BUILD_VARIANT isolates build state'
debug_package=$(config_value "$ROOT/examples/main_deliverable/app" PKGDIR BUILD_VARIANT=debug)
assert_contains "$debug_package" '/debug' 'BUILD_VARIANT isolates package state'

custom_flags=$("$MAKE" -C "$ROOT/examples/main_deliverable/app" -n -B CXXFLAGS=-O0)
assert_contains "$custom_flags" '-O3' 'LazyMake supplies default optimization'
assert_contains "$custom_flags" '-O0' 'project flags follow LazyMake defaults'

darwin_package=$(config_value \
    "$ROOT/examples/main_deliverable/app" \
    PKGDIR \
    HOST_OS=Darwin \
    HOST_ARCH=arm64)
assert_contains "$darwin_package" '/Darwin-arm64/release' 'host overrides select package identity'

FIXTURE=$(mktemp -d "${TMPDIR:-/tmp}/lazymake-v2.XXXXXX")
cp "$ROOT/Makefile.inc" "$FIXTURE/Makefile.inc"
mkdir -p \
    "$FIXTURE/app/src" \
    "$FIXTURE/assets/conf" \
    "$FIXTURE/assets/web" \
    "$FIXTURE/tests" \
    "$FIXTURE/functional" \
    "$FIXTURE/failure"

cat >"$FIXTURE/Build.mk" <<'EOF'
PROJECT_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
CPPFLAGS += -I$(PROJECT_ROOT) -D$(HOST_OS)
include $(PROJECT_ROOT)/Makefile.inc
EOF

cat >"$FIXTURE/Makefile" <<'EOF'
SUBDIRS := app assets
CHECKDIRS := tests functional

include Build.mk
EOF

cat >"$FIXTURE/app/Makefile" <<'EOF'
BINS := app alpha beta nested collision
STATICLIBS := libsupport

app_SOURCES := app.cc
app_STATICLIBS = $(PKGDIR)/lib/libsupport.a
alpha_SOURCES := alpha.cc shared.cc
alpha_CXXFLAGS := -DALPHA
beta_SOURCES := beta.cc shared.cc
beta_CXXFLAGS := -DBETA
nested_SOURCES := src/nested.cc
collision_SOURCES := collision-main.cc collision.c collision.cc

include ../Build.mk
EOF

cat >"$FIXTURE/app/libsupport.cc" <<'EOF'
int support_value(void)
{
    return 11;
}
EOF

cat >"$FIXTURE/app/app.cc" <<'EOF'
extern int support_value(void);

int main(void)
{
    return support_value() == 11 ? 0 : 1;
}
EOF

cat >"$FIXTURE/app/shared.cc" <<'EOF'
#if defined(ALPHA) && defined(BETA)
#error target flags leaked across shared sources
#endif
#if !defined(ALPHA) && !defined(BETA)
#error target flag is missing
#endif
int shared_value(void)
{
    return 7;
}
EOF

cat >"$FIXTURE/app/alpha.cc" <<'EOF'
extern int shared_value(void);
int main(void) { return shared_value() == 7 ? 0 : 1; }
EOF

cat >"$FIXTURE/app/beta.cc" <<'EOF'
extern int shared_value(void);
int main(void) { return shared_value() == 7 ? 0 : 1; }
EOF

cat >"$FIXTURE/app/src/nested.cc" <<'EOF'
int main(void) { return 0; }
EOF

cat >"$FIXTURE/app/collision.c" <<'EOF'
int c_value(void) { return 3; }
EOF

cat >"$FIXTURE/app/collision.cc" <<'EOF'
int cpp_value(void) { return 4; }
EOF

cat >"$FIXTURE/app/collision-main.cc" <<'EOF'
extern "C" int c_value(void);
extern int cpp_value(void);
int main(void) { return c_value() + cpp_value() == 7 ? 0 : 1; }
EOF

printf 'fixture resource\n' >"$FIXTURE/assets/conf/settings.txt"
printf 'fixture web\n' >"$FIXTURE/assets/web/index.txt"
cat >"$FIXTURE/assets/Makefile" <<'EOF'
RESOURCES := conf web

.PHONY: generated
package: generated
generated: resources
	@printf 'generated after resources\n' >"$(PKGDIR)/conf/generated.txt"

include ../Build.mk
EOF

cat >"$FIXTURE/tests/Makefile" <<'EOF'
TESTS := ConventionTest EnvironmentTest
CHECK_ENV := REQUIRED_CHECK_ENV=present

include ../Build.mk
EOF

cat >"$FIXTURE/tests/ConventionTest.cc" <<'EOF'
int main(void) { return 0; }
EOF

cat >"$FIXTURE/tests/EnvironmentTest.cc" <<'EOF'
#include <stdlib.h>
#include <string.h>
int main(void)
{
    const char *value = getenv("REQUIRED_CHECK_ENV");
    return value != 0 && strcmp(value, "present") == 0 ? 0 : 1;
}
EOF

cat >"$FIXTURE/functional/Makefile" <<'EOF'
.PHONY: check
check:
	@test -x "$(PKGDIR)/bin/app"
	@printf 'functional check ran\n' >"$(PROJECT_ROOT)/functional-ran"

include ../Build.mk
EOF

cat >"$FIXTURE/failure/Makefile" <<'EOF'
TESTS := FailTest LaterTest
CHECK_ENV = LATER_MARKER="$(PROJECT_ROOT)/later-ran"

include ../Build.mk
EOF

cat >"$FIXTURE/failure/FailTest.cc" <<'EOF'
int main(void) { return 1; }
EOF

cat >"$FIXTURE/failure/LaterTest.cc" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
int main(void)
{
    const char *path = getenv("LATER_MARKER");
    FILE *marker = path == 0 ? 0 : fopen(path, "w");
    if (marker == 0) return 1;
    fputs("later test ran\n", marker);
    fclose(marker);
    return 0;
}
EOF

"$MAKE" -j8 -C "$FIXTURE" package
fixture_package=$(config_value "$FIXTURE/app" PKGDIR)

[ -x "$fixture_package/bin/app" ] || fail 'package includes default and explicit binaries'
pass 'package includes default and explicit binaries'
[ -f "$fixture_package/lib/libsupport.a" ] || fail 'package includes static libraries'
pass 'package includes static libraries'
[ -f "$fixture_package/conf/settings.txt" ] || fail 'resources are packaged'
pass 'resources are packaged'
[ -f "$fixture_package/conf/generated.txt" ] || fail 'generated content waits for resources'
pass 'generated content waits for resources'

"$fixture_package/bin/app" || fail 'binary links a same-component static library'
"$fixture_package/bin/alpha" || fail 'first target owns its shared-source flags'
"$fixture_package/bin/beta" || fail 'second target owns its shared-source flags'
"$fixture_package/bin/nested" || fail 'nested sources build'
"$fixture_package/bin/collision" || fail 'same-basename C and C++ sources build'
pass 'target-owned objects cover shared, nested, and colliding sources'

"$MAKE" -C "$FIXTURE" check
[ -f "$FIXTURE/functional-ran" ] || fail 'CHECKDIRS run after package completion'
pass 'CHECKDIRS run after package completion'

if "$MAKE" -C "$FIXTURE/failure" check; then
    fail 'check fails when a declared test fails'
fi
[ -f "$FIXTURE/later-ran" ] || fail 'check continues after an earlier test fails'
pass 'check aggregates failures after running later tests'

"$MAKE" -C "$FIXTURE" BUILD_VARIANT=sanitize check
sanitize_package=$(config_value "$FIXTURE/app" PKGDIR BUILD_VARIANT=sanitize)
[ -x "$sanitize_package/bin/app" ] || fail 'BUILD_VARIANT reaches production subdirectories'
pass 'BUILD_VARIANT reaches production subdirectories'
sanitize_test_dir=$(config_value "$FIXTURE/tests" BUILDDIR BUILD_VARIANT=sanitize)
[ -x "$sanitize_test_dir/tests/ConventionTest" ] || fail 'BUILD_VARIANT reaches check directories'
pass 'BUILD_VARIANT reaches check directories'

"$MAKE" -C "$FIXTURE" clean
[ ! -d "$FIXTURE/build/$(uname -s)-$(uname -m)/release" ] || fail 'root clean removes current build variant'
pass 'root clean removes current build variant'
[ -x "$fixture_package/bin/app" ] || fail 'clean preserves the package'
pass 'clean preserves the package'

"$MAKE" -C "$FIXTURE" distclean
[ ! -d "$FIXTURE/build" ] || fail 'distclean removes all build variants'
[ ! -d "$FIXTURE/package" ] || fail 'distclean removes all package variants'
pass 'distclean removes all generated state'

pass 'platform harness completed'
