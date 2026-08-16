# Download

A shared library, the headers and the CMake package files, for each platform.
Unpack one and point CMake at it — nothing else to install, and nothing to
build.

<GbuiDownloads />

## Use it

```sh
tar xzf gbui-0.3.2-linux-x86_64.tar.gz
cmake -S . -B build -DCMAKE_PREFIX_PATH="$PWD/gbui-0.3.2-linux-x86_64"
```

```cmake
find_package(gbui 0.3 REQUIRED)
target_link_libraries(my_app PRIVATE gbui::gbui)
```

That one line carries the include directory and the C++20 requirement with it.
Everything you write from there is in the `gbui` namespace, and
[your first window](/guide/first-window) is the next page to read.

The library is shared, so it has to be findable when the program runs. On Linux
and macOS the linker writes an rpath if you install your program alongside it;
on Windows `gbui.dll` goes beside the executable or on `PATH`.

## What is in the archive

| | |
| --- | --- |
| `include/gbui/` | the headers |
| `lib/` | the library, and `lib/cmake/gbui/` for `find_package` |
| `bin/` | the DLL, on Windows only |
| `README.md`, `LICENSE` | the same three lines as above, and the terms |

Every archive is unpacked, configured against, built against and *run* on the
machine that produced it before the release is allowed to publish. The program
that does it is [`tools/consumer`](https://github.com/gitgusilva/gbui/tree/main/tools/consumer),
and it uses nothing but the installed headers.

## These builds have no window backend

`Window::create` returns nothing in a downloaded build. Layout, painting, the
display list, the SVG painter, the whole component set — all the same code as a
source build.

The reason is that the backend is SDL2, and a prebuilt library that links SDL2
needs *the same* SDL2 on the machine that loads it. The version a distribution
ships is never the version the build machine had, so the one binary that could
be handed to a stranger is the one with nothing underneath it. Shipping it any
other way would mean shipping a download that fails at load time on most
machines, with a message about a shared object rather than about the reason.

If you want a window, build from source. It is one command and SDL2 is the only
thing to install first:

```sh
git clone https://github.com/gitgusilva/gbui
cmake -S gbui -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The configure step says which backend it found — see
[installation](/guide/installation) for the whole of it, including
`FetchContent` and `add_subdirectory`, which are what most projects should use.
A binary download is for trying the library out and for build machines that
should not be compiling it; a source build is what an application ships against.

## Verifying

Every release carries a `SHA256SUMS` next to the archives:

```sh
sha256sum -c SHA256SUMS
```

## Older versions

The [releases page](https://github.com/gitgusilva/gbui/releases) has every
version, each with the same set of archives and its own section of the
changelog as the release notes.
