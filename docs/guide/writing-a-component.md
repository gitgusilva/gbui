# Writing a component

A component is **a function that takes a `Ui&` and an options struct**. There is
no base class, nothing to inherit and nothing to register.

Two shapes, depending on whether it has children:

```cpp
NodeId    thing(Ui&, args…);        // a leaf: builds and returns
Ui::Scope beginThing(Ui&, args…);   // a container: the caller fills it
```

## A worked example

The changes list shows `M`, `A`, `D` or `?` beside a path, coloured by what
happened to the file.

One component is one header and one source file, named after it — the way Qt
gives every widget a file of its own. A new component adds two files and one
line to `CMakeLists.txt`; it does not grow an existing one.

**`include/gbui/widgets/statusPill.hpp`** — the file is named for the symbol it
declares, in the same case.

```cpp
enum class FileStatus { Modified, Added, Deleted, Untracked };

struct StatusPillOptions {
    float size = 18.0f;
};

/** The square status marker beside a path in the changes list. */
NodeId statusPill(Ui& ui, FileStatus status, const StatusPillOptions& options = {});
```

**`src/widgets/statusPill.cpp`**

```cpp
namespace {

struct StatusLook {
    Token color;
    std::string_view letter;
};

StatusLook lookFor(FileStatus status) {
    switch (status) {
        case FileStatus::Added:     return {Token::Added, "A"};
        case FileStatus::Deleted:   return {Token::Removed, "D"};
        case FileStatus::Untracked: return {Token::TextMuted, "?"};
        case FileStatus::Modified:  break;
    }
    return {Token::Modified, "M"};
}

}  // namespace

NodeId statusPill(Ui& ui, FileStatus status, const StatusPillOptions& options) {
    const StatusLook look = lookFor(status);

    Style style;
    style.width = options.size;
    style.height = options.size;
    style.justify = Justify::Center;
    style.align = Align::Center;
    style.radius = 4.0f;
    // The letter on a wash of its own colour reads at a glance without needing
    // a second token per status.
    style.background = Fill{look.color, 0.20f};
    style.shrink = 0.0f;      // a fixed marker never gives up its width

    auto scope = ui.begin(style);
    text(ui, look.letter, {.color = look.color, .weight = FontWeight::SemiBold, .size = 11.0f});
    return scope.id();
}
```

Used:

```cpp
auto row = beginListRow(ui, {.selected = isSelected});
statusPill(ui, FileStatus::Modified);
text(ui, "themes/nord/theme.json", {.grow = 1.0f});
```

## The four rules that example is demonstrating

1. **An options struct, not six parameters.** Call sites stay readable, and
   adding an option later does not break any of them.
2. **Tokens, never colours.** A component that hardcodes a colour is a bug
   report waiting for the first light theme.
3. **`shrink = 0` on anything whose size is its meaning** — a marker, a pill, a
   button, an icon. Without it, a crowded row elides it into nothing.
4. **Containers return the `Scope`; leaves return the `NodeId`.** The caller can
   then tag it, or find it in a test.

## A scope closes where it dies

The one mistake this API makes easy, and it has caused four layout bugs here:

```cpp
{
    auto row = ui.beginRow();
    …
    (void)row;          // silences a warning. It is NOT a close.
}                       // <- this is the close
```

Everything built after `(void)row` is still a child of that row. The last time
it bit, a date picker's two arrows came out different sizes because the second
was built inside the month label. When a container has to end before the
enclosing block does, put it in braces of its own or call `scope.close()`.

A component that opens two containers and hands back one scope uses
`adopt()`/`disown()`: the **inner** scope takes on the extra pop and the outer
one is disowned. The other way round leaves the outer popping the moment the
component returns, which unbalances the stack and puts everything after it in
the wrong parent.

## Interactive components take an `Interaction`

A control that reacts adds two things: the frame it reads, and an id to be known
by.

```cpp
bool statusFilter(Ui& ui, const Interaction& input, std::string_view id, bool on) {
    auto scope = ui.begin({.cursorHint = Cursor::Pointer});
    ui.tag(id).focusable();

    bool activated = input.clicked(id);
    if (input.isFocused(id)) {
        for (const KeyEvent& event : input.keys())
            if (event.key == Key::Space || event.key == Key::Return) activated = true;
    }
    if (input.isFocusVisible(id)) /* draw the ring with an Outline */;
    return activated;
}
```

It reports what happened and never writes to the caller's model — which is what
makes undo, validation and "are you sure?" possible without the toolkit knowing
about any of them.

Shape and sizing come from `ui.design()` rather than from constants, so the
component follows a design switch; anything animated goes through `ui.animate`
with the same `id`, so it costs nothing when no animator is present.

## Where the state lives

Components are stateless functions. Anything that persists — which row is
selected, what a field contains, whether a menu is open — belongs to the
application and is passed in:

```cpp
auto row = beginListRow(ui, {.selected = state.selected == path,
                             .hovered  = state.hovered == path,
                             .id       = path});
```

That keeps the toolkit out of the business of owning your model, and it is why
the tree can be thrown away and rebuilt without losing anything.

## Checklist before it is done

- Does it re-theme? Load a light theme and look.
- Does it survive a narrow window? Put it in a row with something that grows.
- Does it have a header and a source file of its own, both named after it, and
  is the source listed in `CMakeLists.txt`?
- Is it in the umbrella for its group — `components.hpp`, `controls.hpp`,
  `containers.hpp` or `overlays.hpp`? A component nothing includes is not
  supported, it is an accident.
- Is anything it shares with a sibling component in `src/widgets/detail.hpp`
  rather than copied? A helper used by exactly one component stays with it.
- Does it have a test? Layout is pure, so a test is ten lines — see
  [Testing](/guide/testing).
