<div align="center">

# gbui

**A UI toolkit for C++20.** Flexbox layout, themeable components, and a painter
interface a backend implements — with no web engine and no dependency beyond the
standard library.

[![CI](https://github.com/gitgusilva/gbui/actions/workflows/ci.yml/badge.svg)](https://github.com/gitgusilva/gbui/actions/workflows/ci.yml)

[Guide](docs/guide/introduction.md) ·
[Reference](docs/reference/overview.md) ·
[Contributing](CONTRIBUTING.md) ·
LGPL-3.0-or-later

</div>

---

```
┌── scene ─────┐   ┌── layout ───┐   ┌── paint ─────┐   ┌── backend ──┐
│ Arena + Ui   │ → │ flexbox     │ → │ DisplayList  │ → │ Canvas      │
│ (build)      │   │ (frames)    │   │ (commands)   │   │ SVG …yours  │
└──────────────┘   └─────────────┘   └──────────────┘   └─────────────┘
```

Each stage reads only the one before it, which is why layout is arithmetic you
can assert without a window and painting is a list of commands you can inspect
without a GPU.

- **CSS flexbox** — wrapping, percentages, `min-width: auto`, out-of-flow
  positioning, logical pixels.
- **Themes as data** — 24 semantic tokens from a JSON file, plus a `Design` for
  shape, sizing and motion. Material, Cupertino and Fluent built in.
- **~50 components** — from a button to a table, a rich-text editor and eight
  kinds of chart, all stateless functions.
- **The whole interaction layer** — hover, press, focus, `:focus-visible`,
  Tab traversal, wheel routing, cursors, an animation clock.
- **Two backends in the box** — a software rasteriser and an SVG writer. A third
  is six methods.

It was written for [GitBox](https://github.com/gitgusilva/gitbox) as a path off
Electron, and reads the **same `theme.json`** the
[gitbox-themes](https://github.com/gitgusilva/gitbox-themes) registry publishes.

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure

./build/examples/gbui_controls     # every component, interactive
./build/examples/gbui_panel out/   # SVGs, no display needed
```

C++20 and CMake 3.20. SDL2 is optional: without it everything still builds and
every test still passes, and only `Window::create` changes.

## Use it

```cmake
add_subdirectory(gbui)
target_link_libraries(your_app PRIVATE gbui::gbui)
```

```cpp
Arena arena;                        // owns every node
Ui ui(arena);
{
    auto root = ui.beginColumn({.gap = 8.0f, .padding = Edges::all(12.0f),
                                .background = Fill{Token::Bg}});
    text(ui, "Local Changes", {.color = Token::TextStrong});
    button(ui, "COMMIT", {.variant = ButtonVariant::Primary, .block = true});
}                                   // the column closes here

const Theme theme = Theme::dark();
LayoutContext context{.theme = &theme};
layout(arena, ui.root(), Rect{0, 0, 320, 200}, context);

DisplayList list;
record(arena, ui.root(), theme, list);

SvgPainter painter(320, 200, theme.color(Token::Bg));
painter.paint(list);
```

## Documentation

The site in `docs/` is built with VitePress — `npm install && npm run docs:dev`.

| | |
| --- | --- |
| **Start here** | [Introduction](docs/guide/introduction.md) · [Installation](docs/guide/installation.md) · [Your first window](docs/guide/first-window.md) |
| **Concepts** | [Architecture](docs/guide/architecture.md) · [Building a tree](docs/guide/building-a-tree.md) · [Layout](docs/guide/layout.md) · [Theming](docs/guide/theming.md) · [Input and focus](docs/guide/input.md) · [Motion](docs/guide/motion.md) · [Memory](docs/guide/memory.md) |
| **Going further** | [Writing a component](docs/guide/writing-a-component.md) · [Icons](docs/guide/icons.md) · [Writing a backend](docs/guide/writing-a-backend.md) · [Testing](docs/guide/testing.md) |
| **API** | [Overview](docs/reference/overview.md) · [core](docs/reference/core.md) · [style](docs/reference/style.md) · [scene](docs/reference/scene.md) · [layout](docs/reference/layout.md) · [input](docs/reference/input.md) · [anim](docs/reference/anim.md) · [overlay](docs/reference/overlay.md) · [paint](docs/reference/paint.md) · [widgets](docs/reference/widgets.md) · [charts](docs/reference/charts.md) · [platform](docs/reference/platform.md) |

## Status

Version 0.2, and honest about it. What is **not** here: an accessibility tree
and the bridge a screen reader needs, text shaping (so Arabic, Devanagari and
emoji are wrong), images, a GPU painter, and a code editor with syntax
highlighting. Each gap is named in the header of the module that would own it
rather than half-built; the shorter list of engine limits is in
[Layout → What is not implemented](docs/guide/layout.md#what-is-not-implemented).

## Contributing

[CONTRIBUTING.md](CONTRIBUTING.md) — the rules that decide reviews, how to write
a test without a window, and what CI runs: build and test on Linux, macOS and
Windows with warnings as errors, the suite again under ASan and UBSan, a
clang-format check on the lines a change touches, a clang-tidy report, and the
documentation build.

## Licence

LGPL-3.0-or-later — see [LICENSE](LICENSE) and [GPL-3.0.txt](GPL-3.0.txt). Icons
are [Lucide](https://lucide.dev) (ISC); `third_party/stb_truetype.h` is public
domain.
