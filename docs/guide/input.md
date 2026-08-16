# Input and focus

The tree is rebuilt every frame, so a `NodeId` cannot carry identity across one.
A **tag** can: a node named `sidebar.branch.main` is the same control next frame
even though every node in the tree is new. `Interaction` tracks tags, and a
component asks about itself by name.

## The order within a frame

It is the same every time, and getting it wrong is the source of every "my
button needs two clicks" bug:

```cpp
const InputFrame frame = window->takeInput();   // 1. what the platform observed
interaction.update(arena, root, frame);         // 2. resolved against LAST frame's tree
window->setCursor(interaction.cursor());        // 3. the node under the pointer decided it
root = build(ui, interaction);                  // 4. components ask about themselves
layout(arena, root, viewport, context);         // 5. …then layout, record, paint
```

Step 2 using the previous tree is not a compromise. It is the tree the user was
looking at when they clicked, and it is the only one that exists at that moment.

## What a component asks

```cpp
input.isHovered(id);        // the pointer is over it
input.isPressed(id);        // the button is down and started here — a level
input.pressStarted(id);     // …and this is the frame it went down — an edge
input.clicked(id);          // pressed and released on the same tag
input.dragging();           // the tag the current press started on
input.pointerDelta();       // how far the pointer moved since last frame
```

`clicked` requires press *and* release on the same tag, so a press that slides
off a button does not count — the behaviour every desktop toolkit has. The split
between `isPressed` and `pressStarted` is what lets a text field place its caret
on the first frame of a drag and extend a selection on the rest.

`dragging` keeps naming the tag the press began on even when the pointer wanders
off it, which is why a slider keeps following a pointer that has left the track.

## Geometry from last frame

```cpp
const Rect track = input.frameOf("volume.track");
```

A control cannot measure itself while it is being built — layout has not run.
Last frame's rectangle is the right answer anyway: it is the one the pointer was
actually over. Sliders, carets, popovers and charts all map a pointer position
through `frameOf`.

The cost is one frame of lag whenever the geometry changes, which is invisible
at 60 Hz and visible the first time a popup opens. `Position::Absolute` avoids
it entirely where it applies, because `left` and `top` are already relative to
the thing the node belongs to.

## Focus, and its ring

A node opts in while it is being built:

```cpp
ui.tag("settings.autofetch").focusable();
```

`Interaction` collects focusable tags in tree order, so Tab and Shift+Tab
traverse them in the order they appear on screen. Clicking a control moves focus
to it; clicking nothing takes focus away.

**Focus and its ring are two questions.** A control clicked with the mouse has
the keyboard — Space still activates it — but a ring around the thing the user
just pointed at tells them nothing. The same ring after Tab is the only thing
saying where the keyboard went:

```cpp
const bool focused = input.isFocused(id);        // handle keys
if (input.isFocusVisible(id))                    // draw the ring
    style.outline = Outline{2.0f, 2.0f, Fill{Token::Accent}};
```

That is the CSS `:focus-visible` rule. `focus(tag, source)` says how focus
arrived, and `FocusSource::Program` — the default — inherits whichever modality
the user last used, exactly as the web treats a scripted `element.focus()`: a
label clicked to focus its field draws no ring, and the same label reached by
Tab does.

Text and number fields are the documented exception and ring however focus
arrived, because a box that will swallow the next keystroke has to say so.

Use `Outline` rather than `Border` for a ring. An outline is drawn *outside* the
border box and takes no space, so showing one cannot move the control — which is
the single most common way a focus ring gets implemented badly.

## Focus within

Keys go to the focused control, but a container often wants them too: Page Down
belongs to the scroll view even when the row the reader clicked is what holds
focus.

```cpp
if (input.isFocusedWithin("history")) { /* arrows walk the selection */ }
```

The ancestry of the focused node is recorded from the tree each frame, which is
this toolkit's equivalent of an event bubbling.

## Keys and text

```cpp
for (const KeyEvent& event : input.keys()) { … }
const std::string_view typed = input.text();
input.take(Key::Tab);        // consume it, so two controls cannot both act
```

Printable characters arrive as `text`, never as keys: a key code says which
button was pressed, and only the platform knows what that produces on the user's
keyboard layout. `Modifiers::command()` is Ctrl, or Cmd on macOS, so a shortcut
is written once.

`Interaction::modifiers()` is a *level*, separate from the copy on each
`KeyEvent`, because Ctrl+wheel and Shift+drag are gestures where no key event is
generated at all.

## Where the wheel goes

```cpp
if (input.wheelTarget() == id) state.offset -= input.wheel() * step;
```

`Overflow::Scroll` clips exactly as `Hidden` does; the difference is that it
declares the node a candidate for the wheel. `wheelTarget()` then resolves the
**innermost** such node under the pointer by walking up from the deepest hit.

Before that existed, every scroll view reacted to "the pointer is somewhere
inside me", which is true of a page and of the list on it at once: one notch
moved both. `wheelTargetX()` answers the same question for the sideways wheel,
resolved separately because the nearest thing that scrolls sideways is usually
not the nearest thing that scrolls at all.

It is also what lets a widget take the wheel for something that is not
scrolling. A chart checks `wheelTarget()` against its own plot before zooming,
so a chart in the middle of a page cannot swallow a scroll aimed past it.

## Cursors

```cpp
ui.tag("divider").cursor(Cursor::ResizeHorizontal);
window->setCursor(interaction.cursor());   // once a frame, in the loop
```

The node under the pointer decides the shape and the loop only forwards it.
`Cursor::Default` means "ask my parent", so a label inside a button shows the
button's.

## Editing text

`TextEditState` is a string, a caret and an anchor, all in **byte** offsets that
every operation keeps on a character boundary — so a caret can never land inside
a multi-byte sequence:

```cpp
TextEditResult result = applyInput(state, input.keys(), input.text());
if (result.submitted) commit(state.text);
```

Word movement treats bytes above ASCII as word characters, which keeps accented
words whole. `textInput` wraps all of this; the functions are public because
they are useful on their own.
