<div align="center">

# gbui

**A UI toolkit for C++20.** Flexbox layout, themeable components, and a painter
interface a backend implements — with no web engine and nothing to fetch: the
toolkit is the standard library and nothing else, and the two public-domain
headers it vendors sit in the platform layer, where the machine already is.

[![CI](https://github.com/gitgusilva/gbui/actions/workflows/ci.yml/badge.svg)](https://github.com/gitgusilva/gbui/actions/workflows/ci.yml)

### [gitgusilva.github.io/gbui](https://gitgusilva.github.io/gbui/)

[Live demos](https://gitgusilva.github.io/gbui/demos) ·
[Guide](https://gitgusilva.github.io/gbui/guide/introduction) ·
[Your first window](https://gitgusilva.github.io/gbui/guide/first-window) ·
[Reference](https://gitgusilva.github.io/gbui/reference/overview) ·
[Contributing](CONTRIBUTING.md)

</div>

---

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/public/pipeline-dark.svg">
    <img alt="The pipeline: scene builds a tree, layout writes frames, paint records a display list, and a backend draws it." src="docs/public/pipeline-light.svg" width="880">
  </picture>
</p>

Each stage reads only the one before it, which is why layout is arithmetic you
can assert without a window and painting is a list of commands you can inspect
without a GPU. Over sixty components sit on top — from a button to a table, a
tree view, a rich-text editor and nine kinds of chart — all stateless functions,
themed by token rather than by colour.

**Every one of them says what it is.** A role, a name, whatever state and value
apply, and the relations that attach a caption to its field — read into a pruned
accessibility tree that is diffed each frame, so a screen reader is told what
changed rather than everything. It is a rule rather than a milestone: two checks
in CI fail the build for a control with nothing to announce, one over the library
and one over the applications built on it.

It was written for [GitBox](https://github.com/gitgusilva/gitbox) as a path off
Electron, and reads the **same `theme.json`** the
[gitbox-themes](https://github.com/gitgusilva/gitbox-themes) registry publishes.

Seven full application screens — a revenue dashboard, a weather desk, a plant
supervisory HMI, a production line monitor, a grid control desk, a logistics
control tower and a trading desk — run in the browser on the
[demos page](https://gitgusilva.github.io/gbui/demos), compiled to WebAssembly
and rasterised on the CPU. Their source is in [`demos/`](demos), and it is
ordinary consumer code: public headers, one link line, no private access.

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure

./build/demos/gbui_demo --list                 # seven application screens
./build/demos/gbui_demo --component treeView   # one component's live example
```

C++20 and CMake 3.20. SDL2 is optional — without it everything still builds and
every test still passes, and only `Window::create` changes.

```cmake
add_subdirectory(gbui)
target_link_libraries(your_app PRIVATE gbui::gbui)
```

Or don't build it: every release carries a shared library, the headers and the
CMake package files for Linux, Windows and both kinds of Mac —
[download](https://gitgusilva.github.io/gbui/download). Those have no SDL2 and
so no window, because a prebuilt library that links SDL2 needs the same SDL2 on
the machine that loads it; everything else in them is the same code.

Then read [Your first window](https://gitgusilva.github.io/gbui/guide/first-window).

## Licence

LGPL-3.0-or-later — see [LICENSE](LICENSE) and [GPL-3.0.txt](GPL-3.0.txt). Icons
are [Lucide](https://lucide.dev) (ISC); `third_party/stb_truetype.h` is public
domain.
