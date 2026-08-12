---
layout: home

hero:
  name: gbui
  text: A UI toolkit for C++
  tagline: Flexbox layout, themeable components and a painter you can implement — with no web engine and no dependency beyond the standard library.
  actions:
    - theme: brand
      text: Get started
      link: /guide/introduction
    - theme: alt
      text: Reference
      link: /reference/overview
    - theme: alt
      text: View on GitHub
      link: https://github.com/gitgusilva/gbui

features:
  - title: Layout you already know
    details: Rows, columns, grow and shrink, gaps, padding, wrapping, percentages, min-content sizing — the CSS flexbox model, because that is what desktop UIs are written in today.
  - title: Themes as data
    details: 24 semantic tokens loaded from a JSON file, and a Design beside them for shape, sizing and motion. A component names tokens, never colours, so a whole application re-themes without being rebuilt.
  - title: Testable without a screen
    details: Layout is arithmetic and painting is a display list, so both are asserted in CI with no window, no GPU and no font installed.
  - title: One allocation per frame
    details: Nodes live in an arena addressed by index. Building a tree is a push_back, releasing one is a reset, and the same pages are reused every frame.
  - title: Bring your own painter
    details: Six methods make a backend. A software rasteriser and an SVG writer ship with the library; a GPU one is the same six methods.
  - title: Vector everything
    details: An SVG path parser, 40 Lucide glyphs and a canvas node, so icons, commit graphs and charts are geometry rather than bitmaps — sharp at any scale, in the theme's colours.
---

## Have a look at it

<GbuiDemo id="analytics" :height="560" />

A real application screen and the C++ that draws it. Press Run and it is the
same source compiled to WebAssembly, rasterised on the CPU — there is no DOM
inside that rectangle. [Five more, and how it is put together](/demos).

## In thirty seconds

```cpp
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/components.hpp"

Arena arena;                          // owns every node
Ui ui(arena);
{
    auto panel = beginPanel(ui, {.direction = Direction::Column});
    sectionHeading(ui, "UNSTAGED (1)");
    {
        auto row = beginListRow(ui, {.selected = true});
        icon(ui, Icon::FilePlus, {.color = Token::Modified});
        text(ui, "themes/nord/theme.json", {.grow = 1.0f});
        badge(ui, "M");
    }
    button(ui, "COMMIT", {.variant = ButtonVariant::Primary, .block = true});
}
```

That tree is then laid out into a rectangle, recorded into a display list and
handed to a painter. Those three steps are the whole of the library, and
[Architecture](/guide/architecture) explains why they are separate.

## Status

Version 0.2, and honest about what it is: young, small enough to read, and
already carrying a real application screen.

**What works today.** Flexbox with min-content sizing, wrapping, percentages and
out-of-flow positioning; logical pixels, so a 200% display is a property of the
output rather than something a component knows about; theming from the GitBox
theme registry, with Material, Cupertino and Fluent palettes built in; a
software rasteriser with antialiasing, gradients and clipping; real text through
stb_truetype, wrapped or elided by the same function layout used; vector icons;
an SVG backend for review; a window and an event loop on SDL2; the whole
interaction layer — hover, press, click, focus, `:focus-visible`, focus-within,
Tab traversal, wheel routing and per-node cursors; an animation clock; and
around fifty components, from a button to a table, a rich-text editor and eight
kinds of chart.

**What does not exist yet.** An accessibility tree and the bridge a screen
reader needs; text shaping, so Arabic, Devanagari and emoji are wrong; images,
since nothing decodes a PNG; a GPU painter; and a code editor with syntax
highlighting. Each of those is named in the header of the module that would own
it rather than half-built.
