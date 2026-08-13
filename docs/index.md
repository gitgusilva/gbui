---
layout: home

hero:
  name: gbui
  text: A UI toolkit for C++
  tagline: Flexbox layout, themeable components and a painter you can implement — with no web engine and nothing to fetch.
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

<GbuiDemo id="markets" :height="600" />

A real application screen and the C++ that draws it. Press Run and it is the
same source compiled to WebAssembly, laid out and rasterised on the CPU — there
is no DOM inside that rectangle, no elements and no CSS. What you are pointing
at is a display list.

[Six more screens](/demos) · [How it is put together](/guide/architecture) ·
[Every component](/components)
