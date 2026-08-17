---
layout: page
title: Download
description: Prebuilt gbui libraries for Linux, Windows and macOS.
sidebar: false
aside: false
editLink: false
lastUpdated: false
pageClass: download-page
---

<!-- `vp-doc` on purpose: `layout: page` gives a bare canvas with none of the
     prose styling, so code blocks lose their frame and tables their rules. The
     class is how VitePress says "this part is a document". -->
<div class="download-screen vp-doc">

<div class="download-hero">

# Download gbui

A shared library, the headers and the CMake package files. Unpack one, point
CMake at it, and `find_package(gbui)` finds it — nothing else to install and
nothing to build.

</div>

<GbuiDownloads />

<div class="download-columns">

<div>

## Use it

```sh
tar xzf gbui-0.3.2-linux-x86_64.tar.gz
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH="$PWD/gbui-0.3.2-linux-x86_64"
```

```cmake
find_package(gbui 0.3 REQUIRED)
target_link_libraries(my_app PRIVATE gbui::gbui)
```

That one line carries the include directory and the C++20 requirement with it.
Everything you write from there is in the `gbui` namespace, and
[your first window](/guide/first-window) is the next page to read.

The library is shared, so it has to be findable when the program runs: on Linux
and macOS the linker writes an rpath if you install your program alongside it,
and on Windows `gbui.dll` goes beside the executable or on `PATH`.

</div>

<div>

## What is in the archive

| | |
| --- | --- |
| `include/gbui/` | the headers |
| `lib/` | the library, and `lib/cmake/gbui/` for `find_package` |
| `bin/` | the DLL, on Windows only |
| `README.md`, `LICENSE` | the three lines above, and the terms |

Every archive is unpacked, configured against, built against and **run** on the
machine that produced it before the release is allowed to publish. The program
that does it is
[`tools/consumer`](https://github.com/gitgusilva/gbui/tree/main/tools/consumer),
and it uses nothing but the installed headers.

Verify what you downloaded:

```sh
sha256sum -c SHA256SUMS
```

</div>

</div>

<div class="download-note">

## These builds have no window backend

`Window::create` returns nothing in a downloaded build. Layout, painting, the
display list, the SVG painter, the whole component set — all the same code as a
source build.

The reason is that the backend is SDL2, and a prebuilt library that links SDL2
needs *the same* SDL2 on the machine that loads it. The version a distribution
ships is never the version the build machine had, so the one binary that can be
handed to a stranger is the one with nothing underneath it. Shipping it any
other way would mean shipping a download that fails at load time on most
machines, with a message about a shared object rather than about the reason.

If you want a window, build from source. It is one command, and SDL2 is the only
thing to install first:

```sh
git clone https://github.com/gitgusilva/gbui
cmake -S gbui -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

See [installation](/guide/installation) for the whole of it, including
`FetchContent` and `add_subdirectory` — which are what most projects should use.
A binary download is for trying the library out and for build machines that
should not be compiling it; a source build is what an application ships against.

</div>

<p class="download-foot">
Every version is on the
<a href="https://github.com/gitgusilva/gbui/releases">releases page</a>, each
with the same set of archives and its own section of the changelog as the
release notes.
</p>

</div>

<style>
.download-screen {
  max-width: 1100px;
  margin: 0 auto;
  padding: 48px 24px 80px;
}

.download-hero {
  max-width: 640px;
  margin-bottom: 32px;
}

.download-hero h1 {
  margin: 0 0 12px;
  font-size: 40px;
  line-height: 1.15;
  font-weight: 700;
  letter-spacing: -0.02em;
}

.download-hero p {
  margin: 0;
  font-size: 16px;
  line-height: 1.7;
  color: var(--vp-c-text-2);
}

.download-columns {
  display: grid;
  gap: 8px 40px;
  grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
  margin-top: 40px;
  padding-top: 24px;
  border-top: 1px solid var(--vp-c-divider);
}

.download-screen h2 {
  margin: 24px 0 12px;
  padding: 0;
  border: 0;
  font-size: 17px;
  font-weight: 600;
  letter-spacing: -0.01em;
}

.download-screen p,
.download-screen li,
.download-screen td {
  font-size: 14px;
  line-height: 1.7;
}

.download-screen pre {
  font-size: 12.5px;
}

.download-note {
  max-width: 720px;
  margin-top: 40px;
  padding-top: 24px;
  border-top: 1px solid var(--vp-c-divider);
}

.download-foot {
  margin-top: 32px;
  color: var(--vp-c-text-3);
  font-size: 13px;
}
</style>
