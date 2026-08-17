---
layout: home

hero:
  name: gbui
  text: A UI toolkit for C++
  tagline: Flexbox layout, 63 themeable components and a painter you can implement — no web engine, no runtime, nothing to fetch.
  actions:
    - theme: brand
      text: Get started
      link: /guide/introduction
    - theme: alt
      text: Components
      link: /components
---

## This is it, running

<GbuiShowcase :height="560" />

## Everything in it

<GbuiBand />

## Why it is built this way

<div class="gbui-why">

**Layout you already know.** Rows, columns, grow and shrink, gaps, padding,
wrapping, percentages, min-content sizing — the CSS flexbox model, because that
is the model desktop UIs are written in today.

**Themes as data.** 24 semantic tokens from a JSON file, and a `Design` beside
them for shape, sizing and motion. A component names tokens, never colours, so a
whole application re-themes without being rebuilt — which is what the buttons
above the screen are doing.

**Testable without a screen.** Layout is arithmetic and painting is a display
list, so both are asserted in CI with no window, no GPU and no font installed.
391 tests, and every demo screen rendered headless on every push.

**One allocation per frame.** Nodes live in an arena addressed by index.
Building a tree is a `push_back`, releasing one is a `reset`, and the same pages
are reused every frame.

**Bring your own painter.** Six methods make a backend. A software rasteriser
and an SVG writer ship with the library; a GPU one is the same six methods.

**Vector everything.** An SVG path parser, 40 Lucide glyphs and a canvas node,
so icons, commit graphs and charts are geometry rather than bitmaps — sharp at
any scale, in the theme's colours.

**Every control says what it is.** A role, a name, a state, and the relations
that tie a caption to its field, on every component — read into a pruned
accessibility tree that is diffed each frame, so a screen reader is told what
changed rather than everything. Two gates in CI keep it true of the library and
of the applications built on it.

</div>

## Twelve lines

```cpp
#include "gbui/widgets/elements.hpp"

auto column = ui.column({.gap = 8.0f, .padding = Edges::all(12.0f)});
text(ui, "Local Changes", {.color = Token::TextStrong});

if (checkbox(ui, input, "settings.tags", showTags, {.label = "Show tags"}))
    showTags = !showTags;

if (button(ui, input, "COMMIT", {.variant = ButtonVariant::Primary}))
    commit();
```

No base class, no inheritance, no signals and no code generator. A component is
a function that takes a `Ui&` and an options struct, and holds no state — the
value is yours, which is what makes undo, validation and "are you sure?"
possible without the toolkit knowing about any of them.

[Your first window](/guide/first-window) ·
[How it is put together](/guide/architecture) ·
[Seven full screens](/demos) ·
[Download a build](/download)

<style>
.gbui-why {
  display: grid;
  gap: 14px 26px;
  grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
  margin: 8px 0 8px;
}
.gbui-why p {
  margin: 0;
  padding: 14px 16px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 10px;
  background: var(--vp-c-bg-soft);
  font-size: 13.5px;
  line-height: 1.65;
  color: var(--vp-c-text-2);
}
.gbui-why strong {
  color: var(--vp-c-text-1);
}
</style>
