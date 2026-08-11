# input

`#include "gbui/input/keys.hpp"`, `interaction.hpp`, `textEdit.hpp`

Who is hovered, who is pressed, who has the keyboard — and the editing model
behind a text field. The narrative version is [Input and
focus](/guide/input); this is the surface.

## The frame order

The tree is rebuilt every frame, so a `NodeId` cannot carry identity across one.
Tags can. `Interaction` therefore tracks tags, and the order within a frame is
always the same:

```cpp
const InputFrame input = window->takeInput();   // 1. the platform observed this
interaction.update(arena, root, input);         // 2. resolved against LAST frame's tree
window->setCursor(interaction.cursor());        // 3. the node under the pointer decided
root = buildAndDraw(interaction);               // 4. components ask about themselves
```

Step 2 using the previous tree is not a compromise: it is the tree the user was
looking at when they clicked.

## InputFrame

```cpp
struct InputFrame {
    Vec2 pointer;
    bool pointerDown;
    float wheel;                  // in lines, positive scrolls content up
    Modifiers modifiers;          // held right now — a level, not an event
    std::vector<KeyEvent> keys;
    std::string text;             // UTF-8, already composed by the IME

    void swap_with(InputFrame& other);
};
```

`modifiers` is separate from the copy on each `KeyEvent` because Ctrl+wheel and
Shift+drag generate no key event at all, so "was Ctrl down when a key was
pressed" cannot answer them.

## Interaction

```cpp
void update(const Arena&, NodeId root, const InputFrame&);

// pointer
bool isHovered(tag) const;
bool isPressed(tag) const;        // a level: the button is down and started here
bool pressStarted(tag) const;     // the edge: the frame it went down
bool clicked(tag) const;          // pressed and released on the same tag
std::string_view hovered() const;
Vec2 pointer() const;
Vec2 pointerDelta() const;        // movement since the last frame
bool pointerDown() const;
std::string_view dragging() const;   // the tag the press started on

// wheel
float wheel() const;
std::string_view wheelTarget() const;    // innermost Overflow::Scroll under the pointer
std::string_view wheelTargetX() const;   // …and the sideways one
const Modifiers& modifiers() const;

// focus
bool isFocused(tag) const;
bool isFocusVisible(tag) const;   // focused, and the ring should be drawn
bool isFocusedWithin(tag) const;  // focused, or something inside it is
std::string_view focused() const;
bool focusVisible() const;
bool keyboardModality() const;    // the modality the user last used
void focus(tag, FocusSource = FocusSource::Program);
void blur();

// keyboard
const std::vector<KeyEvent>& keys() const;   // for the focused control
std::string_view text() const;
bool take(Key);                   // consume a key so two controls cannot both act

// geometry
Rect frameOf(tag) const;          // where that node was last frame
Rect viewport() const;            // the rectangle the last layout used
Cursor cursor() const;            // what the pointer should look like now
```

`clicked` requires press and release on the same tag, so a press that slides off
a button does not count — the behaviour every desktop toolkit has. `dragging`
keeps naming the tag the press began on even after the pointer leaves it, which
is what lets a slider follow a pointer that has left the track.

`frameOf` exists because a control cannot measure itself while it is being
built. A slider maps the pointer to a value using the track's rectangle from the
previous frame, which is the rectangle the pointer was actually over.

`viewport()` is what a floating component keeps itself inside, so the caller
never has to pass the window size around.

## Focus

A node opts in while it is built:

```cpp
ui.tag("settings.autofetch").focusable();
```

`Interaction` collects focusable tags in tree order, so Tab and Shift+Tab
traverse them in the order they appear on screen. Clicking a control moves focus
to it; clicking nothing takes focus away.

### Focus and its ring are two questions

A control clicked with the mouse has the keyboard — Space still activates it —
but drawing a ring around the thing the user just pointed at tells them nothing.
The same ring after Tab is the only thing saying where the keyboard went. So
`isFocused` answers *who gets the keys* and `isFocusVisible` answers *whether to
draw the ring*, following the CSS `:focus-visible` rule:

```cpp
const bool focused = input.isFocused(id);        // handle keys
if (input.isFocusVisible(id)) {                  // draw the ring
    style.outline = Outline{2.0f, 2.0f, Fill{Token::Accent}};
}
```

`focus(tag, source)` says how focus arrived. `FocusSource::Program` — the
default — inherits whichever modality the user last used, the same rule the web
applies to a scripted `element.focus()`: a label clicked to focus its field
draws no ring, and the same label reached by Tab does.

Controls that take typing are the documented exception and ring on plain
`isFocused`: `textField` and `numberField` show it however focus arrived,
because a box that will swallow the next keystroke has to say so.

### Focus within

Keys go to the focused control, but a container often wants them too — Page Down
belongs to a scroll view even when the row the reader clicked holds focus.
`isFocusedWithin` answers that: the ancestry of the focused node is recorded
from the tree each frame, which is this toolkit's equivalent of an event
bubbling.

```cpp
if (input.isFocusedWithin("history")) { /* arrow keys walk the selection */ }
```

## Keys

```cpp
enum class Key {
    Unknown,
    Left, Right, Up, Down, Home, End, PageUp, PageDown,
    Backspace, Delete, Return, Tab, Escape, Space,
    A, C, V, X, Z, Y, T, D, Plus, Minus,
};

struct Modifiers { bool shift, ctrl, alt, super; bool command() const; };
struct KeyEvent  { Key key; Modifiers modifiers; bool repeat; };
```

`command()` is Ctrl, or Cmd on macOS, so a shortcut is written once. The letter
list is short on purpose: it is what components react to as *keys*. Anything
typed arrives as `InputFrame::text` instead, because a key code says which
button was pressed and only the platform knows what that produces on the user's
layout.

## Text editing

```cpp
struct TextEditState {
    std::string text;
    std::size_t caret;    // byte offsets, always on a character boundary
    std::size_t anchor;   // equal to caret when nothing is selected

    bool hasSelection() const;      // caret != anchor — no separate flag
    std::size_t selectionStart() const, selectionEnd() const;
    std::string_view selectedText() const;
    void moveToEnd(), selectAll(), clampToText();
};

TextEditResult applyInput(TextEditState&, const std::vector<KeyEvent>&, std::string_view typed);
TextEditResult insertText(TextEditState&, std::string_view);
TextEditResult applyKey(TextEditState&, const KeyEvent&);
```

```cpp
struct TextEditResult { bool changed, moved, submitted, cancelled; };
```

Offsets are bytes into UTF-8 and every operation keeps them on a character
boundary, so a caret can never land inside a multi-byte sequence. Word
movement — Ctrl+Left, Ctrl+Backspace — treats bytes above ASCII as word
characters, which keeps accented words whole.

The helpers are public because they are useful on their own:

```cpp
std::size_t previousCharacter(text, offset);
std::size_t nextCharacter(text, offset);
std::size_t previousWord(text, offset);
std::size_t nextWord(text, offset);
```
