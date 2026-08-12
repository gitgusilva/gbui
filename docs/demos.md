---
title: Demos
description: Six application screens built with gbui, running in your browser through WebAssembly.
# The canvas opts out of the prose column; see .vitepress/theme/custom.css.
pageClass: demos-page
aside: false
---

# Demos

Six application screens, and the C++ that draws them. Press Run and they are
not a video and not a screenshot: the same source is compiled to WebAssembly,
lays out and rasterises every frame on the CPU, and copies the result into a
`<canvas>`. There is no DOM inside that rectangle — no elements, no CSS, no
WebGL. What you are pointing at is a display list.

Between them the six exercise every chart, every container and every
interactive control the library has. They live in
[`demos/`](https://github.com/gitgusilva/gbui/tree/main/demos) and use nothing
but the public headers, so a change that makes them awkward has made the
library awkward.

Pick a screen, read the C++ that draws it, and press **Run** when you want to
see it move. Nothing downloads until you do: the module is 1.7 MB and then
rasterises every frame on the CPU, which is not a bill to hand someone who came
to read.

<GbuiDemo :height="640" />

## How it is put together

The screens themselves know nothing about any of this. Each is a class with one
method that is handed a `Ui`, the frame's `Interaction` and a clock, and writes
a tree — exactly what an application does:

```cpp
class Weather final : public Demo {
public:
    NodeId build(Frame& frame) override;
    // ... the model the screen edits
};
```

Around them sits one class, `demos::Host`, which owns the arena, the fonts, the
theme, the animation clock and the interaction state and runs the four pipeline
stages in order. Three shells drive it and each is thin:

| | |
|---|---|
| `demos/runtime/native.cpp` | a window, a screenshot writer and an SVG recorder |
| `demos/runtime/wasm.cpp` | a C surface for JavaScript, about a hundred lines |
| `demos/web/gbui-embed.js` | browser events in, RGBA out — framework-free |

That division is what makes the same six screens run in a native window, in a
headless CI job and in this page without one line of them knowing which.

### Run them locally

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

./build/demos/gbui_demo --list          # the catalogue
./build/demos/gbui_demo scada           # open one in a window
./build/demos/gbui_demo --stills out/   # a frame of each, with no display
```

Left and right move between screens, `t` cycles the design system, `d` toggles
light and dark.

### Build the WebAssembly bundle

The module on this page is a build artefact and is not committed. With the
[Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)
installed:

```sh
tools/build_wasm.sh
npm run docs:dev
```

The script finds three faces on your machine to travel with the module — a
browser has no `/usr/share/fonts`, and a UI toolkit with no face draws no text
at all — and writes everything into `docs/public/demo/`.

### Put one in your own page

`gbui-embed.js` is a plain ES module and depends on nothing:

```js
import { mountDemo } from '/demo/gbui-embed.js'

const demo = await mountDemo(document.querySelector('canvas'), { id: 'grid' })
demo.setDark(false)
demo.setSkin('material')
demo.destroy()
```

## Where the honest edges are

Worth saying, because a demo page is where a toolkit is most tempted to
overstate itself.

**Text is not shaped.** Runs are laid out glyph by glyph, so Arabic, Devanagari
and emoji come out wrong. Everything on these screens is Latin, which is
convenient and not an accident.

**There is no accessibility tree.** The canvas takes focus and the toolkit has
its own focus ring, Tab traversal and `:focus-visible` behaviour — but a screen
reader sees one blank element. That is the largest single gap in the library and
it is named as such in the module that would own it.

**Everything is rasterised on the CPU**, in WebAssembly, single-threaded. That
it keeps up with a 60 Hz display at this size is the interesting part; a GPU
painter is the same six methods and does not exist yet.

**The data is simulated** from a seeded, deterministic wave, so a screenshot
taken at a given second is the same picture on every machine. Nothing here
talks to a plant, a market or a warehouse.
