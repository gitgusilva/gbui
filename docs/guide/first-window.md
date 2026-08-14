# Your first window

Sixty lines, start to finish. Create `main.cpp`:

```cpp
#include "gbui/input/interaction.hpp"
#include "gbui/layout/layout.hpp"
#include "gbui/paint/canvas.hpp"
#include "gbui/paint/paint.hpp"
#include "gbui/platform/font.hpp"
#include "gbui/platform/window.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/style/theme.hpp"
#include "gbui/widgets/components.hpp"
#include "gbui/widgets/controls.hpp"

using namespace gbui;

int main() {
    const Theme theme = Theme::dark();
    FontDatabase fonts;
    Arena arena;
    arena.reserve(512);

    Interaction interaction;     // who is hovered, pressed, focused
    NodeId root;                 // last frame's tree, for the next resolve
    bool showTags = true;        // the application's state, not the toolkit's

    auto window = Window::create({.title = "Hello", .width = 480, .height = 240});
    if (!window) return 1;

    while (window->pumpEvents()) {
        // 1. Resolve the input against LAST frame's tree — the one the user was
        //    looking at when they clicked.
        const InputFrame input = window->takeInput();
        interaction.update(arena, root, input);
        window->setCursor(interaction.cursor());

        // 2. Build. The tree is thrown away and rebuilt every frame; the arena
        //    makes that cost a reset rather than thousands of deletes.
        const float scale = window->scale();
        arena.reset();
        Ui ui(arena);
        ui.setMeasure(measureWith(fonts, scale), theme.typography());
        {
            auto column = ui.column({.justify = Justify::Center,
                                          .align = Align::Center,
                                          .gap = 12.0f,
                                          .background = Fill{Token::Bg}});
            text(ui, "Hello from gbui", {.color = Token::TextStrong,
                                         .weight = FontWeight::SemiBold});
            if (checkbox(ui, interaction, "tags", showTags, {.label = "Show tags"}))
                showTags = !showTags;
            button(ui, interaction, "COMMIT", {.variant = ButtonVariant::Primary,
                                               .leading = Icon::GitCommitHorizontal});
        }
        root = ui.root();

        // 3. Lay out into the window, in logical pixels.
        LayoutContext context;
        context.theme = &theme;
        context.measure = measureWith(fonts, scale);
        const Vec2 size = window->size();
        layout(arena, root, Rect{0, 0, size.x, size.y}, context);

        // 4. Record what to draw, then draw it.
        DisplayList list;
        list.setScale(scale);            // the one place logical becomes device
        record(arena, root, theme, list, context.measure);

        Canvas& canvas = window->canvas();
        canvas.clear(theme.color(Token::Bg));
        SoftwarePainter painter(canvas, fonts, theme.typography());
        painter.paint(list);
        window->present();
    }
    return 0;
}
```

Build it:

```cmake
cmake_minimum_required(VERSION 3.20)
project(hello CXX)
add_subdirectory(gbui)
add_executable(hello main.cpp)
target_link_libraries(hello PRIVATE gbui::gbui)
```

## A note on `using namespace gbui`

The file above is a `main.cpp` that does nothing except build a UI, so the
directive is fine there and every example in this repository uses it.

**An application is not that file.** Once there is a model, a repository layer
and two other libraries in the same translation unit, pulling four hundred
names — including `text`, `table`, `select`, `label`, `icon` and `progress` —
into the global namespace is a habit that will cost something eventually. The
library has one flat namespace on purpose (see below); it does not follow that
you should open it.

A namespace alias is the two-line answer, and the name is yours to pick:

```cpp
namespace gb = gbui;

void commitBox(gb::Ui& u, const gb::Interaction& in) {
    auto panel = gb::panel(u, {.direction = gb::Direction::Column});
    gb::sectionHeading(u, "COMMIT MESSAGE");
    gb::button(u, in, "COMMIT", {.variant = gb::ButtonVariant::Primary, .block = true});
}
```

`gb` rather than `ui`, because `Ui ui(arena)` is the conventional variable name
and an alias that shadows it is a confusing hour.

**Why the namespace is flat.** `gbui::input::Interaction` and
`gbui::paint::DisplayList` would appear in the signature of nearly every
component, and the include path — `gbui/widgets/button.hpp` — already says
where a name lives without repeating it at each use. Sub-namespaces are kept
for modules with a vocabulary of their own that nobody uses while building a
tree: `gbui::json` and `gbui::meta` are the two, and a text shaper or an
accessibility bridge would be the next.

## What each step is doing

**Input.** `Interaction` is resolved against the tree that is still in the
arena — last frame's. That is not a compromise: it is the tree that was on
screen when the pointer went down. Components then ask about themselves by tag
while the new tree is being built. See [Input and focus](/guide/input).

**Build.** `Ui` writes nodes into the `Arena`. The braces matter: `column`
returns a scope guard that closes the container when it goes out of scope, so
the shape of the code is the shape of the UI. `setMeasure` hands the builder the
same measurer layout will use, which is what lets a text field put its caret at
a byte offset that has a position on screen.

**Layout.** `layout` measures and positions everything inside the rectangle you
give it, writing a `frame` onto each node. Passing `context.measure` is what
makes text measure with the font that will draw it — skip it and the library
falls back to an estimate, and runs are elided slightly early or late.

**Paint.** `record` walks the laid-out tree and produces a `DisplayList`: a flat
sequence of drawing commands with everything already resolved. `SoftwarePainter`
turns those into pixels.

## Scale, in one place

Everything above the backend works in **logical** pixels — layout, hit testing
and the input events all speak the same units, so a component never learns what
display it is on. Two calls convert:

```cpp
context.measure = measureWith(fonts, window->scale());  // rasterise at device size
list.setScale(window->scale());                         // multiply on the way out
```

`DisplayList::setScale` scales geometry, radii, border widths and font sizes
together. A border that stayed one pixel while everything around it doubled is
the giveaway that someone scaled the rectangles and stopped there.

## Motion

Animation is opt-in and costs one object the application owns, because the
animator is what remembers where a value was last frame and the arena is reset
every frame:

```cpp
Animator animator;
// once a frame, before building:
animator.tick(deltaSeconds);
ui.setAnimator(&animator);
```

Without it, every component lands on its target immediately and nothing else
changes. See [Motion](/guide/motion).

## Redrawing only when something changes

The loop above repaints continuously, which is what an application with an
animation running wants. One that is idle should not:

```cpp
if (!window->resized() && input.keys.empty() && input.text.empty()
    && !animator.animating() && input.wheel == 0.0f && !input.pointerDown) {
    window->present();      // vsync paces the loop; nothing is redrawn
    continue;
}
```

That difference — repaint on change, not on a clock — is what separates an idle
process at 0% CPU from one burning a core to show a list nobody is touching.
Note there is no `sleep` anywhere: with `WindowOptions::vsync` on, `present`
returns when the frame has been shown, and a hand-rolled sleep can only overshoot
the next refresh.

## Without a display

For a test, a screenshot or a machine with no display server, skip the window
and paint into a `Canvas` directly:

```cpp
Canvas canvas(1180, 620);
drawFrame(canvas, theme, fonts, arena);
// canvas.pixels() is RGBA8, row-major, canvas.pitch() bytes per row
```

The examples do exactly this: `gbui_app --shot frame.ppm` renders one frame and
writes it to a file, and `gbui_controls --shot out.ppm --scale 2` does it at
twice the resolution.
