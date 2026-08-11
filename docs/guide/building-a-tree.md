# Building a tree

`Ui` writes nodes into an `Arena`. Containers open with a scope guard, so the
nesting of your code is the nesting of the interface.

```cpp
Arena arena;
Ui ui(arena);
{
    auto column = ui.beginColumn({.gap = 8.0f, .padding = Edges::all(12.0f)});
    text(ui, "UNSTAGED (1)");
    {
        auto row = ui.beginRow({.gap = 6.0f, .align = Align::Center});
        text(ui, "theme.json", {.grow = 1.0f});
        badge(ui, "M");
    }                    // the row closes here
    button(ui, "COMMIT", {.variant = ButtonVariant::Primary, .block = true});
}                        // and the column here
```

The braces are load-bearing. A `Scope` closes its container when it dies, which
also means an early `return` cannot leave one open.

## Style is an aggregate

Every property has a default, so designated initialisers say only what differs:

```cpp
ui.beginRow({.justify = Justify::SpaceBetween, .height = 40.0f});
```

C++ requires designated initialisers in declaration order. If the compiler
complains about "designator order", check the order the fields appear in
[`style.hpp`](/reference/style) — flex properties come before box properties,
which come before paint properties.

`kAuto` is this library's `auto`. It is a NaN, which is what lets `Style` stay a
plain aggregate instead of a pile of `std::optional`:

```cpp
Style style;
style.width = kAuto;          // "decide for me"
if (isAuto(style.width)) { /* … */ }
```

## The kinds of node

```cpp
ui.begin(style);                       // a container; returns a Scope
ui.add(style);                         // a leaf: a spacer, a rule, a swatch
ui.label("text", textStyle, style);    // a leaf with text
ui.vector(pathData, style, fill, 2);   // a leaf with vector content — an icon
ui.draw(style, std::move(shapes));     // a leaf the caller draws into
```

Text is copied into the arena, so passing a temporary is safe. Path data is
*not* copied — it is expected to live as long as the frame, which the generated
icon table does trivially.

`draw` is the canvas: a rectangle plus a list of `Shape`s in the node's **own**
coordinates, where (0,0) is its content box. A chart, a commit graph or a
sparkline is one leaf built this way rather than a widget per mark, and the
translation to window coordinates happens once, when the frame is recorded.

## Naming a node

```cpp
ui.tag("sidebar.branch.main");
ui.tag("settings.autofetch").focusable();      // Tab can land here
ui.tag("history.row.3").cursor(Cursor::Pointer);
ui.tag("field.caret-box").ignoresPointer();    // names geometry, not a target
```

A tag names the node most recently added. Tags are how hit testing reports what
the pointer is over and how a test finds a node without walking the tree by
index. They cost one interned string.

The three modifiers all apply to that same node. `focusable` is what makes a
node a Tab stop — only tagged nodes qualify, because focus has to survive the
tree being rebuilt and a tag is the only identity that does. `cursor` is
inherited, so a label inside a button shows the button's pointer.
`ignoresPointer` says the tag exists to be *found* rather than *hit*: hit
testing walks past it to the tagged ancestor, while `frameOf` still finds it —
which is how a text field can name the box its caret is measured against without
that box swallowing the click meant for the field.

Because the tree is rebuilt every frame, **a tag is the only identity that
survives**. A `NodeId` from last frame means nothing this frame; the tag of the
row under the pointer means exactly what it says.

```cpp
// Last frame's tree still answers this, before it is thrown away.
NodeId node = hitTest(arena, root, mouse);
while (node.valid() && arena[node].id.empty()) node = arena[node].parent;
const std::string hovered(node.valid() ? arena[node].id : "");
```

## What the builder can measure

```cpp
ui.setMeasure(measureWith(fonts, scale), theme.typography());
const TextMetrics metrics = ui.measure(prefix, style);          // intrinsic
const TextMetrics wrapped = ui.measure(body, style, 240.0f);    // in a width
```

Layout measures too, but it runs after the tree exists. A caret at a byte offset,
a tooltip that has to know how tall it will be before it is placed — both need
the answer while they are being built, and both get it from the same function
layout will use, which is why they agree.

An application that forgets `setMeasure` gets a caret at offset zero and a
floating box that corrects its size a frame late. Nothing warns about it.

## Rebuilding versus mutating

Rebuild. The arena makes a frame's worth of nodes a handful of `push_back`
calls, and `arena.reset()` releases the lot in one operation. The panel example
builds 111 nodes and 80 draw commands per frame, out of an arena reserved once
at 256 nodes and reused for the life of the process.

Mutating a retained tree in place means writing invalidation, and invalidation
is where UI toolkits go to die. Rebuild, and let layout decide what moved.
