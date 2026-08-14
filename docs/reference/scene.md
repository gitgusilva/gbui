# scene

`#include "gbui/scene/tree.hpp"`, `ui.hpp`

## Arena

Owns every node. See [Memory](/guide/memory) for why it is built this way.

```cpp
Arena arena;
arena.reserve(512);                     // one allocation for a whole frame

NodeId id = arena.create(style);
arena.addChild(parent, child);          // O(1): the parent keeps a tail pointer
Node& node = arena[id];

std::string_view text = arena.intern("copied into the arena");
std::uint32_t first = arena.addShapes(std::move(shapes));   // vector art
arena.reset();                          // O(1); keeps capacity for the next frame

arena.forEach(root, [](NodeId id, const Node& node, int depth) { … });
std::size_t bytes = arena.bytesUsed();
```

## NodeId

An index, not a pointer. It survives the arena growing; it does **not** survive
`reset()`. Use a tag for identity that outlives a frame.

```cpp
NodeId id;
if (id.valid()) { … }
```

## Node

```cpp
struct Node {
    Style       style;
    TextStyle   textStyle;
    IconContent icon;              // path data, stroke width, colour
    std::string_view text;         // interned
    std::string_view id;           // the tag

    bool   focusable = false;      // Tab can land here
    bool   ignoresPointer = false; // named for frameOf, invisible to hit testing
    Cursor cursor = Cursor::Default;

    std::uint32_t firstShape = 0, shapeCount = 0;   // a slice of the arena's shapes

    NodeId parent, firstChild, lastChild, nextSibling;

    Rect frame;                    // written by layout, absolute
    Rect content;                  // frame minus padding and border
};
```

Children are an intrusive list, so a container with three children allocates
nothing of its own. Shapes are a **range** into the arena rather than a vector
on the node, for the same reason: clearing the arena stays a size reset.

```cpp
struct Shape {
    Path path;                 // in the node's OWN coordinates
    Fill color{Token::Text};
    float stroke = 0.0f;       // zero fills the contours
};
```

## Ui

The building API. Containers open with a scope guard that closes them on
destruction.

```cpp
Ui ui(arena);

Ui::Scope scope(const Style&);
Ui::Scope row(Style = {});
Ui::Scope column(Style = {});

NodeId add(const Style&);                                   // a leaf
NodeId label(std::string_view, TextStyle = {}, Style = {}); // text
NodeId vector(std::string_view pathData, Style = {},
              Fill = Fill{Token::Text}, float stroke = 2.0f);
NodeId draw(const Style&, std::vector<Shape>);              // a canvas leaf

Ui& tag(std::string_view id);        // names the node just added
Ui& focusable(bool = true);          // …makes it a Tab stop
Ui& cursor(Cursor);                  // …sets the pointer over it
Ui& ignoresPointer(bool = true);     // …names geometry, not a target

NodeId root() const;
NodeId current() const;
NodeId last() const;
```

`Scope` converts to `NodeId`, so a component can return `scope.id()` or the
scope itself.

::: warning A scope closes where it dies
`(void)scope;` at the end of a block silences the unused-variable warning; it is
**not** a close, and anything built after it is still a child. That has caused
four separate layout bugs here. Use braces of its own, or `scope.close()`.
:::

`adopt()` makes one scope close an extra container, for a component that opens a
viewport and a content box but hands back one scope; the **inner** scope adopts
and the outer one is `disown()`ed. The other way round leaves the outer popping
the moment the component returns, which unbalances the stack.

## What the builder is given

Three things the application hands over, all optional, all no-ops when absent:

```cpp
void setMeasure(MeasureText, Typography);   // so components can measure text
void setDesign(Design);                     // shape, sizing, press behaviour
void setAnimator(Animator*);                // somewhere to animate
```

```cpp
TextMetrics measure(std::string_view, const TextStyle&) const;             // intrinsic
TextMetrics measure(std::string_view, const TextStyle&, float maxWidth) const;
bool canMeasure() const;
const Typography& typography() const;
const Design& design() const;
Animator* animator() const;

float animate(id, property, float target, const Transition& = {}) const;
Color animate(id, property, Color target, const Transition& = {}) const;
float pulse(id, property, bool trigger, const Transition& = {}) const;
float latch(id, property, float value, bool set) const;
float now() const;
```

Layout measures too, but it runs after the tree exists. A caret at a byte offset
and a floating box that has to know its own height both need the answer while
they are being built — from the same function layout will use, which is why the
two agree.

With no animator, `animate` returns the target, `pulse` returns 1 and `now`
returns 0. Every component then behaves exactly as it did before motion existed,
which is what makes it opt-in rather than a migration.
