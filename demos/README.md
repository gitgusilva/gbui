# demos

Seven application screens built with nothing but the public library, and three
thin shells that run them: a native window, a headless renderer and a
WebAssembly module.

**These are here to be looked at, not to be used.** Nothing outside this
directory links them, they are not installed, they have no API stability, and
they carry no `gbui::` alias — that alias is the signal that something is part
of the product, and these are not. Copy a screen into your own application if
it is useful; do not depend on this one. `examples/` follows the same rule.

What they *do* prove is why they are separate from `examples/`, which shares a
header and demonstrates the pipeline. These are ordinary consumers: they
include `<gbui/…>`, link `gbui::gbui`, and would build unchanged against an
installed copy of the toolkit. A change that makes them awkward has made the
library awkward — which is the whole reason they exist as a target rather than
as seven loose files.

```
demos/
  include/gbui_demos/
    demos.hpp        the catalogue: Demo, DemoInfo, create()
    host.hpp         everything around a demo that is not the demo
  src/
    kit.hpp          the vocabulary the screens share
    registry.cpp     the catalogue, in the order the gallery reads
    host.cpp
    analytics.cpp    Meridian      · SaaS revenue dashboard
    weather.cpp      Aurora        · a forecaster's desk
    scada.cpp        Helix         · water treatment, SCADA / HMI
    production.cpp   Kaizen        · manufacturing, MES / OEE
    grid.cpp         Voltway       · energy, transmission
    logistics.cpp    Portway       · warehouse and fleet
    markets.cpp      Halyard       · equities, a live trading desk
    catalog/         one live example per component in `gbui::meta`
  runtime/
    native.cpp       a window, a screenshot writer and an SVG recorder
    wasm.cpp         a C surface for JavaScript
  web/
    gbui-embed.js    browser events in, RGBA out — framework-free
```

## Run them

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

./build/demos/gbui_demo --list            # the catalogue
./build/demos/gbui_demo scada             # one, in a window
./build/demos/gbui_demo grid --shot g.ppm # one frame, no display needed
./build/demos/gbui_demo --stills out/     # a frame of every screen
```

While it runs: left and right move between screens, `t` cycles the design
system, `d` toggles light and dark, escape quits.

`-DGBUI_BUILD_DEMOS=OFF` leaves the whole directory out of the build.

## Build for the browser

```sh
tools/build_wasm.sh        # needs the Emscripten SDK
npm run docs:dev           # /demos gets a Run button that works
```

The documentation page opens on the **source**, not on a running screen, and
downloads nothing until the reader presses Run — the module is 1.7 MB and then
rasterises every frame on the CPU, which is not a bill to hand someone who came
to read. Without the bundle built, the page still works: the code view is
highlighted at build time from these files and needs no WebAssembly at all.

The script also finds three faces on the machine to travel with the module — a
browser has no `/usr/share/fonts`, and a UI toolkit with no face draws no text
at all — and writes everything into `docs/public/demo/`, which is ignored by
git and rebuilt by CI.

## Writing another one

A screen is a class with one method. It is handed a `Ui`, the frame's
`Interaction` and a clock, and it writes a tree — which is the whole of what an
application does with this library:

```cpp
class Kiosk final : public Demo {
public:
    NodeId build(Frame& frame) override;
private:
    std::size_t selected_ = 0;      // the model is yours; the toolkit has none
};

DemoInfo kioskDemo() {
    return {.id = "kiosk", .title = "…", /* … */
            .create = [] { return std::unique_ptr<Demo>(new Kiosk()); }};
}
```

Declare it in `src/registry.hpp`, add it to the list in `src/registry.cpp` and
to `CMakeLists.txt`, and it appears everywhere — the runner's `--list`, the
documentation's picker and the browser catalogue all read the same entry.

Fill in `summary`, `highlights` and `tryThis` while you are there: the
documentation site parses the `DemoInfo` out of this file at build time and
shows it under the screen, so that prose is not written twice and cannot drift
from what the screen actually does.

Two rules worth keeping:

- **Take colours from `Token`, never from a literal.** Every screen re-themes
  across four design systems because none of them names a colour.
- **Keep the data deterministic.** `kit::Rolling` runs on the frame clock from a
  seeded wave, so a screenshot taken at a given second is the same picture on
  every machine — which is what stops the documentation's images churning on
  every build.
