<div align="center">

# gbui

**A retained-mode UI toolkit in C++20** — flexbox layout, themeable components,
and a painter interface that a backend implements.

Written from scratch: the only dependency is the standard library.

[Guide](docs/guide/introduction.md) ·
[Reference](docs/reference/overview.md) ·
[Writing a component](docs/guide/writing-a-component.md) ·
LGPL-3.0-or-later

</div>

---

It exists to give [GitBox](https://github.com/gitgusilva/gitbox) a path off
Electron that keeps its design system. The theme layer reads the **same
`theme.json`** the Vue app and the
[gitbox-themes](https://github.com/gitgusilva/gitbox-themes) registry already
publish, so a theme written for the Electron app themes this one unchanged.

```
┌── scene ─────┐   ┌── layout ───┐   ┌── paint ─────┐   ┌── backend ──┐
│ Arena + Ui   │ → │ flexbox     │ → │ DisplayList  │ → │ Canvas      │
│ (build)      │   │ (frames)    │   │ (commands)   │   │ SVG …yours  │
└──────────────┘   └─────────────┘   └──────────────┘   └─────────────┘
```

Each stage only reads the one before it. Layout knows nothing about painting,
painting knows nothing about nodes, and the backend knows nothing about themes —
which is why the whole thing is testable without a window, a GPU or a font.

## What you get

- **CSS flexbox**, including wrapping, percentages, `min-width: auto` and the
  spec's flexible-length loop — so a window dragged from 680 px to 3840 px does
  not break.
- **Themes as data**: 24 semantic tokens from a JSON file, plus a `Design`
  beside them carrying shape, sizing and motion. Material, Cupertino and Fluent
  ship built in.
- **Around fifty components** — button, badge, list row, box, scroll view,
  virtualised list, table, tabs, every form control, date, time and colour
  pickers, a rich-text editor, tooltips, popovers, menus, selects, modals, and
  eight kinds of chart.
- **A whole interaction layer**: hover, press, click, focus, `:focus-visible`,
  focus-within, Tab traversal, wheel routing and per-node cursors.
- **Logical pixels**, so a HiDPI display is one multiply at the edge of the
  pipeline rather than something every component has to remember.
- **Two backends in the box**: a software rasteriser with antialiasing and
  gradients, and an SVG writer for review and golden images.

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure       # 203 cases, well under a second

./build/examples/gbui_controls                   # every component, interactive
./build/examples/gbui_panel out/                 # a panel, wide / narrow / light

# The whole app screen, once per theme, with a switcher:
./build/examples/gbui_gallery gallery/ ../gitbox-themes/themes
xdg-open gallery/index.html                      # left/right themes, up/down widths
```

`gbui_controls` is the fastest way to see the library: tabbed pages of every
component, with the theme and the design system switchable while it runs.
`gbui_gallery` builds one application screen — title bar, toolbar, sidebar,
changes list, diff, commit box, history with graph lanes, status bar — and
renders it in every theme it finds, at 1600, 1180, 900 and 680 px. Nothing in
that screen is theme-aware or width-aware: only the `Theme` handed to `layout`
and `record` and the viewport rectangle change between images, which is exactly
the property worth checking by eye.

Requires a C++20 compiler and CMake 3.20. SDL2 is optional — without it the
library still builds and every test passes; only `Window::create` changes.
Consumed from another project:

```cmake
add_subdirectory(gbui)
target_link_libraries(your_app PRIVATE gbui::gbui)
```

## A first window

```cpp
#include "gbui/widgets/components.hpp"
#include "gbui/layout/layout.hpp"
#include "gbui/paint/paint.hpp"
#include "gbui/scene/ui.hpp"

using namespace gbui;

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

The loop version — input, animation, a real font and a window — is in
[Guide → Your first window](docs/guide/first-window.md).

## Documentation

The site in `docs/` is built with VitePress and holds the guide and the API
reference:

```sh
npm install
npm run docs:dev        # http://localhost:5173
npm run docs:build
```

Start at **Guide → Introduction**. The tutorial for adding a component of your
own is **Guide → Writing a component**.

## Memory

Nodes live in one growing vector inside the `Arena` and are addressed by index,
not by pointer. Children are an intrusive first-child/next-sibling list, so a
container with three children allocates nothing of its own. Text is interned
into fixed 4 KiB blocks that are never reallocated, and handed out as
`string_view`; vector art is a range into a shape store the arena owns.

The consequences are the reason for the design: building a tree is a
`push_back` rather than a `new`; layout and paint walk memory in the order it
was written; releasing a whole tree is `arena.reset()`, one operation with no
destructor cascade and no cycle to leak; and a `NodeId` survives a reallocation
where a `Node*` would not.

## What is here

Modules, in dependency order. `include/gbui/<module>/` and `src/<module>/`
mirror each other, and nothing below `platform` knows what machine it is on.

| Module | Holds |
| --- | --- |
| `core/` | `geometry` (`Vec2`, `Rect`, `Edges`, `Length`, `kAuto` as NaN), `color` (parsing, HSV, WCAG contrast), `cursor`, `json` (a reader sized for theme files), `path` (the SVG path grammar, flattened) |
| `anim/` | `easing` (the CSS curves, solved, plus the classic families), `animator` (transitions, pulses, latches, a clock) |
| `style/` | `style` (flexbox properties, `Fill`, `Gradient`, `TextStyle`, `Border`, `Outline`), `theme` (the registry's 24 tokens and its typography), `design` (shape, sizing, press feedback, motion, chart defaults) |
| `scene/` | `tree` (`Arena`, `Node`, `NodeId`, `Shape`), `ui` (the scope-guard building API) |
| `layout/` | flexbox, min-content sizing, the flexible-length loop, wrapping, hit testing, `textWrap` |
| `input/` | `keys`, `interaction` (hover, press, focus, focus-visible, wheel routing, cursors), `textEdit` |
| `overlay/` | `placement`: flip, shift and stay inside the window |
| `paint/` | `paint` (`DisplayList`, `Painter`, `SvgPainter`, the display scale), `canvas` (a software rasteriser and the painter that drives it) |
| `widgets/` | one header and one source file per component, with four umbrella headers over the groups: `components`, `containers`, `controls` and `overlays`. `icons` is generated; `src/widgets/detail.hpp` is what siblings share |
| `platform/` | `window` (a window and an event loop; SDL2 today), `font` (family resolution, rasterisation, real metrics), `shell` (`openUrl`) |

`third_party/` holds `stb_truetype.h` and may only be included from
`platform/`. `tools/generate_icons.py` regenerates the icon table.

## What is not here yet

Named rather than half-built:

- **No accessibility tree.** Hover, press, click, focus, `:focus-visible`,
  focus-within, Tab traversal and typed text all work; a role and an accessible
  name per node, and the bridge to AT-SPI, UIA and NSAccessibility that a screen
  reader needs, do not exist. Nor does a reduced-motion flag.
- **No text shaping.** Glyphs are rasterised and kerned, which covers Latin,
  but Arabic, Devanagari and emoji need HarfBuzz. There is no glyph atlas
  either: glyphs are cached per face and blended one at a time.
- **No images.** Vector paths draw; PNG and JPEG do not decode yet.
- **One backend.** SDL2 opens the window; there is no native Win32, Cocoa or
  Wayland path, and nothing on the GPU.
- **Flexbox is a subset**: no aspect-ratio, `Align::Baseline` behaves as
  `Align::Start`, and out-of-flow boxes are placed by their top-left corner —
  there is no `right`/`bottom`. Percentages apply to sizes, not to padding,
  margin or gap.
- **No container queries**, so a component adapts to its own width only by
  reading last frame's geometry.
- **`Border` is all four edges.** A toolbar that wants only a bottom rule adds a
  `divider` after itself.
- **Virtualisation assumes a uniform row height.** `virtualList` carries 50 000
  commits in a few dozen nodes, but it picks its slice by dividing the offset by
  the row height. Rows of varying height need a measured index, which a diff
  view will eventually want.
- **No code or diff view.** Syntax highlighting, folding and a minimap are a
  project of their own.

## Licence

LGPL-3.0-or-later, matching GitBox — see [LICENSE](LICENSE) and
[GPL-3.0.txt](GPL-3.0.txt). Icons are [Lucide](https://lucide.dev), ISC
licensed; `third_party/stb_truetype.h` is public domain.
