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
