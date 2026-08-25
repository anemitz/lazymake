# Platform test harness

The harness builds the binary, shared-library, mixed-source, test-suite,
target-group, and external-static-library examples. It also exercises a
temporary multi-component project to verify:

- Centrally namespaced, target-owned objects
- Default and explicit source declarations
- Binaries, shared libraries, static libraries, and tests
- Standard and target-specific compiler and linker variables
- Named static-library target groups and unrelated-target relink isolation
- External static libraries refreshed from the owning component
- Complete resources and generated package content
- Production `SUBDIRS` and isolated `CHECKDIRS`
- Aggregated test failures and test environments
- Parallel builds, header dependencies, and incremental behavior
- Build variant propagation and cleanup semantics

Run it directly on the current host:

```sh
make -C tests host
```

On macOS, this is the native Darwin test. Docker cannot test a Darwin kernel because
Docker Desktop runs Linux virtual machines.

Run the clean Ubuntu tests for both tested CPU architectures:

```sh
make -C tests docker
```

The Docker target builds separate Ubuntu 24.04 images for `linux/arm64` and
`linux/amd64`. Each image contains only G++, GNU Make, and their required base
packages.
