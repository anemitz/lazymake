# AGENTS.md

## The soul of LazyMake

LazyMake exists to make the common case pleasantly small. A project should describe
what it builds; `Makefile.inc` should carry the mechanics of how it is built.

When values compete, prefer them in this order:

1. Readability at the point of use.
2. A small, predictable public interface.
3. Simple, reusable mechanisms instead of repeated special cases.
4. Portability and minimal dependencies.

The best change usually removes something a consuming project must know, write, or
install without hiding behavior that it may need to understand.

## How LazyMake is used

Projects place `Makefile.inc` at their root. Each component declares its build data
and includes the shared file last:

```make
BINS := app
app_SOURCES := Main.cc

include ../Makefile.inc
```

The public declaration vocabulary is `BINS`, `DYNLIBS`, `STATICLIBS`, `TESTS`,
`<target>_SOURCES`, `SUBDIRS`, `CHECKDIRS`, `RESOURCES`, `BUILD_VARIANT`, standard
compiler and linker variables, and their target-specific forms. Preserve this small
interface and the rule that the common include comes last. Treat the README and the
runnable projects under `examples/` as the authoritative usage contract; update and
test them with every public behavior change.

## Design principles

- Keep component Makefiles declarative. They should mostly name targets, sources,
  dependencies, subdirectories, resources, and runnable commands, then include
  `Makefile.inc` as their final statement.
- Keep shared build mechanics in `Makefile.inc`. Do not make every project copy a
  recipe, platform check, flag transformation, or packaging rule.
- Preserve the top-down model: the parent declares components and the shared file
  derives the build graph. Automatic behavior must remain deterministic and easy to
  inspect with Make's normal tools.
- Favor one general rule or template over parallel rules that differ only by target
  name. Use Make's own substitution, filtering, pattern rules, and `define`/`call`
  facilities when they express the idea clearly.
- Earn every abstraction. Extract a concept when it makes several call sites
  smaller or gives one policy a single home. Do not introduce indirection merely to
  shorten one expression.
- Keep policy separate from project data. Global defaults and platform behavior
  belong in the common file; target lists and source lists belong in component
  Makefiles.
- Treat existing public variables and targets as an API. Prefer additive,
  unsurprising changes. If behavior must change, update the README and examples in
  the same change.

## Dependencies and portability

- Keep LazyMake's own requirements to GNU Make, the supported compiler toolchain,
  and basic host utilities already expected by the repository. Prefer GNU Make
  built-ins over spawning a shell utility for simple transformations.
- Do not require Python, Node, a package manager, or another helper runtime when GNU
  Make and existing host tools can express the behavior clearly.
- Retain compatibility with GNU Make 3.81 or greater unless the project's stated
  baseline is deliberately changed. Do not casually use newer Make syntax.
- Keep platform-specific logic narrow and explicit. Share the common path, isolate
  the exceptional assignment or flag, and document non-obvious compatibility
  constraints.
- Do not bake a consuming project's name, directory layout, or private libraries
  into the shared build system.

## Readability and style

- Use short sections and whitespace to reveal the file's structure. Keep related
  assignments and their conditionals together.
- Use uppercase names for public or shared configuration such as `BINS`,
  `BUILDDIR`, and `CXXFLAGS`. Use the established `<target>_SOURCES` form for
  target-specific data. Choose full, concrete names over new abbreviations.
- Use `:=` for values intentionally resolved at definition time, `+=` to extend a
  value, and recursive `=` only when later expansion is part of the design. Avoid
  mixing assignment forms without a semantic reason.
- Indent Make conditionals consistently with four spaces. Recipe lines require one
  literal tab. Keep shell fragments simple enough to read as a single operation.
- Prefer direct expressions over dense nesting. If an expression needs a paragraph
  to explain it, give the concept a good name or split the work into clear stages.
- Remove dead branches and stale commented-out implementations. Preserve a comment
  only when it records a real compatibility constraint or design reason.
- Comments should be concise sentences that explain intent, ordering, or a
  surprising constraint. Do not narrate syntax already visible on the next line.
- Match surrounding style in focused edits; do not combine a behavior change with
  unrelated reformatting.

## Ease of use

- Optimize for the smallest useful component Makefile. Common builds should need
  declarations, not custom recipes.
- Provide sensible defaults while preserving intentional command-line overrides.
  A project should be able to customize normal compiler and linker settings without
  editing the shared file.
- Keep generated artifacts under the derived build and package directories. Do not
  scatter state through the source tree.
- Make ordering requirements explicit, especially dependencies, subdirectories,
  packaging, resources, and runnable commands.
- Examples document one build-system concept at a time. Keep each example minimal
  and include the shared Makefile last.
- User-facing documentation should describe the current interface in the same
  vocabulary as the code. Prefer one accurate example over several nearly identical
  examples.

## Making changes

Before editing, read the whole of `Makefile.inc`, the relevant README section, and
the examples that use the affected variables. Then make the narrowest coherent
change.

For every change, ask:

- Is the consuming Makefile smaller or at least no harder to understand?
- Does the name reveal the concept without requiring a comment?
- Is the behavior implemented once at the right level?
- Could a Make built-in replace a new command or dependency cleanly?
- Are command-line overrides, target ordering, and platform branches preserved?
- Do comments explain reasons rather than restate code?
- Do the README and examples still agree with the implementation?

At minimum, parse or dry-run every affected path with GNU Make. Test behavior changes
with the smallest relevant checked-in example and inspect the expanded commands,
not just Make's exit status. Run `make -C tests host` for build or platform changes
and `make -C tests docker` when Docker is available. Run `git diff --check` before
finishing.

## Worktrees and branches

Use one worktree per PR or unit of work, checked out under `.worktrees/` inside the
repository, and reuse the existing worktree for follow-up work on the same PR
instead of creating a duplicate. Base new branches on `origin/master`. Never run
`git add -A` or `git add .` from the repository root; stage explicit paths instead.

State which worktree and branch the work is happening in when starting a task.

## GitHub workflow

Prefer the `gh` CLI for all GitHub operations. Do not include AI attribution or
emoji in GitHub issues, PR titles, or PR bodies.

Before filing a new issue, search for existing related or duplicate issues and
reference them instead of opening a redundant one. Keep issues concise and
high-level. For bugs, include clear steps to reproduce; when the fix is clear,
include a proposed high-level solution, plus alternatives when they are useful, but
not exact code changes. Put long supplementary details in `<details>` blocks. File
design and interface decisions as issues too, and keep them updated as findings
land during implementation.

Keep GitHub identifiers out of the tree: do not reference issue or PR numbers,
issue/PR URLs, or branch names in `Makefile.inc`, comments, the README, examples,
or tests unless the identifier is essential to a long-lived compatibility
constraint. Summarize the technical reason in place and keep issue/PR links in PR
bodies, commit messages, or review notes instead.

PRs created by agents start as drafts (`gh pr create --draft`) unless the user
explicitly asks for a ready-for-review PR. Do not mark a draft ready, merge it, or
enable auto-merge without an explicit request. Titles are imperative, under 60
characters, with no trailing period and no `This PR...` prefix. Bodies do not use
markdown section headers: start with `This PR`, keep the body to 1-3 sentences, use
bullets rather than paragraphs for multi-item changes, stay high-level, and briefly
note which validation ran (for example `make -C tests host`). Do not use closing
keywords (`Fixes #N`, `Closes #N`) unless auto-closing that issue on merge is
intended, and keep issue and PR discussion comments terse: 2-3 sentences stating
the decision and one reason.

LazyMake is consumed by vendoring: a consumer copies `Makefile.inc` into its own
tree, so a merged change is not live until that project updates its copy. When a
change is meant to be picked up, say so in the PR body and coordinate the
consumer-side update as a separate PR in that repository, noting the intended
merge order in each body.
