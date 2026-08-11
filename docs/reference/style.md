# style

`#include "gbui/style/style.hpp"`, `theme.hpp`, `design.hpp`

## Style

Everything a node asks for, before layout decides what it gets. It is an
aggregate, so designated initialisers work — **in declaration order**.

```cpp
struct Style {
    // flex container
    Direction    direction = Direction::Row;   // Row | Column
    Justify      justify   = Justify::Start;   // Start Center End SpaceBetween SpaceAround SpaceEvenly
    Align        align     = Align::Stretch;   // Start Center End Stretch Baseline
    float        gap       = 0.0f;
    bool         wrap      = false;            // flex-wrap, off by default
    AlignContent alignContent = AlignContent::Start;
    float        crossGap  = kAuto;            // auto follows `gap`

    // flex item
    float  grow = 0.0f;
    float  shrink = 1.0f;
    Length basis = kAuto;
    std::optional<Align> alignSelf;

    // box — each of these is a Length, so a number is pixels and
    // Length::percent(25) is a share of the container's content box
    Length width = kAuto, height = kAuto;
    Length minWidth = kAuto, minHeight = kAuto;      // auto = the content minimum
    Length maxWidth = infinity, maxHeight = infinity;
    Edges  padding, margin;

    // position
    Position position = Position::Relative;    // Relative | Absolute | Fixed
    Layer    layer    = Layer::Content;        // Content | Overlay | Modal
    int      zIndex   = 0;                     // order among siblings
    float    left = kAuto, top = kAuto;        // only read when Absolute or Fixed

    // paint
    Fill     background;
    Gradient backgroundGradient;               // wins when it has two or more stops
    Border   border;
    Outline  outline;
    float    radius = kAuto;                   // auto = the theme's radius
    Overflow overflow = Overflow::Visible;     // Visible | Hidden | Scroll | ScrollX
    float    opacity = 1.0f;
    Cursor   cursorHint = Cursor::Default;
};
```

`minWidth`/`minHeight` default to auto, which resolves to the content minimum
exactly as `min-width: auto` does in CSS. Write `0` to allow an item to shrink
away entirely.

### Overflow

`Scroll` and `ScrollX` clip exactly as `Hidden` does. The difference is not
visual: they declare that this node **consumes the wheel**, which is what lets
`Interaction::wheelTarget()` route a scroll to the innermost thing under the
pointer rather than to everything under it at once. A page that scrolls,
containing a list that scrolls, containing a chart that zooms, is three nodes
competing for one wheel event.

They are two values rather than one plus a flag because the pointer has to
resolve *two* targets — the innermost thing that scrolls up and down, and the
innermost that scrolls side to side — and they are rarely the same node. A table
is exactly that.

### Position, layer, z-index

`Relative` is the flex flow. The other two leave it, and the difference between
them is what they are measured from:

| | Measured from | For |
| --- | --- | --- |
| `Absolute` | the **parent's content box** | a caret in its field, a scrolled pane in its viewport, a slider's knob on its track |
| `Fixed` | the **window** | a menu, a tooltip, a modal — anything that has to escape whatever built it |

The distinction matters more than it looks. A node anchored to the window has
to be told where its container ended up, and the only rectangle a component can
read while the tree is being built is the *last* frame's — so it arrives a frame
late every time that container moves. `Absolute` needs no rectangle at all:
`left` and `top` are already relative to the thing it belongs to.

`Layer` is orthogonal to both, and is inherited: a node inside an overlay is
part of that overlay even when its own style says `Content`, so raising a layer
carries the whole subtree with it. `zIndex` is the *other* question — ordering
among siblings inside one layer, which is what CSS's `z-index` is: a badge over
an avatar, a highlight under a row, a handle over a track. Reaching for a layer
to do that would lift the node out over everything else on screen. Hit testing
follows the same order in reverse, so what looks on top is what the pointer
finds.

## Fill and Gradient

```cpp
Fill{Token::Accent}          // a theme token
Fill{Token::Accent, 0.18f}   // the token at an alpha — how washes are made
Fill{Color{255, 0, 0}}       // a literal, for the rare case the theme should not decide

Gradient::linear(Fill{Token::Accent}, Fill{Token::Accent, 0.0f}, 180.0f);
Gradient::radial(Fill{Token::BgElevated}, Fill{Token::Bg});
```

Stops carry `Fill`s rather than colours, so a gradient re-themes like everything
else. `angle` is in degrees and follows CSS: 0 points up, 90 points right, so
180 is the familiar top-to-bottom.

## Border and Outline

```cpp
struct Border  { float width = 0.0f; Fill color{Token::Border}; };
struct Outline { float width = 0.0f; float offset = 2.0f; Fill color{Token::Accent}; };
```

Separate on purpose, exactly as CSS separates them: a border takes space and
moves the content inside it; an outline is drawn *outside* the border box and
does not — so showing one on focus cannot make the control jump, which is the
single most common way a focus ring gets implemented badly. The default two
pixels at two pixels of offset clears WCAG 2.2's focus-appearance minimum on a
control of any size.

`Border` is all four edges. A toolbar that wants only a rule underneath adds a
`divider` after itself.

## TextStyle

```cpp
struct TextStyle {
    FontRole       role = FontRole::Ui;             // Ui | Mono | Editor
    FontWeight     weight = FontWeight::Regular;    // Thin…Black, as CSS numbers them
    FontSlant      slant = FontSlant::Normal;       // Normal | Italic
    TextAlign      align = TextAlign::Start;        // Start Center End
    float          size = kAuto;                    // auto = the theme's size for the role
    Fill           color{Token::Text};
    Gradient       colorGradient{};                 // painted instead of `color`
    float          lineHeight = 0.0f;               // multiplier; 0 = the painter decides
    TextOverflow   overflow = TextOverflow::Ellipsis;   // Ellipsis | Clip | Wrap
    int            maxLines = 0;                    // wrapping only; 0 is unlimited
    WordBreak      wordBreak = WordBreak::Normal;   // wrapping only
    TextDecoration decoration{};                    // underline, strike, thickness, colour
};
```

### WordBreak

CSS splits this across `word-break` and `overflow-wrap`, which between them have
five values and three that mean roughly the same thing. These are the three that
behave differently:

| | Breaks at | For |
| --- | --- | --- |
| `Normal` | spaces, and after a hyphen or slash; a too-long word is cut | paths and URLs, which wrap after a segment rather than through one |
| `KeepAll` | spaces only; a long word overflows | identifiers a reader must copy whole — half a hash is not a hash |
| `Anywhere` | between any two characters | base64, CJK, anything with no spaces |

Every caller of `wrapText` passes the style's policy — the measurer, the
painter, the layout and the rich editor. They have to agree: a box sized by a
measure that broke anywhere and painted by one that only broke at spaces
overflows the box it was given.

### Faces

Weight and slant are matched against the files on the machine, not merely
recorded. `FontDatabase` scores faces by family first, then by weight and slant
distance, then by how much name the file has left over.

That order is the whole thing. The leftover-name term used to sit above the
weight, and since `LiberationSans-Bold.ttf` is three characters shorter than
`LiberationSans-Regular.ttf`, the bold file scored better as a *regular* face —
so every unstyled label came out bold, and **bold stopped meaning anything**.

Whatever the machine cannot supply is synthesised: a shortfall of 150 or more in
weight thickens the coverage, and a requested italic with no italic face is
sheared about the baseline. Real faces are always preferred, because a designed
bold is not a thickened regular — but a UI that silently draws its headings at
regular weight is worse than one that emboldens.

## Theme

```cpp
static Theme Theme::dark();
static Theme Theme::light();
static Theme Theme::material(bool darkMode = true);
static Theme Theme::cupertino(bool darkMode = true);
static Theme Theme::fluent(bool darkMode = true);
static std::optional<Theme> Theme::fromFile(const std::string&, std::string* error);
static std::optional<Theme> Theme::fromJson(const json::Value&, std::string* error);
```

| Call | Answers |
| --- | --- |
| `theme.color(Token::Bg)` | a token's colour |
| `theme.setColor(token, colour)` | override one |
| `theme.typography()` | fonts, sizes and the corner radius |
| `theme.graphLane(index)` | the commit graph's lane colour, wrapping at eight |
| `theme.onAccent()` | a foreground that stays legible on the accent fill |
| `theme.focusRing(surface)` | a ring colour that stands out from what it is drawn over |
| `theme.id()`, `name()`, `isDark()` | identity |

The three named palettes are the published colours of Material 3, Cupertino and
Fluent mapped onto these tokens. They are approximations by necessity — those
systems have a hundred-odd roles and this has twenty-four — and they exist to
compare the same components side by side, not to certify anything.

## Token

The 24 semantic colours, spelled as the theme schema spells them:

```
Bg BgElevated BgOverlay SurfaceHover
Border BorderStrong
TextStrong Text TextMuted
Accent AccentHover AccentFg
Added Removed Modified
Graph1 … Graph8 GraphMarker
```

`tokenName(token)` and `tokenFromName("surfaceHover")` convert both ways.

## Typography

```cpp
struct Typography {
    std::string uiFont;      float uiFontSize = 13.0f;
    std::string monoFont;
    std::string editorFont;  float editorFontSize = 13.0f;
    float editorLineHeight = 0.0f;    // 0 = let the renderer choose
    float radius = 6.0f;
};
```

## Design

What a design system decides beyond its colours. A theme is 24 colours and a
type scale; Material, Cupertino and Fluent differ far more in **shape and
behaviour** — how round a control is, how tall, whether a press throws ink, how
fast anything moves.

```cpp
ui.setDesign(Design::material());   // or gitbox(), cupertino(), fluent()
```

| Field | Decides |
| --- | --- |
| `controlRadius`, `controlHeight`, `borderWidth` | the shape and size every control shares |
| `press`, `rippleAlpha`, `hoverAlpha` | what a press and a hover look like |
| `switchWidth`, `switchHeight`, `switchKnob`, `switchKnobOn` | the switch, which is what a person recognises a system by |
| `checkboxSize`, `checkboxRadius`, `radioSize` | the small controls |
| `motion` | the `Transition` everything animated uses |
| `chart` | line weight, tick count, axis width, donut thickness, the series palette and the tooltip |
| `outlineFocusRing` | whether focus draws a ring outside the control or recolours it |

`Typography::radius` stays the authority for panels and cards; `controlRadius`
is the one controls use, because the two are not the same number in every system
— Cupertino rounds a button far more than it rounds a pane.

The design is handed to the **builder** rather than read at paint time, because
these decisions change the tree: a control's height and radius are laid out, and
whether a press throws ink decides whether a node exists at all.
