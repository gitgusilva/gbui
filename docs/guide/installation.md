# Installation

## Requirements

| | |
| --- | --- |
| Compiler | C++20 — GCC 11+, Clang 14+, MSVC 19.30+ |
| Build | CMake 3.20 or newer |
| Optional | SDL2, for the window backend |

Everything else is vendored or standard. `third_party/stb_truetype.h` (public
domain) rasterises glyphs and is the only third-party code in the tree.

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
find_package(gbui REQUIRED)
target_link_libraries(my_app PRIVATE gbui::gbui)
```

## Build options

| Option | Default | Meaning |
| --- | --- | --- |
| `GBUI_BUILD_TESTS` | on standalone | Build the test suite |
| `GBUI_BUILD_EXAMPLES` | on standalone | Build the examples |
| `GBUI_WERROR` | on standalone | Treat warnings as errors |
| `GBUI_PLATFORM_SDL2` | `ON` | Use SDL2 for windowing when it is present |

"On standalone" means the defaults are on when gbui is the top-level project and
off when it is consumed from another one — your build should not grow a test
suite because of a dependency.

## Run the examples

Four binaries land in `build/examples`, and between them they are the fastest
way to see what the library draws.

```sh
./build/examples/gbui_controls                  # every interactive component
./build/examples/gbui_controls --shot out.ppm --scale 2   # one frame at 2×, no display
./build/examples/gbui_controls --light --design 1         # light mode, Material

./build/examples/gbui_app                                  # a GitBox screen
./build/examples/gbui_app --themes ../gitbox-themes/themes # left/right to switch
./build/examples/gbui_app --shot frame.ppm                 # one frame, no display

./build/examples/gbui_gallery out/ ../gitbox-themes/themes # every theme, four widths
./build/examples/gbui_panel out/                           # a panel, three variants
```

`gbui_controls` is the widget gallery: tabbed pages covering toggles, text,
numbers, pickers, lists, tables, overlays and charts, with the design system and
the theme switchable at runtime — which is the honest test of a toolkit that
claims to re-theme.

`gbui_gallery` writes an `index.html` alongside its SVGs; open it and use the
arrow keys to step through themes and widths. Nothing in the screen it draws is
theme-aware or width-aware, so the sweep is a review of the components and the
themes at once.
