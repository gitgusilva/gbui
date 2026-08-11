# layout

`#include "gbui/layout/layout.hpp"`, `textWrap.hpp`

## Laying out a tree

```cpp
LayoutContext context;
context.theme = &theme;                // required: typography and tokens
context.measure = measureWith(fonts);  // optional: defaults to an estimate

layout(arena, root, Rect{0, 0, width, height}, context);
```

Writes an absolute `frame` and `content` box onto every node. It runs two passes
— intrinsic sizes, then placement — allocates nothing beyond one reusable
scratch vector, and is pure: same tree, same viewport, same frames.

Coordinates are logical pixels. The display scale is applied later, once, in
`DisplayList::setScale`.

## Measurement

```cpp
struct TextMetrics {
    float width, height, baseline;   // baseline measured from the top of the line box
};

using MeasureText = std::function<TextMetrics(std::string_view, const TextStyle&,
                                              const Typography&, float maxWidth)>;

TextMetrics approximateTextMetrics(std::string_view, const TextStyle&,
                                   const Typography&, float maxWidth);
```

The default estimates: 0.52 em per glyph for proportional text, 0.60 em for
monospace. It is deterministic — good enough to lay out a preview and stable
enough to assert on — but a real font engine should replace it through
`context.measure`. The [platform](/reference/platform) module supplies one with
`measureWith(fonts, scale)`.

`maxWidth` is infinity when the caller wants the intrinsic width, and a real
width when it is asking how tall a wrapped run will be.

::: warning Pass the same measurer to `record`
Recording decides where a run is elided and which characters are on which line.
Measuring it differently from the pass that sized its box means text is cut
while it still fits, or drawn on lines the box was never sized for.
:::

## Line breaking

```cpp
WrappedText wrapText(std::string_view text, float maxWidth, int maxLines,
                     const MeasureLineWidth& width, WordBreak = WordBreak::Normal);

struct TextLine   { std::string_view text; float width; };
struct WrappedText { std::vector<TextLine> lines; float widest; bool truncated; };
```

Greedy, which is the algorithm every UI toolkit uses and the one CSS describes:
Knuth–Plass minimises raggedness across a paragraph and is what a typesetter
wants, but a resizing window is not a typesetter, and reflowing every line when
the last word changes is the wrong trade.

The rules, in the order they apply:

- a newline always breaks, and an empty paragraph still produces a line, so a
  blank line in a commit message keeps its height;
- lines break at spaces, and the spaces at a break are dropped rather than drawn
  at the end of a line;
- a single word too wide for the line is broken between characters on a UTF-8
  boundary — `overflow-wrap: break-word`, the right default for a toolkit whose
  containers clip, and free in the normal case because `min-width: auto` already
  reserves the longest word's width.

Each line is a `string_view` into the original, so wrapping allocates the vector
of lines and not one character. `truncated` says `maxLines` cut the run short
and the painter should end that line with an ellipsis.

It lives beside the layout engine because both sides need the *same* answer:
layout asks how tall a paragraph is, painting asks which characters are on line
three, and a second implementation in the painter is how text ends up drawn on
lines its box was never sized for.

## Intrinsic size

```cpp
float intrinsicMainSize(const Arena&, NodeId, const LayoutContext&);
```

The natural size of a subtree along the axis it lays its children out on,
ignoring grow and shrink. A scroll container needs this to know how far its
content reaches before deciding whether a scrollbar is warranted.

## Hit testing

```cpp
NodeId hitTest(const Arena&, NodeId root, Vec2 point);
```

The deepest node whose frame contains the point, or an invalid id. It respects
clipping, so a child scrolled out of its parent is not hit, and it walks
siblings in reverse `zIndex` order, so what looks on top is what is found.

It answers with the deepest node, which is usually a label rather than the row
you care about. Walk up to the nearest tagged ancestor — which is what
`Interaction` does for you:

```cpp
NodeId node = hitTest(arena, root, mouse);
while (node.valid() && arena[node].id.empty()) node = arena[node].parent;
```

## The model

Flexbox. A child's main size comes from `basis`, then `width`/`height`, then its
content; `grow` and `shrink` then resolve the difference in the loop the CSS
specification describes — distribute, freeze whatever hit a bound, hand what
those refused to the items still able to move, repeat. A row with one
`maxWidth` item therefore still fills its container exactly.

With `wrap` on, items are broken into lines first and each line resolves its own
flexible lengths, so an item only competes with the ones beside it. A wrapping
container's intrinsic cross size is its lines *stacked*, not its tallest child.

Children out of the flow are placed before the rest and take no part in the
distribution: `Absolute` against the parent's content box, `Fixed` against the
window. See [Position](/reference/style#position-layer-z-index).

**Not implemented:** baseline alignment (`Align::Baseline` behaves as `Start`),
aspect ratios, `right`/`bottom` for out-of-flow boxes, percentages on padding,
margin, gap and `left`/`top`, and container queries. See
[Layout → What is not implemented](/guide/layout#what-is-not-implemented).
