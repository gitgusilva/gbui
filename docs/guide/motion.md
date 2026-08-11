# Motion

The model is CSS's `transition`, not its `@keyframes`, and that difference is
what makes it fit a tree rebuilt from scratch every frame. A component never
says "animate from here to there". It says **where the value should be now** —
checked, hovered, open — and asks what it currently is:

```cpp
const float on = ui.animate(id, "on", checked ? 1.0f : 0.0f);
knob.left = on * travel;
```

The animator keeps the difference between frames, keyed by the tag the node
already carries for hit testing. Nothing new is threaded through a call site,
and components stay stateless.

## Opting in

```cpp
Animator animator;                     // outlives the arena: it is the memory

while (window->pumpEvents()) {
    animator.tick(deltaSeconds);       // once a frame, before anything is built
    Ui ui(arena);
    ui.setAnimator(&animator);
    …
}
```

Without an animator every component returns its target and behaves exactly as it
did before, so an application that has not opted in gets no animation and no
surprises.

`tick` clamps its delta. A window that was dragged, or a debugger that stopped
the process for a minute, would otherwise deliver one enormous step and finish
every animation at once.

## The three shapes

```cpp
float  ui.animate(id, property, target, transition);   // a value travelling
Color  ui.animate(id, property, target, transition);   // per channel
float  ui.pulse(id, property, trigger, transition);    // a one-shot, 0 → 1
float  ui.latch(id, property, value, set);             // remembered across frames
float  ui.now();                                       // seconds, for loops
```

A **transition** closes the gap when a target changes. A **pulse** is an event
with a lifetime that ends where it began — a ripple, a flash, a shake — and a
transition is the wrong model for it, because nothing is travelling anywhere. It
reports raw progress, and 1 when nothing is running, so `progress < 1` means "a
pulse is playing" without a second question.

`latch` is the companion: an effect that starts at the pointer has to keep the
point it started from for as long as it runs, and the component drawing it is
rebuilt from scratch every frame.

`now()` is for the things that *loop* rather than travel — a caret's blink, a
spinner. They have no target and never finish, so they are a phase read off a
clock rather than an animation.

## Two rules that keep it quiet

**Nothing animates the frame a key is first seen.** A window that opens with
three switches already on plays no animation, and a row scrolled into a
virtualised list does not shimmer.

**Keys nobody asks about are forgotten.** A virtualised list does not accumulate
state for the fifty thousand rows that scrolled away.

## Curves

`Transition` is a duration, a delay and an easing. The four CSS keywords are the
cubic Béziers CSS actually specifies, solved rather than approximated — so
`EaseIn` is `cubic-bezier(0.42, 0, 1, 1)` and not `t³`, which is a visibly
different curve. Beside them are the classic families in / out / in-out: sine,
quad, cubic, quart, quint, expo, circ, back, elastic, bounce, and a
single-overshoot `Spring`.

```cpp
ui.animate(id, "x", target, {.duration = 0.2f, .easing = Easing::Spring});
ui.animate(id, "x", target, {.bezier = CubicBezier{0.2f, 0.0f, 0.0f, 1.0f}});
```

`EaseOut` is the default and the right one in an interface: a control that
leaves fast and arrives slowly feels like it is responding to you, while one
that eases in feels like it hesitated.

Back, elastic and bounce deliberately leave the 0..1 range. That is the point of
them, and it is fine for a position and wrong for an opacity.

The application's whole feel is one field on the `Design`:
`Design::motion` is the transition everything unspecified uses, which is why
switching from `gitbox()` to `cupertino()` changes how things move and not only
how they look.

## What it does not do yet

- **No keyframes.** Anything that loops without a target changing — a spinner,
  an indeterminate bar — still needs a clock from the application, which is what
  `now()` and `ProgressOptions::phase` are for.
- **No transforms.** `scale` and the rest belong on the node and want to be
  applied after placement, so painting and hit testing see the same geometry. A
  component that wants to grow recomputes its own numbers today.
- **Colour transitions move a resolved colour, not a token.** A component that
  wants to travel between two tokens fades one over the other at an alpha
  instead, which works and is not the same thing as interpolating the two.
- **No reduced-motion flag.** The honest implementation is one switch on
  `Animator` that collapses every duration to zero, so components keep one code
  path; it is not built.
