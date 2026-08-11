# anim

`#include "gbui/anim/animator.hpp"`, `easing.hpp`

The animation clock. The narrative version is [Motion](/guide/motion); this is
the surface.

## Animator

```cpp
class Animator {
    void tick(float deltaSeconds);   // once a frame, before anything is built

    float animate(id, property, float target, const Transition& = {});
    Color animate(id, property, Color target, const Transition& = {});
    float pulse(id, property, bool trigger, const Transition& = {});
    float latch(id, property, float value, bool set);

    float now() const;          // seconds on the clock, for anything that loops
    bool  animating() const;    // true while anything is still moving
    void  clear();              // forget everything — a view being torn down
    std::size_t size() const;
};
```

The application owns it, because it is the memory: the arena is reset every
frame and the animator is what remembers where a value was. It is handed to the
builder with `ui.setAnimator(&animator)`, and every call above is mirrored on
`Ui` so a component never has to check for null.

`tick` clamps its delta. A window that was dragged, or a debugger that stopped
the process for a minute, would otherwise deliver one enormous step and finish
every animation at once.

Three behaviours are worth knowing because they are what keep it quiet:

- **Nothing animates the frame a key is first seen.** A window that opens with
  three switches already on plays no animation.
- **Keys nobody asks about are forgotten**, so a virtualised list does not
  accumulate state for rows that scrolled away.
- **`animating()`** is what an application that redraws only on change asks to
  decide whether the next frame has to be drawn at all.

### animate, pulse, latch

A **transition** closes the gap when a target changes. A **pulse** is a one-shot
with a lifetime that ends where it began — a ripple, a flash, a shake — and
reports raw progress, 1 when nothing is running, so `progress < 1` means "a
pulse is playing" and needs no second question. Retriggering restarts it, which
is what a second click on the same button should do.

`latch` remembers a number across frames, replacing it only when `set`. It is
the companion to `pulse`: an effect that starts at the pointer has to keep the
point it started from for as long as it runs, and the component drawing it is
rebuilt from scratch every frame.

Colours animate per channel, which is what makes a fade between two theme
tokens read as a fade rather than a jump through grey.

## Transition

```cpp
struct Transition {
    float duration = 0.14f;    // seconds; zero is immediate
    float delay = 0.0f;
    Easing easing = Easing::EaseOut;
    std::optional<CubicBezier> bezier{};   // set, it wins over `easing`

    float shape(float t) const;
};
```

Zero duration is how a component opts out without the call site changing shape.
`Design::motion` is the transition everything unspecified uses, so switching
design changes the *feel* as well as the look.

## Easing

```cpp
float ease(Easing curve, float t);
float ease(const CubicBezier& curve, float t);

struct CubicBezier { float x1, y1, x2, y2; };   // as cubic-bezier() in CSS
```

| Family | Values |
| --- | --- |
| CSS keywords | `Linear`, `Ease`, `EaseIn`, `EaseOut`, `EaseInOut` |
| Gentle to severe | `Sine`, `Quad`, `Cubic`, `Quart`, `Quint`, `Expo`, `Circ` — each `In`, `Out`, `InOut` |
| Overshooting | `Back`, `Elastic`, `Bounce` — each `In`, `Out`, `InOut` |
| Arriving | `Spring`, one overshoot and settle |

The four CSS keywords are the cubic Béziers CSS actually specifies, solved
rather than approximated: `EaseIn` is `cubic-bezier(0.42, 0, 1, 1)` and not
`t³`, which is a visibly different curve.

The last three families deliberately return values outside 0..1. That is the
point of them, and it is right for a position and wrong for an opacity.

`CubicBezier` is the escape hatch: four numbers out of a design tool need no new
enum value. Control points outside 0..1 on x are clamped, as CSS requires — a
non-monotonic timing function has no single answer.
