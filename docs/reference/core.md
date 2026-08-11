# core

`#include "gbui/core/geometry.hpp"`, `color.hpp`, `cursor.hpp`, `json.hpp`,
`path.hpp`

The layer with no dependencies of its own: arithmetic, parsing and geometry.

## Geometry

```cpp
struct Vec2  { float x, y; };
struct Edges { float top, right, bottom, left; };   // always the CSS order
struct Rect  { float x, y, width, height; };
```

| Call | Answers |
| --- | --- |
| `Edges::all(v)`, `Edges::symmetric(vertical, horizontal)` | the usual shorthands |
| `edges.horizontal()`, `edges.vertical()` | the two sums |
| `rect.right()`, `rect.bottom()` | the far edges |
| `rect.contains(point)` | hit testing, half-open on the far edges |
| `rect.deflate(edges)` | the content box inside padding and border |
| `rect.intersect(other)` | the overlap, empty when they miss — clipping |
| `rect.translated(dx, dy)` | the same box moved |
| `rect.empty()` | zero or negative on either axis |

```cpp
inline constexpr float kAuto;   // a quiet NaN
bool isAuto(float value);
float resolve(float value, float fallback);
```

`kAuto` is how "unset" is spelled without wrapping every field in
`std::optional`, which keeps `Style` a plain aggregate.

## Length

```cpp
struct Length {
    float value = kAuto;
    bool  relative = false;      // value is a percentage, 0..100

    constexpr Length(float pixels);          // implicit, on purpose
    static constexpr Length px(float);
    static constexpr Length percent(float);
    static constexpr Length autoSize();

    bool  isAuto() const;
    float resolve(float basis) const;        // kAuto when the basis is unknown
};
```

The implicit constructor from `float` is the whole reason this could be
introduced without touching a call site: `.width = 240.0f` still means 240
pixels, and only code that wants a share writes `Length::percent(25)`.

A percentage resolves against the container's **content box** along the same
axis, as CSS does. Against a basis that is not known — an intrinsic pass, a
container sizing itself to its children — it behaves as `auto`, which is also
CSS's rule and the only answer that cannot loop.

Equality is the only operator it carries. Arithmetic on a length that might be a
percentage is a question with no answer until it is resolved, and a type that
quietly behaved like a float would hide exactly that.

## Colour

```cpp
struct Color { std::uint8_t r, g, b; float a = 1.0f; };

std::optional<Color> parseColor(std::string_view text);
```

Accepts `#RGB`, `#RRGGBB`, `#RRGGBBAA` and the `"30 30 30"` triplet form that
CSS custom properties use. Malformed input returns nothing rather than black, so
a broken theme is reported to its author instead of silently painted.

| Call | Answers |
| --- | --- |
| `color.withAlpha(a)` | the same colour at another opacity |
| `color.hex()` | `"#rrggbb"`, for backends that speak CSS |
| `color.luminance()` | relative luminance, per WCAG |
| `Color::contrast(a, b)` | the WCAG ratio, 1.0 to 21.0 |
| `color.readableOn(light, dark)` | whichever of the two is legible on it |

```cpp
struct Hsv {
    float hue, saturation, value, alpha;
    Color toColor() const;
    static Hsv fromColor(Color);
};
```

The space a colour is *edited* in. It is its own type rather than a conversion
on every frame because the round trip through RGB is lossy exactly where a
picker is used: at zero saturation every hue is the same grey, so dragging into
a corner and back out would lose the hue that was chosen. A picker holds `Hsv`
and produces a `Color`, never the other way round.

## Cursor

```cpp
enum class Cursor {
    Default, Pointer, Text, Hand, Grab, Grabbing,
    ResizeHorizontal, ResizeVertical, ResizeDiagonalUp, ResizeDiagonalDown,
    NotAllowed, Progress, Wait, Crosshair, Help,
};
```

It lives in `core` because `style` names one and `input` reports one, and a
vocabulary type shared by two modules belongs below both. `Default` means "ask
my parent", so a label inside a button shows the button's.

## JSON

```cpp
std::optional<json::Value> json::parse(std::string_view, ParseError* = nullptr);
std::optional<json::Value> json::parseFile(const std::string&, ParseError* = nullptr);
```

A reader, not a writer, sized for theme files. `Value` offers `asBool`,
`asNumber`, `asString`, `asArray`, `asObject` and `find(key)`, each answering
nothing when the type does not match — so a malformed file surfaces as a named
missing field rather than a default.

Trailing content is an error, so a truncated file cannot half-succeed, and
nesting is bounded so a hostile file cannot recurse the parser into the stack
guard.

## Path

```cpp
Path parseSvgPath(std::string_view d, float tolerance = 0.2f);
```

Parses the SVG path grammar — `M L H V C S Q T A Z`, both cases — and flattens
curves to polylines within `tolerance` pixels. Arcs are converted to cubics
using the endpoint parameterisation from the SVG specification, which matters
more than it sounds: Lucide spells most of its rounded corners as `a`, and this
library shipped once without arc support and drew half its icons in pieces.

```cpp
class Path {
    void moveTo(Vec2);
    void lineTo(Vec2);
    void cubicTo(Vec2 c1, Vec2 c2, Vec2 end, float tolerance = 0.2f);
    void close();

    const std::vector<Contour>& contours() const;   // { points, closed }
    Rect bounds() const;
    Path transformed(float scale, Vec2 offset) const;
};
```

An unsupported command ends the parse and returns what was read: a malformed
icon should draw partially rather than take the frame down.
