# Installation

## Requirements

| | |
| --- | --- |
| Compiler | C++20 — GCC 11+, Clang 14+, MSVC 19.30+ |
| Build | CMake 3.20 or newer |
| Optional | SDL2, for the window backend |

Everything else is vendored or standard. `third_party/stb_truetype.h` (public
domain) rasterises glyphs and is the only third-party code in the tree.

## Or don't build it

Every release carries a shared library, the headers and the CMake package files
for Linux, Windows and both kinds of Mac — see [download](/download). Unpack
one, point `CMAKE_PREFIX_PATH` at it, and `find_package(gbui)` finds it.

Those builds have no SDL2 and therefore no window: a prebuilt library that links
SDL2 needs the same SDL2 on the machine that loads it, which a download cannot
promise. Everything else in the library is the same code. Build from source if
you want a window — which is the rest of this page, and is what an application
should be shipping against anyway.

## Build it

```sh
git clone https://github.com/gitgusilva/gbui
cd gbui
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The configure step reports which backend it found:

```
-- gbui 0.2.0: SDL2 backend ON
```

Without SDL2 the library still builds and every test passes; only
`Window::create` changes, returning nothing instead of a window. Rendering
offscreen — into a `Canvas` or an SVG — works either way, which is what lets a
build machine with no display still produce a frame.

## Use it from your project

As a subdirectory:

```cmake
add_subdirectory(gbui)
target_link_libraries(my_app PRIVATE gbui::gbui)
```

With `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(gbui
    GIT_REPOSITORY https://github.com/gitgusilva/gbui
    GIT_TAG        main)
FetchContent_MakeAvailable(gbui)
target_link_libraries(my_app PRIVATE gbui::gbui)
```

Or installed:

```sh
cmake --install build --prefix /usr/local
```

```cmake
find_package(gbui 0.2 REQUIRED)
target_link_libraries(my_app PRIVATE gbui::gbui)
```

`gbui::gbui` carries its own include directory, its C++20 requirement and its
dependency on SDL2 when the build had one, so that one line is the whole of the
integration. Everything you then write is in the `gbui` namespace:

```cpp
gbui::Arena arena;
gbui::Ui ui(arena);
gbui::text(ui, "Local Changes", {.color = gbui::Token::TextStrong});
```

## Static or shared

Static by default, shared with the switch every CMake project uses:

```sh
cmake -S . -B build -DBUILD_SHARED_LIBS=ON
cmake --install build --prefix /usr/local
```

| | |
| --- | --- |
| Linux | `libgbui.so.0.2.0`, with an `SONAME` of `libgbui.so.0` |
| macOS | `libgbui.0.2.0.dylib` |
| Windows | `gbui.dll` in `bin/`, with `gbui.lib` — the import library — in `lib/` |

The consumer side does not change: `find_package(gbui)` and `gbui::gbui` either
way. On Windows the DLL has to be findable at runtime, which means on `PATH` or
beside the executable; Linux and macOS resolve it through the `RPATH` the linker
writes.

`tools/consumer/` is a project that does exactly this and nothing else, and CI
builds it against a fresh install on all three platforms after every change —
because a DLL that exports nothing still builds perfectly, and only fails at the
link step of whoever tries to use it.

::: warning One ABI caveat, and it is the usual one
The public API passes `std::string`, `std::vector`, `std::optional` and
`std::function` across the boundary. That is a deliberate trade — it is what
keeps the API pleasant to write — and it means a shared gbui and its consumer
have to be built by **the same compiler and standard library**, and on MSVC with
the same runtime and configuration. Mixing a Debug consumer with a Release DLL
is undefined behaviour that usually shows up as a corrupted string.

A C ABI with opaque handles would remove that constraint and is not planned.
:::

## Build options

| Option | Default | Meaning |
| --- | --- | --- |
| `GBUI_BUILD_TESTS` | on standalone | Build the test suite |
| `GBUI_BUILD_DEMOS` | on standalone | Build the demo screens and their runners |
| `GBUI_WERROR` | on standalone | Treat warnings as errors |
| `GBUI_PLATFORM_SDL2` | `ON` | Use SDL2 for windowing when it is present |

"On standalone" means the defaults are on when gbui is the top-level project and
off when it is consumed from another one — your build should not grow a test
suite because of a dependency.

## Run it

One binary, `build/demos/gbui_demo`, and it is the fastest way to see what the
library draws. It carries seven application screens and a live example of every
single component, and it renders them in a window or straight to a file.

```sh
./build/demos/gbui_demo --list              # the screens
./build/demos/gbui_demo weather             # open one
./build/demos/gbui_demo --components        # every component the library has
./build/demos/gbui_demo --component select  # one component's live example
```

Left and right move between screens while it runs, `t` cycles the design system
and `d` toggles light and dark — which is the honest test of a toolkit that
claims to re-theme.

Nothing here needs a display:

```sh
./build/demos/gbui_demo weather --shot out.ppm --scale 2   # one frame at 2×
./build/demos/gbui_demo weather --svg out.svg              # the same frame as SVG
./build/demos/gbui_demo --stills out/ --at 12              # every screen, 12s in
./build/demos/gbui_demo --component slider --shot s.ppm --at-pointer 420 260
```

`--at` winds the clock forward before the shot, so an animation is caught where
you want it rather than at frame one, and `--at-pointer` parks the pointer so a
still can catch a hover — a chart's readout, a row's highlight.

`--theme` takes a palette file in the
[gitbox-themes](https://github.com/gitgusilva/gitbox-themes) format, which is
what `Theme::fromJson` reads. A whole registry is a shell loop:

```sh
for t in ../gitbox-themes/themes/*/theme.json; do
  ./build/demos/gbui_demo analytics --theme "$t" \
    --svg "out/$(basename "$(dirname "$t")").svg"
done
```
