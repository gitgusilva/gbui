# overlay

`#include "gbui/overlay/placement.hpp"`

Where a floating thing goes. A tooltip, a dropdown, a popover and a context menu
all ask the same question — given an anchor, a size and the window, where does
this sit so it is next to what it belongs to and still on screen? — so the
answer is one pure function rather than a copy inside each of them. It is tested
without a window.

## The model

The one Floating UI and Popper settled on, because it is the one that behaves
the way people expect:

1. try the preferred side;
2. **flip** to the opposite side if it does not fit;
3. **shift** along the other axis to stay inside the window;
4. keep a margin from the edges so nothing touches them.

## Placement

```cpp
enum class Placement {
    Auto,                              // pick the side with the most room
    Top, TopStart, TopEnd,
    Bottom, BottomStart, BottomEnd,
    Left, LeftStart, LeftEnd,
    Right, RightStart, RightEnd,
};

std::optional<Placement> placementFromName("bottom-start");
std::string_view placementName(Placement);
Placement sideOf(Placement);          // the side, with alignment stripped
```

Side, then alignment along that side. `BottomStart` means below the anchor with
the left edges aligned — the usual place for a dropdown.

## place

```cpp
struct PlacementOptions {
    Placement preferred = Placement::Auto;
    float gap = 6.0f;       // between the anchor and the box
    float margin = 8.0f;    // how close to the window edge it may come
    bool flip = true;
    bool shift = true;
};

struct PlacementResult {
    Rect rect;
    Placement placement;    // Auto resolved and any flip applied
    bool flipped;
};

PlacementResult place(const Rect& anchor, Vec2 size, const Rect& bounds,
                      const PlacementOptions& = {});
```

`bounds` is normally the window — `Interaction::viewport()` is it. Everything is
in the same coordinate space and the result is absolute, ready for
`Style::position = Position::Fixed`.

`placement` comes back resolved because an arrow or a tail has to know which
side it ended up on.

## Using it through a component

Every floating component here takes a `FloatingOptions`, which is these fields
plus the bounds, and finds its own anchor rectangle through
`Interaction::frameOf`:

```cpp
struct FloatingOptions {
    Placement placement = Placement::Auto;
    float gap = 6.0f;
    float margin = 8.0f;
    bool flip = true;
    bool shift = true;
    Rect bounds{};      // empty means the viewport the last layout used
};
```

That is why a popup places itself a frame late the first time it opens: the
anchor's rectangle only exists once a layout has run. See
[tooltip, popover, menu and modal](/reference/widgets#overlays).
