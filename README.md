# LazyMake

**Declare what your C or C++ project builds. Keep how it builds in one shared
file.**

```make
BINS := hello
hello_SOURCES := Hello.cc

include ../Makefile.inc
```

```text
component Makefile ──include──▶ Makefile.inc ──▶ build/… + package/…
     project data                  mechanics           artifacts
```

Run `make`. You get:

- Incremental builds with automatic header tracking
- Executables, libraries, tests, and packaged resources
- Predictable per-platform artifact directories
- Standard compiler and linker overrides

No configure step. No generator. Just GNU Make and a compiler.

## See what disappears

A two-source executable with header tracking:

```diff
- CXXFLAGS += -g -O3 -std=c++17 -pedantic -Wall
- SOURCES := Main.cc Greeter.cc
- OBJECTS := $(SOURCES:%.cc=build/%.o)
- DEPENDS := $(OBJECTS:.o=.d)
-
- app: $(OBJECTS)
-	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
-
- build/%.o: %.cc
-	@mkdir -p $(@D)
-	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@
-
- -include $(DEPENDS)
+ BINS := app
+ app_SOURCES := Main.cc Greeter.cc
+
+ include ../Makefile.inc
```

The removed mechanics live once in `Makefile.inc`, not in every component.

## Grow by adding declarations

```make
BINS := server
DYNLIBS := libmetrics
STATICLIBS := libprotocol
TESTS := ParserTest SocketTest

server_SOURCES := Main.cc Server.cc
libmetrics_SOURCES := Metrics.cc
libprotocol_SOURCES := Frame.cc Parser.cc

SUBDIRS := tools
CHECKDIRS := functionaltests
RESOURCES := conf

include Makefile.inc
```

```text
make          production package + local tests
make check    production package + every test
```

## Pick the right tool

| Need | Use |
| --- | --- |
| Tiny or unusual build | Hand-written Make |
| Small, repeatable C/C++ build | **LazyMake** |
| Source packages that probe varied Unix hosts or features | Autoconf and Automake |
| Package discovery, exports, or IDE generation | CMake or Meson |
| Hermetic builds, remote execution, or shared caching | Bazel or similar |

## Reference

- Include `Makefile.inc` last. Its location defines the project root.
- Targets: `BINS`, `DYNLIBS`, `STATICLIBS`, and `TESTS`.
- Composition: `SUBDIRS`, `CHECKDIRS`, and `RESOURCES`.
- Sources: `<target>_SOURCES`; defaults to `<target>.cc`.
- Extensions: `.c`, `.cc`, `.cpp`, and `.cxx`.
- Global settings: `CC`, `CXX`, `CPPFLAGS`, `CFLAGS`, `CXXFLAGS`, `LDFLAGS`,
  and `LDLIBS`.
- Per-target settings: `<target>_CPPFLAGS`, `<target>_CFLAGS`,
  `<target>_CXXFLAGS`, `<target>_LDFLAGS`, `<target>_LDLIBS`, and
  `<target>_STATICLIBS`.
- Groups: `TARGET_GROUPS`, `<group>_TARGETS`, and `<group>_STATICLIBS`. A
  group applies archive paths to named binaries, shared libraries, or tests
  as both link inputs and prerequisites. Per-target `_STATICLIBS` come first,
  then matching groups in `TARGET_GROUPS` order. Use `=` when an archive path
  contains `$(PKGDIR)`.

```make
TARGET_GROUPS := core
core_TARGETS := app SmokeTest
core_STATICLIBS = $(PKGDIR)/lib/libcore.a
```

- External archives: `EXTERNAL_STATICLIBS`, `<lib>_PATH`, and `<lib>_DIR`.
  Names are the owning component's `STATICLIBS` targets. A path that appears in
  an effective `_STATICLIBS` list is refreshed by recursing into `_DIR` for
  that name, unless this Makefile itself lists the name in `STATICLIBS`. Use
  `=` when `_PATH` contains `$(PKGDIR)` or `_DIR` contains `$(PROJECT_ROOT)`.
  Parent `SUBDIRS` still need an explicit producer-before-consumer order for
  `make -j`. Concurrent standalone consumer builds that share a stale archive
  can both recurse into the owner and race on `ar`; that is out of contract.

```make
EXTERNAL_STATICLIBS := libcore
libcore_PATH = $(PKGDIR)/lib/libcore.a
libcore_DIR = $(PROJECT_ROOT)/lib
```

| Command | Result |
| --- | --- |
| `make` | Package plus local tests |
| `make package` | Production targets, resources, and `SUBDIRS` |
| `make check` | Build and run all tests |
| `make <name>` | One declared target |
| `make clean` | Current component's build state |
| `make distclean` | Every build and package variant |

Separate incompatible artifacts with `BUILD_VARIANT`:

```sh
make BUILD_VARIANT=debug CXXFLAGS=-O0
```

```text
build/<os>-<architecture>/<variant>/<component>/<target>/
package/<os>-<architecture>/<variant>/{bin,lib}/
```

## Try it

```sh
make -C examples/main_deliverable
make -C examples/shared_lib
make -C examples/mixed_sources
make -C examples/test_suite check
make -C examples/target_groups check
make -C examples/external_staticlibs
```

Requires GNU Make 3.81+, a C/C++ toolchain, and basic host utilities. Validate
LazyMake with `make -C tests host`; use `make -C tests docker` for Ubuntu ARM64 and
AMD64.
