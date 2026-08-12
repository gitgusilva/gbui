---
title: Demos
description: Six application screens built with gbui, running in your browser through WebAssembly.
# The canvas opts out of the prose column; see .vitepress/theme/custom.css.
pageClass: demos-page
aside: false
---

# Demos

Six application screens, running here, in this page. Not a video and not a
screenshot: the C++ below is compiled to WebAssembly, lays out and rasterises
every frame on the CPU, and copies the result into a `<canvas>`. There is no
DOM inside that rectangle — no elements, no CSS, no WebGL. What you are
pointing at is a display list.

Click a row, drag a slider, sort a table, pan a chart. Click into the demo
first to give it the wheel and the keyboard; <kbd>Esc</kbd> hands them back to
the page.

<GbuiDemo picker :height="660" />

## What each one is for

The set is chosen to cover the kinds of screen a UI toolkit is actually asked
for, and between them they exercise every chart, every container and every
interactive control the library has.

### Meridian Analytics · SaaS

The familiar one, and it goes first for that reason: a reader who has built
this screen in React knows what they are looking at and can spend their
attention on how the toolkit says it. KPI tiles with sparklines, a line chart
you can pan and zoom over a brush strip, a channel donut, and a table that
sorts when you click a header — reordering the rows is the application's job,
because only it knows how to compare two of its own.

**Try:** drag inside the revenue chart; hold <kbd>Ctrl</kbd> and scroll to zoom;
drag the window along the strip underneath; click `ARR` twice.

### Aurora Weather Desk · Meteorology

A forecaster's wall display. Almost nothing on it is clickable, which is the
point — the work is making eleven numbers, a week of forecasts and a rainfall
chart legible from across a room, and that is a typography and spacing problem
rather than an interaction one. The current observation is a dial and a wrapping
grid of fields; the station network at the bottom is selectable.

**Try:** click a station in the network to move the whole screen to it.

### Helix Process Control · Water treatment, SCADA / HMI

A plant supervisory screen, and the one that behaves differently from the rest:
it is **always dark**, whatever your preference at the top of this page says. An
operator at three in the morning is not helped by a light theme, so the demo
declares a fixed palette and the host honours it.

Everything on it is a reading against a setpoint rather than a number on its
own, and it is genuinely operable.

**Try:** switch a pump off; drag the flow setpoint and watch the line in the
trend move with it; acknowledge an alarm.

### Kaizen Line Monitor · Manufacturing, MES / OEE

OEE is one number made of three, and the screen exists to show which of the
three is losing the shift. The two charts here are the ones a dashboard rarely
reaches for and a factory always does: a stacked bar over time, and a heatmap
that turns a table nobody reads into a shape anybody can. The `Takt` column is a
meter drawn *inside* a table cell.

**Try:** hover a cell in the defect grid; hover a lollipop in the Pareto.

### Voltway Grid Operations · Energy, transmission

Demand against generation, a day-ahead candlestick settlement and a live feeder
table. The candlestick is the one chart in the toolkit whose scale deliberately
does not reach zero — a price has no baseline, and forcing one on a market that
trades between 45 and 95 squeezes the whole day into the top of the plot.

**Try:** hover a candle; hover the donut's wedges, then click one to single it
out.

### Portway Control Tower · Logistics, WMS / TMS

A warehouse and fleet desk, carrying the scatter — the only chart here with a
real *x* scale. Every other chart spaces its samples evenly along the bottom,
which is right for a series over time and wrong for a correlation. Lateness
against distance, with the pallet count in the size of the dot.

**Try:** hover a bubble out on the right.

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
