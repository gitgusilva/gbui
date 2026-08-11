# Testing

Layout is arithmetic and painting is a list of commands, so most of the library
is testable without a window, a GPU or a font installed. The suite is 203 cases
across nineteen files — layout, text wrapping, themes, JSON, paths, placement,
interaction, focus, text editing, virtualisation, scrolling, fonts, charts,
tables, the rich editor and the URL opener — it runs in well under a second, and
it is the reason changing the layout engine is not frightening.

```sh
ctest --test-dir build --output-on-failure
```

## Testing layout

```cpp
TEST("space refused by a max-width goes to the others, not to waste") {
    Arena arena;
    Ui ui(arena);
    NodeId capped, free;
    {
        auto row = ui.beginRow();
        capped = ui.add({.grow = 1.0f, .basis = 0.0f, .maxWidth = 60.0f});
        free = ui.add({.grow = 1.0f, .basis = 0.0f});
        (void)row;
    }

    const Theme theme = Theme::dark();
    LayoutContext context;
    context.theme = &theme;
    layout(arena, ui.root(), Rect{0, 0, 400, 20}, context);

    CHECK_NEAR(arena[capped].frame.width, 60.0f);
    CHECK_NEAR(arena[free].frame.width, 340.0f);
    CHECK_NEAR(arena[capped].frame.width + arena[free].frame.width, 400.0f);
}
```

Note what is being asserted: not "it looks right" but the arithmetic — the row
adds up to its container. That test exists because an earlier version
distributed free space once instead of looping, and lost the surplus of every
item that hit a maximum.

## Testing what is drawn

Record a display list and look at the commands:

```cpp
DisplayList list;
record(arena, ui.root(), theme, list);

bool paintedAccent = false;
for (const auto& command : list.commands()) {
    if (const auto* fill = std::get_if<FillRect>(&command)) {
        if (fill->color == theme.color(Token::Accent)) paintedAccent = true;
    }
}
CHECK(paintedAccent);
```

This is how "a primary button paints the accent" and "a transparent subtree
costs no draw commands" are tested — no screenshot, no tolerance, no flake.

## Testing data the library consumes

Themes, icons and paths are parsed from text somebody else wrote, so the tests
cover the failures as much as the successes:

```cpp
CHECK(!parseColor("300 0 0").has_value());       // out of range
CHECK(error.find("bgElevated") != std::string::npos);  // names the field at fault
```

One test walks **every shipped icon** and asserts its geometry stays inside the
24×24 grid. It failed twice while the icon pipeline was being written — once for
missing arc support, once for a moveto whose case was changed — and both bugs
would otherwise have shipped as "some icons look odd".

## The harness

The suite uses a 60-line harness in `tests/harness.hpp` rather than a framework,
because what these tests need is a name, an assertion and a non-zero exit code:

```cpp
TEST("name")          // declares a case
CHECK(expression)
CHECK_EQ(a, b)
CHECK_NEAR(a, b)      // floats, 0.01 tolerance
```

Add a `.cpp` to `tests/CMakeLists.txt` and it runs.

## Testing something interactive

An `Interaction` resolves against a tree, and a test can build one by hand — no
window, no pointer, no timing:

```cpp
Interaction interaction;
InputFrame frame;
frame.pointer = {40.0f, 14.0f};
frame.pointerDown = true;
interaction.update(arena, ui.root(), frame);      // press
CHECK(interaction.isPressed("history.row.3"));

frame.pointerDown = false;
interaction.update(arena, ui.root(), frame);      // release on the same tag
CHECK(interaction.clicked("history.row.3"));
```

Because the resolve runs against the tree that already exists, a test drives a
control exactly the way the loop does: build, update, build again. That is how
focus traversal, select's keyboard handling and the virtualised list's slice are
all asserted.

## Looking at the result

For anything visual, render it:

```sh
./build/examples/gbui_app --shot frame.ppm             # one frame, no display needed
./build/examples/gbui_controls --shot out.ppm --page 3 --tab 2   # a page, with focus moved
./build/examples/gbui_gallery out/ themes/             # every theme, four widths
```

The gallery is the review tool: the same screen in every theme it finds, at
1600, 1180, 900 and 680 px, where a layout regression or a theme with a bad
token shows up in one glance.

`gbui_controls --shot` is worth knowing in detail, because photographing a UI
that has no state of its own takes some care: `--pointer x y` parks the pointer
so a hover can be captured, `--tab n` presses Tab that many times so a focus ring
exists to photograph, and the shot renders several frames before writing —
everything that places itself from last frame's geometry needs a few frames to
settle, and a picture taken too early shows a layout that never appears on
screen.
