# Architecture

## Three stages, one direction

Everything the library does is one of three stages, always in this order:

```cpp
Arena arena;                                  // 1. build   — what exists
Ui ui(arena);
{ auto root = ui.column({}); /* … */ }

layout(arena, ui.root(), viewport, context);  // 2. layout  — where it goes
record(arena, ui.root(), theme, list);        // 3. paint   — what to draw
painter.paint(list);                          //    …and a backend draws it
```

Nothing reads backwards. Layout writes a `frame` and a `content` box onto every
node; `record` only reads them; a painter never sees a node at all.

Three consequences follow, and they are the reason for the design:

1. **Layout is testable.** It is arithmetic over structs, so a test asserts
   positions with no window, no GPU and no font installed.
2. **Painting is inspectable.** A `DisplayList` is a flat vector of commands you
   can print, count or compare. "This button paints the accent colour" is a
   test, not a screenshot.
3. **Backends are interchangeable.** The software rasteriser and the SVG writer
   consume the same list, so the picture in the documentation is drawn by the
   same code path as the pixels on screen.

## Modules

`include/gbui/<module>/` and `src/<module>/` mirror each other. A module may
include from itself and from the modules above it in this table, never below.

| Module | Depends on | Holds |
| --- | --- | --- |
| `core` | — | `geometry` (`Vec2`, `Rect`, `Edges`, `Length`, `kAuto`), `color`, `cursor`, `json`, `path` |
| `anim` | core | `easing` (the curve set), `animator` (the clock) |
| `style` | core, anim | `style` (flexbox and paint properties), `theme` (tokens, typography), `design` (shape, sizing, motion) |
| `layout` | core, style, scene | flexbox, min-content sizing, wrapping, hit testing, `textWrap` |
| `scene` | core, style, layout, anim | `tree` (arena, node, shapes), `ui` (the building API) |
| `overlay` | core | `placement`: where a floating box goes |
| `input` | core, scene | `keys`, `interaction` (hover, press, focus, wheel), `textEdit` |
| `paint` | core, style, scene, layout | display list, painter interface, SVG and software backends |
| `widgets` | all of the above | one file per component, plus `icons` and the private `detail` |
| `platform` | all of the above | window, event loop, fonts, `openUrl` |

Two entries in that table are worth explaining. `scene` includes `layout`
because `Ui` hands components the measurer — a caret has to sit at a byte offset
that has a position on screen, and only the function layout uses can say where
that is. And `style` includes `anim` because a `Design` carries the transition
everything animated uses, which is what makes switching design change the *feel*
and not only the look.

`third_party/` may only be reached from `platform/`. Everything above it is
standard library and nothing else, which is what keeps the core portable to a
machine with no font stack and no display server.

## Naming and namespaces

The library follows the convention that Abseil, Qt and Boost all land on in
their own way: **one root namespace, modules as directories and targets, not as
separate namespaces.**

```cpp
namespace gbui { ... }               // everything public
namespace gbui::json { ... }         // a sub-namespace when names would collide
namespace gbui::charts { ... }       // a module large enough to own its vocabulary
```

- **Root namespace: `gbui`.** There is no `gbcore`, no `gbchart`. A second
  top-level namespace buys nothing and costs every user an extra name to learn.
- **A sub-namespace when the module has its own vocabulary** that would
  otherwise collide — `json::Value` and `charts::Axis` earn one; `Button` and
  `Theme` do not.
- **Headers are `gbui/<module>/<file>.hpp`**, so an include says which module it
  came from and the dependency table above can be checked by eye.
- **CMake targets are namespaced**: `gbui::gbui` today, `gbui::charts` when
  charts arrive as a separate library. A consumer writes
  `target_link_libraries(app PRIVATE gbui::gbui gbui::charts)`.
- **A module becomes its own library** when it pulls in a dependency the core
  does not need, or when a user would reasonably want one without the other.
  Charts and the platform backends qualify; `layout` never will.

### Code style

| Thing | Style | Example |
| --- | --- | --- |
| Types | `CamelCase` | `DisplayList`, `NodeId` |
| Functions and methods | `camelBack` | `listRow`, `intrinsicMainSize` |
| Private members | trailing underscore | `contours_` |
| Constants | `k` prefix | `kAuto`, `kMenuItemHeight` |
| Files | named for the symbol, same case | `textInput.hpp` declares `textInput()` |
| Tests | `<subject>Test.cpp` | `textWrapTest.cpp` |
| Options | an `…Options` struct | `ButtonOptions` |

The file naming is deliberately **not** the usual C++ convention — the standard
library, LLVM and Chromium all pair `snake_case` files with `CamelCase` symbols.
The trade was made the other way here because almost nothing in this library is
a class: the public surface is free functions, and a reader looking for
`textInput` should not have to know it lives in `text_input.hpp`.
Directories stay lowercase.

`.clang-format` and `.clang-tidy` at the repository root enforce the mechanical
half. The library builds with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion
-Wsign-conversion -Werror`, or `/W4 /WX` under MSVC, on all four of the
compilers CI runs. A warning that is wrong gets the narrowest suppression that
silences it and a comment saying why.

## Errors

There are no exceptions in the public API and none thrown internally. A call
that can fail returns `std::optional` and, where the reason matters, fills in an
error string that names the field at fault:

```cpp
std::string error;
const auto theme = Theme::fromFile("themes/nord/theme.json", &error);
if (!theme) {
    // "colors.accentHover is missing" — not "parse error"
    std::fprintf(stderr, "%s\n", error.c_str());
}
```

The rule behind it: a failure caused by data somebody else authored — a theme,
an icon, a stylesheet — has to name what is wrong with it, because the person
who can fix it is not the person running the program.

## Comments

Comments explain **why**. What the code does is already written down in the
code, and a comment that restates it goes stale silently. Anything surprising —
a workaround, a spec quirk, a bug that shipped once — is worth a sentence, and
the bug that shipped once is worth naming so it does not come back.
