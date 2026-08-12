# Contributing to gbui

Thank you for looking. This is a small library with strong opinions, and most
of them are written down — so the fastest way to get a change merged is to know
which opinion your change is arguing with.

Start with [Guide → Architecture](docs/guide/architecture.md). It is four pages
and it explains nearly every review comment you might otherwise receive.

## Getting set up

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure

./build/examples/gbui_controls      # every component, interactive
./build/examples/gbui_panel out/    # SVGs, no display needed
```

A C++20 compiler and CMake 3.20. SDL2 is optional: without it the library still
builds, every test still passes, and only `Window::create` changes. Nothing
above `platform/` may gain a dependency — that is the property the whole design
rests on, and a pull request that adds one to `core`, `layout` or `widgets` will
be asked to remove it whatever else it does.

## The rules that decide reviews

1. **Three stages, one direction.** build → layout → paint. No stage reads the
   one after it. If a change needs layout to call back into the builder, that is
   a design conversation before it is a patch.
2. **Colour comes from a `Token`, shape and sizing from the `Design`.** A
   component that hardcodes either is a bug — it is how a light theme or a
   design switch breaks in the one place nobody looked.
3. **Components are stateless functions.** They take a `Ui&`, an options struct
   and — when they react — an `Interaction` and an id. They report what the user
   did and never write to the caller's model.
4. **Identity is a string tag, never a `NodeId`.** The tree is rebuilt every
   frame; only a tag survives it.
5. **One component, one header, one source file**, all named after the symbol in
   the same camelCase (`numberField.hpp` declares `numberField()`). Anything two
   components share goes in `src/widgets/detail.hpp` rather than being copied.
6. **Name what is missing.** A gap admitted in a header comment is a feature. A
   half-built one that silently misbehaves is a liability, and this codebase
   would rather ship the honest sentence.

[Guide → Writing a component](docs/guide/writing-a-component.md) walks through
a real one, with the checklist to run before calling it done.

## Tests

Layout is arithmetic and painting is a list of commands, so almost everything
here is testable with no window, no GPU and no font installed. The suite is 203
cases and runs in well under a second; there is no reason for a change to arrive
without one.

The harness is sixty lines in `tests/harness.hpp` — `TEST`, `CHECK`, `CHECK_EQ`,
`CHECK_NEAR`. Add a `.cpp`, list it in `tests/CMakeLists.txt`, and it runs.

Assert the arithmetic rather than the appearance: "the row adds up to its
container", "a primary button paints the accent", "the arena stays in the
dozens". [Guide → Testing](docs/guide/testing.md) has the patterns, including
how to drive a control through an `Interaction` without a pointer.

For anything visual, render it and look:

```sh
./build/examples/gbui_controls --shot out.ppm --page 3 --pointer 420 260
./build/examples/gbui_gallery out/ ../gitbox-themes/themes
```

## Documentation

The site in `docs/` is the API documentation, not a companion to it. A change
that adds, renames or retires anything public updates the matching page in
`docs/reference/` in the same commit — and if it changes how something is *used*,
the guide page too.

```sh
npm install
npm run docs:dev        # http://localhost:5173
npm run docs:build      # also a dead-link check: VitePress fails the build on one
tools/build_docs.sh     # the whole site, every version, as CI publishes it
```

The site at [gitgusilva.github.io/gbui](https://gitgusilva.github.io/gbui/) is
published on every push that touches `docs/`.

### Releasing a version of the documentation

Every release keeps its own copy, so a link to `/v0.2/guide/layout` still says
what 0.2 said after 0.3 has shipped. The current version lives at the root and
the older ones in a directory each, which is the convention the Vite and Vue
sites use — VitePress has no versioning of its own.

Cutting one is three edits in `docs/.vitepress/versions.ts` and a tag:

1. move the outgoing version into `archived`;
2. set `current` to the new one, in step with `CMakeLists.txt`;
3. `git tag v0.3 && git push --tags`.

The archived copy is built **from its tag**, in a worktree, so it is what that
release actually said rather than today's text wearing an old label — and it is
also why a version leaves `archived` only when its directory is deleted, never
before. A dropdown entry that 404s is worse than a page saying it is out of
date.

## Style

`.clang-format` and `.clang-tidy` at the root are the mechanical half. The rest:

| Thing | Style | Example |
| --- | --- | --- |
| Types | `CamelCase` | `DisplayList`, `NodeId` |
| Functions and methods | `camelBack` | `beginListRow`, `intrinsicMainSize` |
| Private members | trailing underscore | `contours_` |
| Constants | `k` prefix | `kAuto`, `kMenuItemHeight` |
| Files | named for the symbol, same case | `numberField.hpp` |
| Options | an `…Options` struct | `ButtonOptions` |

**Comments explain why.** What the code does is already written down in the
code, and a comment that restates it goes stale silently. Anything surprising —
a workaround, a spec quirk, a bug that shipped once — is worth a sentence, and
the bug that shipped once is worth naming so it does not come back.

The library builds with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion
-Wsign-conversion -Werror`, or `/W4 /WX` under MSVC — `GBUI_WERROR`, on by
default for a standalone build and on in every CI job. `-Wconversion` is the one
that earns its keep here: a float quietly becoming an int in the middle of a
layout calculation is this codebase's most likely bug.

A warning that is wrong gets the narrowest suppression that silences it and a
comment saying why — per file in `CMakeLists.txt`, not per build.

### clang-format runs on the lines you touched

```sh
pipx install clang-format==18.1.8      # the version CI uses
clang-format -i path/to/file.cpp       # or just the lines you changed
```

CI checks **the diff, not the tree**. The existing sources are not fully
clang-format-clean, and reformatting eighteen thousand lines to make a badge go
green would bury every future `git blame` under one commit — so code you write or
change has to be formatted, and code you did not touch is left alone. The
backlog shrinks as files are worked on.

### clang-tidy is a report, not a gate

CI runs it on the files a change touches and prints what it finds into the job
summary; it does not fail the build. That is deliberate, and the reason is the
same one: there is a backlog of about a hundred findings under the current check
list, and a gate that is red before anyone has touched anything teaches everyone
to ignore it.

Read the report anyway, and do not add to it. The day the backlog is empty,
`WarningsAsErrors` in `.clang-tidy` goes on and this paragraph goes away.

The checks that are switched off are switched off with a reason written next to
each one in `.clang-tidy`; they argue with a decision this project made on
purpose. If you think one of them is right, say so in an issue — that is a
conversation worth having and a one-line change either way.

## Commits and pull requests

- **Write the message in English**, whatever language the discussion is in.
- **A conventional prefix**: `feat:`, `fix:`, `docs:`, `style:`, `refactor:`,
  `test:`, `chore:`, with a scope where it helps — `fix(layout): …`.
- **The subject says what changed; the body says why.** A diff already shows the
  what.
- **No `Co-Authored-By` trailers for tooling.** A commit is attributed to the
  person who made the decisions in it.
- **Keep a pull request to one idea.** A formatting sweep and a bug fix in one
  branch is two reviews wearing one hat.

CI runs on every push and pull request: build and test on Linux (GCC and Clang,
with SDL2), macOS and Windows, all four with warnings as errors; the test suite
again under ASan and UBSan; the format check above; the clang-tidy report; and
the documentation build. Everything except the clang-tidy report must be green.

## Reporting something broken

The useful bug report for a UI toolkit is a picture and a repro:

```sh
./build/examples/gbui_controls --shot bug.ppm --page 2 --size 900 780
```

Say which compiler and platform, whether SDL2 was found, and what you expected
instead. A layout bug is usually visible in the tree, and printing it is three
lines — see [Guide → Layout](docs/guide/layout.md#debugging-a-layout).

## Licence

gbui is LGPL-3.0-or-later. By contributing you agree that your contribution is
licensed under the same terms, so that linking the library into a closed
application stays possible while changes to the library itself come back.
