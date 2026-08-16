# Layout

The layout engine implements the part of CSS flexbox that application UIs use.
If you have written `display: flex` before, this will hold no surprises; the
vocabulary is deliberately identical.

## Direction, justify, align

```cpp
ui.row({.justify = Justify::SpaceBetween, .align = Align::Center});
```

- `Direction::Row` lays children out along x, `Column` along y. The chosen axis
  is the **main** axis; the other is the **cross** axis.
- `Justify` distributes free space along the main axis: `Start`, `Center`,
  `End`, `SpaceBetween`, `SpaceAround`, `SpaceEvenly`.
- `Align` places children on the cross axis: `Start`, `Center`, `End`,
  `Stretch` (the default, filling the container). `alignSelf` overrides it for
  one child.

## Sizing, in one paragraph

A child's main-axis size comes from the first of these that is set: `basis`,
then `width`/`height`, then its own content. Then `grow` shares out what is
left over and `shrink` takes back what is missing, weighted by base size the way
CSS does it — in the loop the specification describes, so space refused by an
item that hit a maximum is handed to the items still able to move rather than
lost. On the cross axis, `Stretch` fills and anything else uses the content size.

The idiom for "take the rest of the row":

```cpp
text(ui, commit.subject, {.grow = 1.0f});   // starts from zero, takes the rest
```

`grow` with a `basis` of `0` is what the `text` component does for you. Starting
from the natural width instead would push the siblings out of the row before
any space was shared.

## Pixels or percentages

Every size is a `Length`, which is either an absolute number or a share:

```cpp
ui.scope({.width = 240.0f});                                    // 240 pixels
ui.scope({.width = Length::percent(25), .maxWidth = 320.0f});   // a quarter, up to 320
```

`Length` converts implicitly from `float`, so a plain number still means pixels
and only code that wants a share says so. A percentage resolves against the
container's **content box** along the same axis, as CSS does; against a basis
that is not known — an intrinsic pass, a container sizing itself to its
children — it behaves as `auto`, which is CSS's rule and the only answer that
cannot loop.

Percentages apply to `width`, `height`, the two minimums, the two maximums and
`basis`. Padding, margin, gap and `left`/`top` are absolute.

## Automatic minimum size

By default an item **will not shrink below its content**, exactly as
`min-width: auto` behaves in CSS. That is what stops a crowded toolbar from
squeezing its buttons into slivers.

```cpp
ui.add({.shrink = 1.0f, .width = 300.0f});                 // floor: its content
ui.add({.shrink = 1.0f, .width = 300.0f, .minWidth = 0});  // may vanish entirely
```

Two things answer zero, following the same rule CSS uses, because both have a
defined way to survive being too small:

- a node that clips its overflow;
- a run of text with `TextOverflow::Ellipsis`, which elides instead.

## Wrapping

```cpp
ui.row({.gap = 6.0f, .wrap = true, .alignContent = AlignContent::Start,
             .crossGap = 8.0f});
```

Off by default, and deliberately: a toolbar that silently becomes two rows when
it runs out of room is usually a bug, and everything built before wrapping
existed expects to overflow or shrink instead. Turning it on is what makes a row
of tags, chips or theme cards reflow rather than crush.

Each line resolves its own flexible lengths, so an item only ever competes for
space with the ones beside it. `alignContent` distributes the lines and
`crossGap` separates them — unset, it follows `gap`. An item wider than a whole
line still gets a line of its own, which is the spec's rule and what stops an
over-wide child producing an empty line and a loop.

The part worth knowing when a wrapping container comes out the wrong height: a
wrapping row is as tall as its lines *stacked*, not as its tallest child.

## Text that does not fit

```cpp
text(ui, path, {.grow = 1.0f});                               // elides with …
text(ui, path, {.grow = 1.0f, .overflow = TextOverflow::Clip});
text(ui, body, {.overflow = TextOverflow::Wrap, .maxLines = 3});
```

`Ellipsis` is the default because nearly every string in an application is
longer than its column. Elision happens at paint time, using the same measurer
layout was given — which is why `record` takes it as an argument.

`Wrap` breaks the run into the lines that fit, greedily, with hard breaks on
newlines and `maxLines` clamping and ellipsising the last line the way CSS's
`line-clamp` does. Where a line may end is `TextStyle::wordBreak`: `Normal`
breaks at spaces and after a hyphen or slash — which is what makes a path wrap
after a segment rather than through one — `KeepAll` lets a long identifier
overflow rather than cutting a hash in half, and `Anywhere` breaks between any
two characters.

Layout and the painter call the *same* `wrapText`, which is not an
implementation detail: a box sized by one breaking rule and painted by another
overflows the box it was given.

## Out of the flow

```cpp
ui.scope({.position = Position::Absolute, .left = 12.0f, .top = 4.0f});
ui.scope({.position = Position::Fixed, .layer = Layer::Overlay, …});
```

`Absolute` is measured from the **parent's content box** — a caret in its field,
a scrolled pane in its viewport, a slider's knob on its track — and needs no
geometry from anywhere else. `Fixed` is measured from the **window**, which is
what a menu, a tooltip or a modal needs in order to escape whatever built it,
and it costs a frame of lag the first time it opens because the anchor's
rectangle only exists after a layout.

`Layer` is a separate question and is inherited: `Content`, then `Overlay`, then
`Modal`, each drawn after the one before it, so a popup opened deep in a sidebar
is not painted under the pane beside it. Ordering *within* a layer, among
siblings, is `zIndex`.

## Logical pixels

Layout, hit testing and the input events all work in logical pixels. The display
scale is applied once, in `DisplayList::setScale`, on the way to the backend —
so the same tree at 1, 1.5 and 2 produces the same drawing at three resolutions,
and no component has to know which one it is on.

## One place this differs from CSS, and it will surprise you

**Free space is computed from the *hypothetical* main sizes, not from the flex
base sizes.** An item's minimum is therefore taken out of the container *before*
the `grow` ratios are applied, where CSS §9.7 distributes first and clamps
afterwards.

The difference only shows when an item has both a `grow` and a minimum on the
same axis, and then it is large. Two children with `basis = 0`, `grow` of 1 and
3, and `minWidth = 120` each, in a 600-pixel row:

| | leading | trailing |
| --- | --- | --- |
| CSS | 148 | 445 |
| here | 208 | 385 |

With bigger minimums the ratio stops meaning much at all. So **do not use `grow`
to express a proportion of the container when the same item has a minimum** —
use a percentage `basis` with `shrink`, which is exact:

```cpp
pane.basis = Length::percent(share * 100.0f);   // a share of the container
pane.grow = 0.0f;
pane.shrink = 1.0f;
pane.minWidth = floorSize;                      // still a floor, still honoured
```

`splitPane` is built exactly that way and its header says why. Whether the
engine should be brought in line with the spec is an open question rather than a
plan: it sits under every component in the tree, so changing it is its own
change with its own tests.

## What is not implemented

Named rather than half-built:

- `Align::Baseline` behaves as `Align::Start`. Runs of different sizes on one
  line are centred against each other instead of sitting on a shared baseline.
- No aspect-ratio.
- No `right`/`bottom` for out-of-flow boxes: a box is placed by its top-left
  corner.
- Percentages on padding, margin, gap and `left`/`top`.
- No container queries, so a component cannot yet rebuild itself at a
  breakpoint of its own width. Reading last frame's width through
  `Interaction::frameOf` is what components do in the meantime.
- No inline formatting context: `richText` wraps *between* spans, not through
  them, so a mixed-style paragraph cannot break mid-sentence inside one span.

## Debugging a layout

Print the tree. Layout is arithmetic, so the answer is always visible:

```cpp
arena.forEach(ui.root(), [](NodeId, const Node& n, int depth) {
    std::printf("%*s %.0fx%.0f at %.0f,%.0f '%.*s'\n", depth * 2, "",
                n.frame.width, n.frame.height, n.frame.x, n.frame.y,
                static_cast<int>(n.text.size()), n.text.data());
});
```

Both layout bugs found in this library's first hour were found this way: a box
collapsing to zero, and a frame at y = 5.8e25 from a dangling reference.
