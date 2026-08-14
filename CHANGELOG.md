# Changelog

Every released version, newest first, written from the history rather than from
memory. Dates are the day the version was tagged.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
the versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html),
with the caveat every 0.x project carries: **the API is not stable yet**, and a
minor version may change one. What is promised before 1.0 is that a change is
named here and the reason for it is in the commit.

## [Unreleased]

Work towards 0.3 lives on `main` and gets a number when it ships, not commit by
commit.

### Changed

- **Every `begin*` container is named after what it makes.** `beginPanel` is
  `panel`, `beginBox` is `box`, and so on through `toolbar`, `listRow`,
  `popover`, `modal` and `modalActions`; `beginScroll` is `scrollArea`, because
  `scroll` is what half the call sites already call their `ScrollState`. On the
  builder, `ui.begin` is `ui.scope`, `ui.beginRow`/`beginColumn` are `ui.row`
  and `ui.column`, and `ui.beginIds` is `ui.ids`. A container and a leaf now
  share one naming rule and differ only in what they return — `Ui::Scope`
  against `NodeId` — which is the distinction that was worth spelling, and
  `begin` was never it. **This renames public API**; the mapping above is the
  whole of it, and a compiler finds every call site.
- Documentation: **every component has its own page**, listed in the sidebar and
  found by search, generated from the same metadata the gallery read. Components
  declared in one header share one page — `colorField` with `colorPicker`,
  `scrollArea` with `scrollbar`, `text` with `strong` and `emphasis` — because
  the file they are in is the toolkit's own statement that they are one idea.
  `/components` is the contents page for them.

## [0.2.1] — 2026-08-13

A packaging fix, released from `0.2.x` rather than from `main`: the branch is
0.2 and this, and nothing else.

### Fixed

- **A shared build could not run its own executables on Windows.** Windows has
  no rpath — a program finds a DLL beside itself or on `PATH` — while CMake's
  multi-config generator puts the library in `build/Release/` and every
  executable in `build/<dir>/Release/`. The build and the link both succeed and
  the *loader* fails at startup, which on a machine with no desktop is a dialog
  nobody can dismiss: the process never returns and never says why. The CI job
  that builds shared and runs the tests sat for six hours before it was
  cancelled. The DLLs are copied beside each executable now, on Windows and only
  when the build is shared.
- CI grew the ceilings whose absence turned that bug into six hours:
  `ctest --timeout 180`, so a hung test fails with its own name, and
  `timeout-minutes` on the job that found it.

## [0.2] — 2026-08-12

The first version with a published site, a CI pipeline, an installable library
and a set of application screens to look at. It is the version the documentation
described from the start; it had never been tagged, which is what this tag fixes
— an archived version with no tag is a dropdown entry that 404s.

### The toolkit

- **Build → layout → paint**, three stages in one direction: layout is
  arithmetic that can be asserted without a window, and painting is a display
  list that can be inspected without a GPU.
- **CSS flexbox** with wrapping, percentages, min-content sizing and out-of-flow
  positioning, in logical pixels — a 200% display is a property of the output
  rather than something a component knows about.
- **Theming as data**: 24 semantic tokens read from the `gitbox-themes`
  registry's JSON, with Material 3, Cupertino and Fluent palettes built in, and
  a `Design` beside them for shape, sizing and motion.
- **A software rasteriser** with antialiasing, gradients and clipping, and an
  **SVG writer** for review and golden images.
- **The interaction layer** in full: hover, press, click, focus,
  `:focus-visible`, focus-within, Tab traversal, wheel routing and per-node
  cursors.
- **An animation clock** on CSS's `transition` model — a component says where a
  value should be, not how to get there.
- **Around fifty components**, from a button to a table, a rich-text editor and
  eight kinds of chart, all stateless functions themed by token.
- **One allocation per frame**: nodes live in an arena addressed by index, so
  building a tree is a `push_back` and releasing one is a reset.

### The component set as data

- `gbui::meta` describes every component — its group, its documentation, its
  signature, and each option with type, default and doc — **generated from the
  headers** by `tools/generate_meta.py`, so a table nobody maintains cannot fall
  behind the code it describes. CI regenerates it and fails on a difference.

### Demos and documentation

- **Six application screens** built from the public headers alone: a revenue
  dashboard, a weather desk, a plant supervisory HMI, a production line monitor,
  a grid control desk and a logistics control tower. They link `gbui::gbui` and
  nothing else, so a change that makes them awkward has made the library
  awkward.
- The demos **run in the browser** through WebAssembly, rasterised on the CPU
  into a `<canvas>` — the same source, with no DOM inside the rectangle.
- A documentation site on VitePress, **published per version**: the current
  release at the root and every archived one at its own address, built from its
  own tag.
- The source is shown first and the screen runs when the reader asks, so nothing
  downloads until they press Run.

### Build and packaging

- Installable with CMake, as a **static or shared library**, and consumed from
  outside the tree in CI to prove the install actually works.
- SDL2 is optional: without it everything still builds and every test still
  passes, and only `Window::create` changes.
- CI builds, tests, sanitises and lints on Linux, macOS and Windows, with
  warnings as errors.

### Fixed

- Every `std::optional` in an options struct has a default member initialiser,
  so a designated-initialiser call site cannot leave one indeterminate.
- Four things that only writing six screens against the library could find — see
  `acf2897`.
- The runner works on a fresh machine and on MSVC; the gallery example the
  gitignore was hiding is committed.

### Known at the time

- No accessibility tree, and no bridge for a screen reader.
- No text shaping, so Arabic, Devanagari and emoji are wrong.
- No GPU painter — the `Painter` interface is six methods precisely so one can
  be written.
- No image decoding.

[Unreleased]: https://github.com/gitgusilva/gbui/compare/v0.2.1...HEAD
[0.2.1]: https://github.com/gitgusilva/gbui/releases/tag/v0.2.1
[0.2]: https://github.com/gitgusilva/gbui/releases/tag/v0.2
