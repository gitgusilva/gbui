# paint

`#include "gbui/paint/paint.hpp"`, `canvas.hpp`

## Recording

```cpp
DisplayList list;
list.setScale(window->scale());
record(arena, root, theme, list, measure);
```

Walks a laid-out tree and produces a flat, ordered sequence of drawing commands
with everything resolved: absolute coordinates, concrete colours, elided or
wrapped text. Layout must have run first — this reads frames, it does not
compute them.

`measure` has to be the same function layout was given, for the reason on the
[layout](/reference/layout#measurement) page.

## Scale

```cpp
void DisplayList::setScale(float scale);   // device pixels per logical pixel
float DisplayList::scale() const;
```

Everything above the display list works in **logical** units — layout, hit
testing and the input events all speak the same coordinates. The conversion
happens here, once, on the way into the list, so there is exactly one multiply
in the pipeline and no component can forget it.

It scales geometry *and* radii, border widths and font sizes. A border that
stayed one pixel while everything around it doubled is the giveaway that someone
scaled the rectangles and stopped there.

## Paint

```cpp
struct ResolvedGradient { GradientKind kind; float angle; std::vector<std::pair<float, Color>> stops; };

struct Paint {
    Color color;
    ResolvedGradient gradient;

    bool  isGradient() const;      // two or more stops
    Color at(float t) const;       // the colour along the ramp, clamped at the ends
    void  scaleAlpha(float factor);
};
```

A struct rather than a variant, because every backend has to handle both and
`paint.isGradient()` reads better at each of those sites than a visitor. The
colours arrive already resolved against the theme.

## Commands

```cpp
struct FillRect   { Rect rect; Paint paint; float radius; };
struct StrokeRect { Rect rect; Paint paint; float width; float radius; };
struct DrawText   { Rect box; std::string_view text; Paint paint;
                    std::string_view family; float size; FontWeight weight;
                    FontSlant slant; TextAlign align; float baseline; };
struct DrawPath   { Path path; Paint paint; float strokeWidth; };  // 0 = fill
struct PushClip   { Rect rect; float radius; };
struct PopClip    {};

using DrawCommand = std::variant<FillRect, StrokeRect, DrawText, DrawPath,
                                 PushClip, PopClip>;
```

A border sits **inside** its rectangle, as a CSS border does. Clips nest, carry
a radius — the boxes this toolkit clips against are rounded — and are always
balanced.

## DisplayList

```cpp
list.add(command);
list.commands();       // const std::vector<DrawCommand>&
list.size();
list.empty();
list.reserve(n);
list.clear();
std::string_view owned = list.own(std::move(string));   // for elided runs
```

Being a plain vector is the point: it can be counted, inspected and asserted on
in a test without rendering anything. `own` takes a string the list has to
outlive and hands back a view; it is a `deque`, so earlier views stay valid.

## Painter

```cpp
class Painter {
    virtual void fillRect(const FillRect&) = 0;
    virtual void strokeRect(const StrokeRect&) = 0;
    virtual void drawText(const DrawText&) = 0;
    virtual void drawPath(const DrawPath&) = 0;
    virtual void pushClip(const PushClip&) = 0;
    virtual void popClip() = 0;

    void paint(const DisplayList&);   // replays in order
};
```

Six methods, on purpose: a rasteriser, a GPU renderer and the SVG writer all fit
behind it. See [Writing a backend](/guide/writing-a-backend).

## SvgPainter

```cpp
SvgPainter painter(width, height, background);
painter.paint(list);
std::string document = painter.finish();
```

Writes an SVG document, gradients and clips included. Useful for golden-image
tests, design review without a build of the application, and documentation
screenshots.

## Canvas and SoftwarePainter

```cpp
Canvas canvas(1180, 620);
canvas.clear(theme.color(Token::Bg));

SoftwarePainter painter(canvas, fonts, theme.typography());
painter.paint(list);

const std::uint8_t* pixels = canvas.pixels();   // premultiplied RGBA8, row-major
std::size_t pitch = canvas.pitch();             // bytes per row
```

A CPU rasteriser. Rounded rectangles, strokes and vector paths get their
antialiasing from a signed distance to the geometry; glyph coverage arrives from
the font module and is blended the same way. A fill visits the distance function
only near its edges and a stroke never visits its own interior, which took one
frame of the example screen from 11.4 ms to 1.8 ms with pixel-identical output.

Drawing primitives are available directly when something needs them:

```cpp
struct Clip { Rect rect; float radius; };   // rounded, because the boxes are

canvas.fillRoundedRect(rect, radius, paint, clip);
canvas.strokeRoundedRect(rect, radius, thickness, paint, clip);
canvas.drawPath(path, paint, strokeWidth, clip);
canvas.blendCoverage(x, y, w, h, coverage, paint, clip, gradientBox);
```
