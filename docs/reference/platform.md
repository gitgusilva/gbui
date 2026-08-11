# platform

`#include "gbui/platform/window.hpp"`, `font.hpp`, `shell.hpp`

The only module that knows what machine it is on. Everything above it builds and
tests without a display server or a font installed.

## Window

```cpp
WindowOptions options{.title = "gbui", .width = 1180, .height = 640};
std::unique_ptr<Window> window = Window::create(options);
if (!window) { /* no backend, or it failed to open */ }

while (window->pumpEvents()) {
    const InputFrame input = window->takeInput();
    interaction.update(arena, root, input);
    window->setCursor(interaction.cursor());

    Canvas& canvas = window->canvas();
    // …build, lay out at window->size(), record with window->scale(), paint…
    window->present();
}
```

| Call | Answers |
| --- | --- |
| `pumpEvents()` | false once the user has closed the window |
| `canvas()` | the framebuffer, already sized to the window |
| `present()` | uploads and flips — and, with vsync, returns when the frame has been shown |
| `size()` | the drawable size in **logical** pixels |
| `scale()` | device pixels per logical pixel: 1, 2, or fractional |
| `mouse()` | position and left button state |
| `takeInput()` | everything since the last call — pointer, wheel, modifiers, keys, typed text — drained by reading |
| `resized()` | whether the size changed since the last frame |
| `setCursor(cursor)` | the pointer shape, from `Interaction::cursor()` |

```cpp
struct WindowOptions {
    std::string title = "gbui";
    int width = 1180, height = 640;
    int minWidth = 420, minHeight = 320;
    bool vsync = true;
};
```

SDL2 supplies the window and the events and nothing else — every pixel comes
from `Canvas`, so a native Win32, Cocoa or Wayland backend is a copy of one
file. Without a backend compiled in, `Window::create` returns nothing and
offscreen rendering still works.

**`vsync` is on for a reason.** The alternative is not "sleep a while at the end
of the frame": a fixed sleep cannot know when the next refresh is, so 16 ms of
sleep plus 5 ms of work misses a 16.7 ms refresh and lands on the one after —
which is how a loop with 5 ms of work in it presents at 46 fps. Turn it off to
measure how fast the toolkit can actually draw.

## Fonts

```cpp
FontDatabase fonts;
fonts.addSearchPath("/opt/fonts");                  // beyond the system defaults

std::shared_ptr<Font> font = fonts.font("IBM Plex Sans", false, 13.0f,
                                        {.weight = FontWeight::SemiBold});
TextMetrics metrics = font->measure("Local Changes");
const Glyph* glyph = font->glyph(U'L');             // rasterised and cached
```

`FontDatabase` resolves a family the theme asks for against the files on the
machine, ranking candidates by family, then by weight and slant distance, then
by how much name the file has left over, and walking down the list until one
loads — a candidate can be a collection or a variable font the rasteriser will
not open. Failing that it falls back through a list of common families, so a
theme naming a font nobody has still renders. What the machine cannot supply is
synthesised: dilated coverage for weight, a shear for italic.

### The same window on three machines

```cpp
fonts.clearSearchPaths();                                   // drop the system paths
fonts.addFontFile("Inter", "assets/Inter-Regular.ttf");
fonts.addFontFile("Inter", "assets/Inter-SemiBold.ttf", FontWeight::SemiBold);
fonts.addFontFile("Inter", "assets/Inter-Italic.ttf", FontWeight::Regular,
                  FontSlant::Italic);
```

This is CSS's `@font-face`, and it is what makes an application look the same on
Linux, Windows and macOS. A family resolved from the machine is whatever that
machine happens to have, and the metrics of a substitute are not the metrics of
the design — so wrapping and elision differ slightly between machines.
Registered faces are searched *before* anything installed, so a bundled "Inter"
wins over a system one.

### Measuring

```cpp
LayoutContext context;
context.measure = measureWith(fonts, window->scale());
```

The seam that turns "laid out about right" into text that measures the way it
will draw. The face is rasterised at the **device** size and its metrics divided
back down, so glyphs are sharp rather than a magnified 13-pixel bitmap, while
wrapping still breaks at the same words on every display.

`fontFor(fonts, style, typography, scale)` is the same resolution, exposed so a
painter and the layout cannot disagree. `nextCodepoint` decodes UTF-8 one code
point at a time, because both the measurer and the rasteriser walk text the same
way.

**Two real limits.** stb_truetype opens a file's default instance, so a family
shipped as one variable file per slant has exactly one weight available whatever
its name promises; and weight and slant are read from the file *name* — the
convention every tool leans on — so a file named against convention is read
wrongly. There is no shaping (Latin is correct, complex scripts are not) and no
glyph atlas: glyphs are cached per face and blended one at a time.

## Opening a URL

```cpp
bool openUrl(std::string_view url);
```

`xdg-open` on Linux and the BSDs, `open` on macOS, `ShellExecute` on Windows.

**Only `http`, `https` and `mailto` are opened.** The desktop's opener will
happily launch a `file://` path or a `.desktop` entry, and a URL usually arrives
from a document, a commit message or a remote — places where the person running
the application did not choose it. Widening that list is a decision for whoever
knows where their URLs come from, so it is not made here.

The child is started with `fork` and `exec`, never through a shell, so a URL
containing quotes, semicolons or backticks is an argument and not a command.
`false` means the scheme was refused or no opener could be started — never that
the browser itself failed, which happens after this returns and cannot be
observed.
