# Writing a backend

A backend implements six methods. Everything the toolkit can draw arrives as one
of them, in absolute coordinates, with colours already resolved from the theme.

```cpp
class MyPainter final : public Painter {
public:
    void fillRect(const FillRect&) override;
    void strokeRect(const StrokeRect&) override;
    void drawText(const DrawText&) override;
    void drawPath(const DrawPath&) override;
    void pushClip(const PushClip&) override;
    void popClip() override;
};
```

Then:

```cpp
DisplayList list;
record(arena, root, theme, list, measure);
MyPainter painter;
painter.paint(list);          // replays the list in order
```

Two implementations ship with the library and are worth reading before you write
a third: `SvgPainter` is the last 140 lines of `src/paint/paint.cpp`, and
`SoftwarePainter` — a rasteriser with antialiasing, gradients and rounded
clipping — is `src/paint/canvas.cpp`.

## The commands

| Command | Carries |
| --- | --- |
| `FillRect` | rect, paint, corner radius |
| `StrokeRect` | rect, paint, width, radius — the border sits *inside* the rect, as CSS does |
| `DrawText` | the content box, the run, paint, family, size, weight, slant, alignment, baseline offset |
| `DrawPath` | flattened contours, paint, and a stroke width where zero means fill |
| `PushClip` / `PopClip` | a rect and radius; nested, and always balanced |

Coordinates are **device** pixels by the time a backend sees them: everything
above the display list works in logical units and `DisplayList::setScale`
multiplies once on the way in — geometry, radii, stroke widths and font sizes
together. A backend does no scaling of its own.

Text is already elided and already broken into lines. The run in `DrawText` is
what should be drawn, not what the node contained.

## Paint is a colour or a gradient

```cpp
void MyPainter::fillRect(const FillRect& command) {
    if (!command.paint.isGradient()) { solid(command.rect, command.paint.color); return; }
    for (const auto& [position, color] : command.paint.gradient.stops) { … }
}
```

A struct rather than a variant, because every backend has to handle both and
`paint.isGradient()` reads better at each of those sites than a visitor.
`gradient.kind` is linear or radial, `angle` follows CSS — 0 points up, 90
points right — and `Paint::at(t)` samples the ramp for a backend that would
rather interpolate itself. The stops arrive already resolved against the theme,
and `scaleAlpha` has already been applied where a subtree carried an opacity.

## Measuring

A backend with a real font engine should also supply measurement, or layout and
painting will disagree about how wide a string is:

```cpp
LayoutContext context;
context.theme = &theme;
context.measure = [&](std::string_view text, const TextStyle& style,
                      const Typography& typography, float maxWidth) -> TextMetrics {
    return myShaper.measure(text, style, typography, maxWidth);
};

layout(arena, root, viewport, context);
record(arena, root, theme, list, context.measure);   // the same function
```

Passing different functions to those two calls is a real bug with a visible
symptom: runs elide while they still fit. `measureWith(fonts)` does this for the
built-in font module.

## Platform backends

A window backend implements `Window` — `pumpEvents`, `canvas`, `present`,
`size`, `scale`, `mouse`, `takeInput`, `resized`, `setCursor` — and is selected
by `Window::create`. The SDL2 one is a single file, and it uses SDL for the
window and the events only: every pixel is drawn by the toolkit, so a native
Win32, Cocoa or Wayland backend is a copy of that file with the platform calls
swapped.

Three of those methods are the whole of what a new backend has to think about:

- **`takeInput`** builds an `InputFrame` — pointer, wheel, modifiers as a
  *level*, key events and composed UTF-8 text — and is drained by reading.
  Printable characters belong in `text`, never in `keys`.
- **`scale`** is device pixels per logical pixel. Everything else in the
  application stays in logical units.
- **`present`** returns when the frame has been shown, so the display paces the
  loop. Nothing sleeps: a fixed sleep cannot know when the next refresh is, and
  16 ms of sleep plus 5 ms of work misses a 16.7 ms refresh and waits for the
  one after.

When no backend is compiled in, `Window::create` returns nothing and offscreen
rendering still works. That is deliberate: a build machine with no display
should still be able to render a frame to a file.
