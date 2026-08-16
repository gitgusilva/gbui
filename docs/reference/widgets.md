# widgets

Every component is a free function taking a `Ui&` and an options struct. See
[Writing a component](/guide/writing-a-component) to add your own.

**One component, one header.** `button` is `gbui/widgets/button.hpp`, `slider`
is `gbui/widgets/slider.hpp`, and so on, so a translation unit includes what it
uses. Four umbrellas gather the groups for callers who want all of it:

| Umbrella | What it pulls in |
| --- | --- |
| `gbui/widgets/elements.hpp` | text, link, label, icon, image, badge, progress, spacing — and the input set: button, checkbox, radio, switch, select, slider, text field, textarea, number field, and the `field` that wraps one |
| `gbui/widgets/containers.hpp` | box, panel, list row, toolbar, scroll view, table, tabs, virtualised list, marquee |
| `gbui/widgets/overlays.hpp` | tooltip, popover, menu, modal |
| `gbui/widgets/components.hpp` | the composed editors — colour, date, time and rich text — and the charts |

**Which group does a thing belong to?** Four questions, asked in this order.
The order is the taxonomy; asking them in any other one produces two axes and no
answer for the things that sit on both.

1. **Is its job the content inside it?** → a **container**. This is why `panel`
   and `toolbar` are containers rather than the composed components they plainly
   are: what they are *for* is arranging their children.
2. **Does it leave the flow and float?** → an **overlay**.
3. **Is it a leaf with a counterpart in HTML?** → an **element**. It draws
   itself and decides nothing beyond the theme it takes its colours from. This
   is why `select` is an element: it opens a popover, but so does a native
   `<select>`, and the question is asked of the control, not of its menu.
4. **Otherwise it is composed and has opinions** → a **component**. `datePicker`
   decided what a month looks like; `colorPicker` decided where the hue rail
   goes.

All four are the same kind of function to the compiler. The groups are for
readers, and for the umbrella a translation unit includes.

::: tip `controls.hpp`
The old fifth umbrella still exists and still compiles — it now includes
`elements.hpp` and `components.hpp`. Prefer the one you mean in new code: it is
shorter, and it says which half you are reaching for.
:::

Interactive components take an [`Interaction`](/reference/input) and an **id**,
report what the user did, and hold no state: the value is yours.

```cpp
if (checkbox(ui, input, "settings.tags", value, {.label = "Show tags"}))
    value = !value;
```

The component never writes to your model. It says what happened and leaves the
decision with you, which is what makes undo, validation and "are you sure?"
possible without the toolkit knowing about any of them.

## Text

```cpp
NodeId text(Ui&, std::string_view value, const TextOptions& = {});
NodeId sectionHeading(Ui&, std::string_view value);
NodeId strong(Ui&, std::string_view, TextOptions = {});     // importance
NodeId emphasis(Ui&, std::string_view, TextOptions = {});   // stress
```

```cpp
struct TextOptions {
    Token        color = Token::Text;
    FontWeight   weight = FontWeight::Regular;
    FontSlant    slant = FontSlant::Normal;
    FontRole     role = FontRole::Ui;
    float        size = kAuto;
    TextAlign    align = TextAlign::Start;
    float        grow = 0.0f;                       // take the rest of the row
    TextOverflow overflow = TextOverflow::Ellipsis;
    int          maxLines = 0;                      // wrapping only; 0 is unlimited
    float        lineHeight = 0.0f;
    bool         underline = false, strikeThrough = false;
    Gradient     gradient{};                        // across the run
};
```

`sectionHeading` is the small, muted, uppercase heading — "UNSTAGED (3)". The
two shorthands carry the meaning HTML gives them rather than the appearance:
`strong` happens to be drawn semibold and `emphasis` italic today, and a call
site that says what it means is one a restyle does not have to visit.

### Mixed runs

```cpp
richText(ui, {{"on branch "}, {"main", Token::Accent, {}, FontWeight::SemiBold},
              {", 3 files changed"}}, {.wrap = true});
```

A node holds one `TextStyle` and therefore one colour, so a line whose runs
differ is a **row of spans** rather than a string with markup in it. Keeping
them as data is what stops this becoming a parser.

`wrap` lets the line break **between** spans — not inside one, which would need
an inline formatting context the engine does not have. Splitting a sentence into
more spans gives the layout more places to break. A paragraph that must break
mid-sentence wants `text` with `TextOverflow::Wrap` instead.

## Button

```cpp
NodeId button(Ui&, std::string_view label, const ButtonOptions& = {});
NodeId button(Ui&, const Interaction&, std::string_view label, const ButtonOptions& = {});

struct ButtonOptions {
    ButtonVariant       variant = ButtonVariant::Secondary;  // Primary Secondary Ghost Danger
    std::optional<Icon> leading;                             // drawn before the label
    bool                disabled = false;
    bool                block = false;                       // fill the row
    float               height = 0.0f;                       // 0 = the design's control height
    std::string_view    id;                                  // tag
    std::optional<bool> ripple;                              // unset asks the Design
};
```

Disabled is drawn, not merely flagged: the button renders at 45% opacity and
stops taking focus.

The `Interaction` overload is what a ripple needs — the ink grows from the
*point* the press landed, which the frame knows and the builder does not. Unset,
`ripple` asks the active `Design`: Material throws ink, the others change the
surface.

## Image

`#include "gbui/widgets/image.hpp"`

```cpp
NodeId image(Ui&, const Bitmap&, const ImageOptions& = {});

struct Bitmap { const std::uint8_t* pixels; int width, height, stride; };
enum class ImageFit { Fill, Contain, Cover, None };
```

HTML's `<img>`, with the parts of it that are a toolkit's business: a box, a
fit, a radius, an opacity and an `alt`. `ImageFit` is CSS's `object-fit` and is
named after it — `Contain` letterboxes, `Cover` crops, and both are cut by the
box, which is why the picture is drawn as a child of it rather than on it.

**The pixels are borrowed for the frame.** They are read when the frame is
painted, not when `image` is called, and nothing copies them — the same contract
a label's `string_view` has, and the same one it breaks the same way. Put them
in a member, a cache or a static; a buffer that dies with the enclosing scope is
a picture of whatever the stack holds by the time anyone looks.

The picture is sampled bilinearly, because a logo scaled into a table row is
almost never at its own size and nearest-neighbour turns its edges into a
staircase. Corners are cut by the same coverage the fills use, so an image in a
rounded box and the box agree about the curve. `SvgPainter` carries it too, as a
base64 PNG written on the way out — an exported document that quietly dropped
the pictures would be the one thing that painter exists not to be.

### Getting the pixels

```cpp
#include "gbui/platform/image.hpp"

class Image {
    static Image fromFile(const std::string& path);
    static Image fromMemory(const std::uint8_t*, std::size_t);
    Bitmap bitmap() const;      // valid while the Image is
    const std::string& error() const;
};
```

PNG, JPEG, BMP, GIF, TGA and PSD, through the vendored `stb_image.h`. It lives
in `platform/` for the reason everything else does: that is where the machine
is, and core, style, scene, layout, paint and widgets stay free of third-party
code. `Image` owns and `Bitmap` borrows, which is the whole reason there are
two — decode once into something you keep, hand a view to the widget each frame.

A failure carries a reason. "The picture did not appear" has a dozen causes and
the decoder is the only thing that knows which one it was.

## Icon and badge

```cpp
NodeId icon(Ui&, Icon which, const IconOptions& = {});   // color, size, stroke
NodeId badge(Ui&, std::string_view value, const BadgeOptions& = {});
```

A badge is a pill, and it never shrinks: a branch name that elides to nothing is
worse than a row that overflows. See [Icons](/guide/icons) for the set and how
to extend it.

## Containers

```cpp
Ui::Scope box(Ui&, const BoxOptions& = {});
Ui::Scope panel(Ui&, const PanelOptions& = {});
Ui::Scope toolbar(Ui&, const ToolbarOptions& = {});
Ui::Scope listRow(Ui&, const ListRowOptions& = {});
NodeId    spacer(Ui&, float grow = 1.0f);
NodeId    divider(Ui&, Direction containerDirection);
```

`box` is the general container, the way `<div>` is one — `ui.scope(Style{…})`
already builds anything the layout engine can express, so this is the
ergonomics. The presets are functions returning options, not a second API:

```cpp
auto card    = box(ui, BoxStyle::card());
auto sidebar = box(ui, BoxStyle::sidebar());
auto bar     = box(ui, BoxStyle::navbar({.height = 44.0f}));
auto body    = box(ui, BoxStyle::section());
auto middle  = box(ui, BoxStyle::centre());
```

`BoxOptions` carries the layout, size, appearance, `cursor`, `id` and
`focusable` fields a container repeats; `panel` stays as the older, narrower
form of `card`.

```cpp
struct ListRowOptions {
    bool             selected = false;
    bool             hovered = false;
    float            height = 28.0f;
    Edges            padding = Edges::symmetric(0.0f, 12.0f);
    float            gap = 6.0f;
    std::string_view id;
};
```

`selected` washes the row in the accent at 18%; `hovered` uses `surfaceHover`.
Both are passed in — components hold no state.

`divider` takes the direction of the container it sits in, because a rule spans
the cross axis and is one pixel on the main one.

::: warning `ToolbarOptions::bottomBorder` draws nothing
`Border` is all four edges, so a toolbar cannot ask for a rule on one of them.
Add `divider(ui, Direction::Column)` after the toolbar until the primitive grows
per-edge widths.
:::

## Scrolling and virtualised lists

`#include "gbui/widgets/scroll.hpp"`, `virtualList.hpp`

```cpp
Ui::Scope scrollArea(Ui&, const Interaction&, id, ScrollState&, const ScrollOptions& = {});
```

The content is laid out at its natural size, clipped to the viewport, and moved
by `state.offset`. The wheel scrolls it while it is the innermost scrollable
node under the pointer; Page Up, Page Down, Home and End do too when focus is on
it *or inside it*; the bar can be dragged. `ScrollState` is the application's:

```cpp
struct ScrollState {
    float offset;         // yours to set — restore it and the view opens where it was
    float contentSize;    // written by the component from last frame's geometry
    float viewportSize;
    float maxOffset() const;
    bool  scrollable() const;
    float progress() const;   // 0 at the top, 1 at the bottom
};
```

`ScrollOptions` carries `direction`, an explicit `axis`
(`None | Vertical | Horizontal`), the wheel `step`, the bar's width and
auto-hiding, `focusable`, and minimums and maximums on the **viewport** — a
maximum is what turns "as tall as its content" into "as tall as its content,
then scroll", which is the rule a dropdown needs. `None` still clips: a box that
must not grow past a size but must not scroll either is a real thing. Both axes
at once is not built, and is named rather than half-done.

Everything inside a scroll view costs a node, so 50 000 rows really do build
50 000 nodes. When the rows are uniform, don't:

```cpp
VirtualSlice shown = virtualList(ui, input, "history", state,
                                 {.count = commits.size(), .rowHeight = 28.0f},
                                 [&](Ui& ui, std::size_t index) {
                                     auto row = listRow(ui, {.id = rowId(index)});
                                     text(ui, commits[index].subject, {.grow = 1.0f});
                                 });
```

`row` is called once per visible index, in order, inside a box of exactly
`rowHeight` — the list enforces the height rather than trusting the caller,
because a row that laid itself out taller would slide the ones after it out of
step with the scrollbar.

The rows that are *not* visible become two spacers, one standing in for
everything above the slice and one for everything below. The content is
therefore the full height it would have been, so the scrollbar, `maxOffset` and
Page Down all keep working on the real list while the arena holds the forty rows
a person can see.

```cpp
struct VirtualSlice { std::size_t first, count, total; float pitch; };
```

Returned rather than kept, so a caller can say "showing 41–78 of 50 000" without
counting anything. `overscan` (2 by default) builds that many rows above and
below the viewport, which is what keeps a fast drag from showing an edge — the
viewport is last frame's, like everything else that needs geometry before layout
has run.

```cpp
struct RowMetrics { float height, gap, top; float pitch() const; };
void revealRow(ScrollState&, const RowMetrics&, std::size_t index);
```

Scrolls the least distance that brings a row fully into view, and does nothing
when it already is: a row one line below the fold moves one line rather than
jumping the list under the reader. This is what arrow-key navigation over a list
needs, virtualised or not — a select's open list uses the same call.
`VirtualListOptions::rows()` hands over the metrics of a virtualised one.

```cpp
void scrollbar(Ui&, const Interaction&, id, const ScrollState&, Rect box,
               ScrollAxis = Vertical, float width = 10.0f, bool autoHide = true);
```

A bar for a view that is not where the bar belongs. Normally `scrollArea` draws
its own and this is not needed; the table is the case that forced it out. `box`
is where the bar should go, in the current container's coordinates, and the ids
are the view's own — so the press, the drag and the paging are still handled by
`scrollArea`. It draws; it does not behave. Turn the view's own bar off with
`ScrollOptions::scrollbar` when you use it, or there will be two.

### Compare

Two things in the same rectangle, with a handle that says how much of each — a
before and an after. Both are drawn at the **full size of the box** and one is
revealed over the other, which is what makes the comparison work: the reader is
looking at the same pixels in the same place rather than remembering one while
they look at the other.

```cpp
const CompareResult result = compare(ui, input, "shot", model.seam,
                                     [&](Ui& ui) { image(ui, original); },
                                     [&](Ui& ui) { image(ui, retouched); },
                                     {.beforeLabel = "Original", .afterLabel = "Retouched"});
if (result.changed) model.seam = result.position;
```

**The seam is clipped by a percentage, not by a measured width**, and that is
the part worth knowing. A clip sized from last frame's geometry would be a frame
late and would jump on every resize; a percentage resolves during layout, so it
is right on the first frame. The content inside that clip is `100 / position`
percent of *it*, which comes back out to the full width of the box — a layout
identity rather than an arithmetic one. The handle is placed the same way, by
two flexible spacers, which also keeps it wholly inside the box at either end.

`slideOnHover` is off, because a comparison is something a reader sets and then
looks at, and a seam that moves whenever the pointer crosses the picture cannot
be left anywhere.

**Accessibility.** It is a slider and genuinely one: a value from 0 to 1, the
arrow keys at 2% and Page at 10% — finer than the colour picker's 5%, because a
seam is aimed at an edge and a colour at a region — plus Home and End. The value
is announced in words, `"60% Retouched"`, since "60 percent" alone says neither
how much of what nor revealing what. Both sides are named and both stay in the
tree whatever the handle is doing, because a reader who cannot see them is not
comparing them by eye and hiding one would leave them with half of it.

### Marquee

```cpp
struct MarqueeState { float offset = 0.0f; };
void marquee(Ui&, const Interaction&, id, MarqueeState&, float delta,
             const std::function<void(Ui&)>& content, const MarqueeOptions& = {});
```

A strip whose contents slide past and come round again — a ticker along the top
of a trading screen, a status band, a row of logos. The content is drawn
**twice**, side by side: one copy leaves a hole behind it as it travels and the
second, exactly a content's width back, fills it, so the seam never arrives at a
moment anyone could see. It is built twice per frame, so it should be a row of
labels rather than a table.

**A position, not a clock**, and that is the difference between a strip that
slides and one that twitches. Derived from a clock the position would be
`fmod(seconds * speed, contentWidth)` — so the instant the content changes
width, which for a ticker is every time a number gains a digit or a sign, the
modulus lands somewhere else and the whole strip jumps sideways. Advanced by the
frame's delta instead, a change in width moves only where the next wrap will be.

`delta` of zero stops it, which is the one interaction a ticker has: held still
while the pointer is over it, a reader can read a name instead of chasing it.
Stopping is the caller's to decide because only the caller knows what should
stop it — `demos/src/markets.cpp` spends one line on it.

A tape's cards should carry **snapshots**. Each card on that screen says what a
name traded at at a moment and is never rewritten; new ones are printed and old
ones fall off the end. A card whose number kept changing under the reader would
be a live cell that happens to be sliding, which is a different thing and a
worse one.

Both passes are out of the flow, which is how they slide and also why the strip
takes its size from `grow` and its height by stretching: there is no content in
the flow to measure either from, and an unsized one collapses to an empty band.

## Table

`#include "gbui/widgets/table.hpp"`

```cpp
TableResult table(Ui&, const Interaction&, id, const std::vector<Column>&,
                  std::size_t rowCount, TableState&,
                  const std::function<void(Ui&, std::size_t row, std::size_t column)>& cell,
                  const TableOptions& = {});
```

What makes a table a table is the part a list of rows cannot do: **the widths
are resolved once for the whole table** and handed to the rows, so every row's
third cell starts at the same x. `cell` is called for each visible cell and
builds whatever belongs there, into a box that is already the right width.

```cpp
struct Column {
    std::string_view title;
    ColumnSize       sizing = ColumnSize::Fraction;   // Fixed | Fraction | FitContent
    float            width = 1.0f;      // pixels, or the share
    std::string_view fitSample;         // the widest value expected, for FitContent
    float            minWidth = 48.0f, maxWidth = kAuto;
    TextAlign        align = TextAlign::Start;
    bool             sortable = false;
    bool             resizable = true;
};
```

`FitContent` measures the header's title and `fitSample`, **not** the cells, and
that is a real limit rather than an oversight: a cell is a callback building
arbitrary UI, so asking it how wide it would like to be means building the whole
table twice. For a commit hash, a date or a count — the columns that actually
want fitting — a sample is exact.

`sortable` is off by default and deliberately: this widget owns the geometry,
not the data. It reports that the reader asked for a different order and the
application reorders its rows. A column that advertises sorting it does not
implement is worse than one that never offered.

```cpp
struct TableState {
    ScrollState body, columns;     // vertical, and the shared horizontal one
    std::vector<float> widths;     // what the reader dragged; kAuto keeps the rule
    int sortColumn = -1; bool ascending = true;
    std::size_t selected = npos;
};
```

Dragged widths live in the state rather than in `Column` so the layout rules a
developer wrote and the widths a reader chose stay separate — shipping a new
column order should not throw away either. The header and the rows share one
horizontal scroll so they cannot drift apart.

`TableOptions` carries row and header heights, `stickyHeader`, `zebra`,
`virtualise` (on, and reusing `virtualList`), cell padding and the three kinds
of rule: `gridLines` under the header, `rowLines` and `columnLines`. Both row
and column lines are off by default — on a table read top to bottom the rows are
already separated by their alignment, and a line under each is a lot of ink for
no information.

`TableResult` reports `sortChanged`, `selectionChanged`, the `shown` slice and
the resolved `columnWidths`.

**Both scrollbars belong to the table, not to what scrolls.** The rows scroll
up and down *inside* a box that scrolls side to side, so the row view's own
right-hand edge is out at the end of the widest column: a bar drawn against it
is one the reader has to scroll sideways to find, and once the columns are wide
enough, one they cannot reach at all. The rows' bar is turned off and drawn
against the table's own box instead — which is where a browser puts it — through
`scrollbar()` below.

A header does not light up on hover. A row does, because the reader is picking
one *out* of many; a column title is not picked out of anything, and the two
things it does need to say — that it can be clicked, and which way it would sort
— are already carried by the cursor and by the arrow every sortable column wears
permanently.

## Tabs

```cpp
std::optional<std::size_t> tabs(Ui&, const Interaction&, id,
                                const std::vector<TabItem>&, std::size_t selected,
                                const TabsOptions& = {});
void tabPanels(Ui&, std::size_t selected,
               const std::vector<std::function<void(Ui&)>>& panels,
               const TabPanelsOptions& = {});
```

The strip is **one** focusable stop — the ARIA roving-tabindex pattern — so Tab
moves past the whole strip rather than through every tab in it, and the arrow
keys move between them once it has the keyboard. Home and End jump to the ends
and disabled tabs are skipped. The indicator slides to the new tab when an
animator is present and simply appears when there is not.

`TabsOrientation::Vertical` is a sidebar: same component, same keys, the
indicator down the leading edge. `TabItem::group` draws a heading above the
first tab of a run — vertical strips only, and drawn *between* tabs rather than
being one, so the keyboard walks straight past it.

`tabPanels` is separate because the two are rarely siblings: a vertical strip
sits beside its panels and a horizontal one above them. An unselected panel is
built inside a box of no height that clips, so it takes part in nothing the
reader can see while still being *there* — `lazy` turns that off and skips it
entirely, which is cheaper and means `frameOf` finds nothing inside it.

**Not built:** an overflow menu for when the tabs do not fit (they shrink and
elide), a close affordance, and reordering by drag.

## Elements: the input set

`#include "gbui/widgets/elements.hpp"`

The primitives a form is made of. Each holds no state, takes the value in and
reports what the user did with it. `select` is one of these — it opens a
popover, but so does a native `<select>`, and where a control draws its list is
not what decides whether the control is a primitive.

| Component | Signature | Reports |
| --- | --- | --- |
| `checkbox` | `(ui, input, id, checked, options)` | `true` on the frame it was toggled |
| `radio` | `(ui, input, id, selected, options)` | `true` when chosen; nothing when already selected |
| `toggle` | `(ui, input, id, on, options)` | `true` on the frame it was flipped |
| `textInput` | `(ui, input, id, TextEditState&, options)` | `TextInputResult` — changed, moved, submitted, cancelled, toggledReveal, and `value`/`hasValue` on a number |
| `textarea` | `(ui, input, id, TextareaState&, options)` | `TextEditResult` — `submitted` is the modified Return |
| `field` | `(ui, input, id, options, control)` | `FieldResult` — the id to focus when the caption was clicked |
| `slider` | `(ui, input, id, value, options)` | `{value, changed}`, snapped to `step` |
| `progressBar` | `(ui, options)` | nothing; a negative value draws the indeterminate form |
| `label` | `(ui, input, id, text, options)` | the id to focus when it was clicked, or nothing |
| `hyperlink` | `(ui, input, id, label, options)` | `true` on the frame it was followed |

Every one of them:

- is activated by a click **or** by Space/Return while focused;
- dims to 45% when `disabled`, **and recolours** — opacity alone still reads as
  editable — and stops taking focus;
- takes its size and shape from the active `Design`, so a switch is 40×22 under
  `gitbox()` and 52×32 under `material()` without any call site changing;
- shows its focus ring only when focus arrived from the keyboard — the
  `:focus-visible` rule, described under
  [focus and its ring](/reference/input#focus-and-its-ring-are-two-questions).

### Text and numbers

`textInput` is one box with an `InputType`, the shape `<input>` has: `Text`,
`Password` and `Number`. HTML's `email`, `url`, `tel` and `search` are
deliberately absent, because here they would change nothing — their whole effect
is to pick a soft keyboard and hand the browser a validator, and this toolkit
has neither.

All three support a placeholder, a leading icon, `readOnly`, `disabled` and
`invalid`. `Password` draws bullets, with a reveal eye at the trailing edge that
shows the text while it is held (`revealToggle`, on by default — a box you
cannot read back is a box people mistype into). `textInput` is the exception to
the ring rule above: a box that will swallow the next keystroke rings however
focus arrived.

The pointer places its caret. A press puts the caret on the character boundary
nearest the click, and holding and moving drags a selection out from there —
both measuring the run exactly as the caret is drawn, so what is clicked is
where it lands. A password box maps the click against its bullets and counts
that many characters into the string, because the two are not the same number
of bytes.

**A number is edited as text, and that is the whole design.** The state is a
`TextEditState` like every other input's, because a caret needs a string and a
number being typed is not yet a number: `-`, `1.` and empty are all states a
reader passes through. So while the box has the keyboard the text is the source
of truth and nothing rewrites it underneath the caret; anything that could still
become a number is accepted and anything else is refused outright.

`result.value` is the text read as a number, **clamped even when the text is
not** — typing `500` into a box that stops at `60` leaves `500` on screen and
returns `60` throughout. `result.hasValue` is false for an empty box, which is
how a number input says it has no value. On blur the text is normalised from the
clamped value, which is the one moment the two are allowed to disagree.

The wheel steps the value while the pointer is over the box, and Up and Down
step it while it has the keyboard. Home and End do **not** go to the bounds —
they belong to the caret, as they do in anything else that takes typing. The
steppers go `Sides`, `Stacked` (the classic spin box) or `None`, and drop to
`Stacked` automatically below `stackedBelow` pixels wide, so a box in a narrow
column shows its value rather than two buttons.

### Label and link

```cpp
if (const auto target = label(ui, input, "f.name", "Repository", {.forId = "name"}))
    interaction.focus(*target);
```

Clicking a label focuses the control it names, exactly as `<label for>` does —
and does not when that control is disabled or read-only. It returns the id
rather than moving focus itself, which keeps the toolkit from mutating
interaction state behind a component's back.

`hyperlink` follows `href` through [`openUrl`](/reference/platform#opening-a-url)
on click or Return. An empty `href` reports the click and follows nothing, which
is what a link that scrolls somewhere inside the application wants. `chord`
requires modifiers for a click to count — for a link inside editable text, where
a plain click has to place the caret instead. It underlines always by default,
because colour alone is not a cue for everyone.

## Pickers

`#include "gbui/widgets/datePicker.hpp"`, `timePicker.hpp`,
`dateTimePicker.hpp`, `colorPicker.hpp`

Each comes in two entry points rather than a flag: an **inline** one that is
always open and owns its space, and a **field** that borrows space from a
popover. They share the state and the drawing.

```cpp
DatePickerResult   datePicker(ui, input, id, selected, DatePickerState&, options);
DateFieldResult    dateField(ui, input, id, selected, DatePickerState&, options);
TimePickerResult   timePicker(ui, input, id, selected, TimePickerState&, options);
DateTimePickerResult dateTimePicker(ui, input, id, selected, DateTimePickerState&, options);
DateTimeFieldResult  dateTimeField(ui, input, id, selected, DateTimeFieldState&, options);
ColorPickerResult  colorPicker(ui, input, id, ColorPickerState&, options);
ColorPickerResult  colorField(ui, input, id, ColorPickerState&, options);
```

`Date` is three integers and `Time` is three more, deliberately not
`std::chrono::year_month_day` — that type does the arithmetic inside, and
putting it in the public surface would push a `<chrono>` include into every call
site. `Date::serial()` and `Time::secondsOfDay()` are how two of them compare
and step.

**The calendar.** Arrows move by a day, Page Up and Page Down by a month, and
days outside the month or outside `minimum`/`maximum` are **drawn and dimmed,
not hidden** — a grid with holes in it is harder to read than one with days you
cannot pick. Today is marked with a dot rather than a ring, so it cannot be
confused with the selection. The state carries what is being *looked at*, which
is not the same as what is chosen — the same split a select makes between its
highlight and its value.

**The clock.** Columns of hours and minutes rather than steppers, because a time
is picked far more often than it is nudged: "quarter past two" is two glances,
and two number fields make it two edits. Each column scrolls its selection into
view, so opening on 23:45 does not show midnight. `minuteStep` coarsens it, and
bounds that wrap midnight are handled as two ranges.

**Twelve-hour is a display, not a value.** `Time::hour` is always 0–23, so
switching a picker to AM/PM keeps the same instant.

**Formatting is a pattern**, in the grammar every date library uses:

```cpp
formatDate(date, "dd/MM/yyyy", locale);
formatTime(time, "HH'h'mm", clock);
formatDateTime(when, "EEE, d MMM 'at' h:mm a", calendar, clock);
```

`yyyy yy MMMM MMM MM M dd d EEEE EEE` for dates, `HH H hh h mm m ss s a` for
times; anything else is copied through, and text in single quotes is verbatim —
which is how a pattern says "de" in Portuguese without the `d` being read as a
day. The pattern decides the **shape** and `CalendarLocale`/`ClockLocale` decide
the **words**, which is how one function covers Brazilian, American and ISO
without a locale database.

**The colour picker** is a saturation/value square, a hue rail, an alpha rail
over a chequerboard, a hex readout and optional swatches, each switchable off on
its own. The square is built the way every picker on the web is: the pure hue
behind, white-to-transparent across it and transparent-to-black down it — two
gradients over a fill, three nodes rather than a shader. Its state is `Hsv`, not
`Color`, for the reason on the [core](/reference/core#colour) page.

**Still missing:** a text field that accepts a *typed* date, and relative
phrasing ("3 days ago"), which needs its own words from the locale.

## Rich editor

`#include "gbui/widgets/richEditor.hpp"`

```cpp
RichEditorResult richEditor(Ui&, const Interaction&, id, RichDocument&,
                            RichEditorState&, const RichEditorOptions& = {});
```

A block editor: paragraphs, headings, lists, quotes and code blocks, with bold,
italic, underline, strikethrough, inline code and links over ranges within a
block. Typing, splitting a block with Return and merging with Backspace all
work, and the toolbar is the caller's to compose — `defaultToolbar()` is a
starting point, and a `Custom` item runs a callback given the document and the
state.

Marks are **ranges over the text**, not a tree of nested spans, which is the
model every editor that survived contact with users ended up at — Quill's
deltas, ProseMirror's marks. Nesting looks natural until a bold run and a link
overlap by half, and then it is two trees that cannot both be right.

The `BlockStyle` toolbar item is a dropdown rather than one button per heading,
because a block is a heading *or* a paragraph and a row of mutually exclusive
buttons is a select wearing the wrong clothes.

**Not finished, and the gaps are named rather than discovered:** no images (the
painter cannot decode one), no undo, no nested lists, and the caret moves by
character and by block rather than by *visual line* — so a block that wraps is
edited by a caret that does not know where the lines are.

## Overlays

`#include "gbui/widgets/overlays.hpp"`

All of them sit in a layer above the content, are positioned by the
[placement engine](/reference/overlay) rather than by the flex flow, and are
anchored **by tag**: they ask the interaction layer where the anchor was last
frame, which is the rectangle the user was pointing at and the only geometry
available while the tree is being built.

**The application owns whether they are open.** A component that decided that
for itself would need to keep state, and a toolkit that keeps state cannot
rebuild its tree.

```cpp
void        tooltip(Ui&, const Interaction&, anchorId, std::string_view text, options);
Ui::Scope   popover(Ui&, const Interaction&, id, anchorId, options);
bool        menuItem(Ui&, const Interaction&, id, std::string_view label, options);
void        menuSeparator(Ui&);
Modal       modal(Ui&, const Interaction&, id, title, Vec2 position, options);
Ui::Scope   modalActions(Ui&);
ToastResult toast(Ui&, const Interaction&, ToastState&, float delta, options);
```

The first five take a `FloatingOptions` — `placement`, `gap`, `margin`, `flip`,
`shift`, `bounds` — as the base of their own options struct. `toast` does not:
it is anchored to an edge rather than to a control, and the one case where it
*is* anchored to a control says so with `ToastPlacement::Anchored`.

`tooltip` draws nothing when its anchor is not hovered, so the call sits
unconditionally beside the control it describes. `delay` (0.4 s) is what stops a
pointer dragged across a toolbar from flashing one per control; it needs an
animator for the clock, and without one the tooltip shows at once.

`popover` bounds its own height: `maxHeight = kAuto` does **not** mean
unbounded, it means the room actually available on the side it lands on, less
the margin — so a popup that would run past the bottom of the window stops at it
and scrolls inside instead. Pass a `ScrollState*` for that scrolling; null means
it clips without moving. `matchAnchorWidth` is what a select's list wants.

### Toast

Short-lived messages, stacked in a corner and gone on their own. The queue is
`ToastState`, owned by the application like every other piece of state here —
and it matters more than usual, because toasts are raised from *anywhere*: a
network reply, a file watcher, a shortcut three screens away. A component that
owned them would be a component with a global.

```cpp
state.toasts.push({.kind = ToastKind::Error, .message = "Could not reach origin."});
…
toast(ui, input, model.toasts, delta);          // once, near the end of the frame
```

**The id is the whole of the grouping.** `push` treats two entries with the same
id as one and bumps a count instead of stacking a second copy, and an empty id
is derived from the kind, the title and the message — so a retry loop reports
"still offline ×40" rather than forty copies of one sentence. Set an id
explicitly where messages that read alike are genuinely different events.

**Where it goes.** Six corners and edges, or `ToastPlacement::Anchored` against
a tagged node using the same placement engine a popover uses. `bounds` says
which rectangle the corners are measured from, so a stack can live inside a
panel rather than over the window. Which way the stack *grows* is never a
decision the caller makes — away from the edge it is anchored to, so a top-left
stack reads downwards and a bottom-right one upwards.

A bottom stack does not measure itself to find its own bottom: the container is
the whole column and `justify` puts the toasts at the end of it, which is
correct on the first frame where arithmetic on last frame's height would not be.

**`group` is a second axis, and a different one.** It routes an entry to an
outlet: a `toast()` call carrying a group shows only the entries in it, which is
how a dialog reports into itself while the application's messages go to the
corner.

**The timer stops while it is being read.** A message that vanishes mid-sentence
was not delivered either, so the stack pauses while the pointer is over a toast
or the keyboard is inside it — Toastify's behaviour, and what WCAG's "enough
time" rule asks for. `duration = 0` never expires, which is the right answer for
anything the reader has to act on. Only what is *on screen* ages: an entry
waiting behind `maxVisible` has not been read, so its clock has not started.

`progress` draws a bar draining across the foot, and only where there is a time
to show — a sticky toast gets none, because a full bar under it would say the
opposite of what is true. It dims while paused, which is the only way the pause
is visible.

**Accessibility.** Each toast is its own live region: `Status` for info and
success, which waits for a pause, and `Alert` for warning and error, which
interrupts — because the next thing the reader was about to do will not work.
The stack never takes the keyboard; only the × and the action are Tab stops, and
each × is named after its message, since four buttons called "Dismiss" are four
buttons nobody can tell apart. The progress bar carries no record at all: it is
the timer, the timer already pauses whenever a reader is near it, and announcing
it would be a second message nobody asked for.

`menuItem`'s check mark goes on the leading edge for a menu — where every
desktop menu reserves a gutter for state — and on the trailing edge for a
select, where the leading edge belongs to the labels being compared. It is
focusable by default, which is right for a menu the keyboard should walk and
wrong for a list whose owner drives the highlight.

`modal` takes the position and gives it back, so dragging survives the tree
being rebuilt; pass an empty position on the first frame to have it centred.
`dismissed` covers the close button, the backdrop and Escape.

**Not built:** toasts, and closing a menu on the next click outside — the
application does that today.

## Select

`#include "gbui/widgets/select.hpp"`

```cpp
struct SelectState {
    bool open = false;
    std::optional<std::size_t> highlighted;   // where the keyboard is, not the value
    ScrollState list;                         // written by the component
};

SelectResult select(ui, input, id, items, selected, SelectState&, options);   // {chosen}
```

The state is the application's, like every other piece the toolkit reads. The
important field is `highlighted`: **walking a list is not choosing from it.**

| | Closed | Open |
| --- | --- | --- |
| `Up` / `Down` | step the value | move the highlight, wrapping at both ends |
| `Home` / `End` | — | first / last row |
| `Return`, `Space` | open the list | commit the highlight, close |
| `Escape` | — | close, value untouched |

Opening puts the highlight on the current value, so a list opened to look at and
closed with Return changes nothing. The list keeps the highlighted row in view
as it moves, scrolls past `maxVisible` (or `maxListHeight`, which wins when both
apply — a row count cannot know how tall the window is), and **the box keeps the
keyboard**: its rows are drawn and clickable but are not places Tab can land, so
Tab leaves the control rather than walking into an open popup.

The filterable form is not built.

## Charts

`lineChart`, `barChart`, `scatterChart`, `heatmap`, `candlestickChart` and
`donutChart` have a page of their own: [charts](/reference/charts).

## Icons

```cpp
enum class Icon { Archive, Bold, ChartPie, Check, ChevronDown, …, X, Count };

std::string_view iconPath(Icon);
std::optional<Icon> iconFromName("git-branch");
```

Forty Lucide glyphs, generated by `tools/generate_icons.py`; do not edit the
table by hand. See [Icons](/guide/icons).

## Not built yet

Named rather than half-built: a **tree view** (the branch sidebar), a **split
pane** with draggable dividers, a **breadcrumb**, a **toast**, a **marquee**, an
**empty state**, an **avatar**, and a **code and diff view** with syntax
highlighting — that last one is a project of its own and is the honest reason a
full migration off a web stack is a year rather than a quarter.
