// Who is hovered, who is pressed, who has the keyboard.
//
// The tree is rebuilt every frame, so a NodeId cannot carry identity across
// one. Tags can: a node named "sidebar.branch.main" is the same control next
// frame even though every node in the tree is new. Interaction therefore
// tracks tags, and a component asks about itself by name.
//
// The order within a frame matters, and it is the same every time:
//
//     1. the platform produces an InputFrame
//     2. interaction.update(...) resolves it against LAST frame's tree
//     3. the new tree is built, and components read hovered/pressed/clicked
//     4. layout, record, paint
//
// Step 2 using the previous tree is not a compromise: it is the tree the user
// was looking at when they clicked.
#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "gbui/core/geometry.hpp"
#include "gbui/core/cursor.hpp"
#include "gbui/input/keys.hpp"
#include "gbui/scene/tree.hpp"

namespace gbui {

/** Everything the platform observed since the last frame. */
struct InputFrame {
    Vec2 pointer{};
    bool pointerDown = false;
    /** Wheel movement in lines; positive scrolls the content up. */
    float wheel = 0.0f;
    /**
     * Which modifiers are held right now.
     *
     * Level, not an event, and separate from the copy on each `KeyEvent`:
     * Ctrl+wheel and Shift+drag are gestures where no key event is generated at
     * all, so asking "was Ctrl down when a key was pressed" cannot answer them.
     */
    Modifiers modifiers{};
    std::vector<KeyEvent> keys;
    /** UTF-8 typed this frame, already composed by the platform's IME. */
    std::string text;

    /** Hands this frame's events over and leaves `other` empty of them.
     *  Pointer state is level, not an event, so it is copied rather than moved. */
    void swap_with(InputFrame& other) {
        pointer = other.pointer;
        pointerDown = other.pointerDown;
        wheel = other.wheel;
        modifiers = other.modifiers;
        keys.swap(other.keys);
        text.swap(other.text);
        other.keys.clear();
        other.text.clear();
        other.wheel = 0.0f;
    }
};

/** How focus arrived somewhere, which is what decides whether a ring is drawn.
 *
 *  `Program` is not a third modality: focus moved by code inherits whichever of
 *  the two the user last used, the same rule `:focus-visible` applies to a
 *  scripted `element.focus()`. A label clicked to focus its field therefore
 *  draws no ring, and the same label reached by Tab does. */
enum class FocusSource { Pointer, Keyboard, Program };

class Interaction {
public:
    /** Resolves the frame against a laid-out tree. Safe to call with an empty
     *  arena on the first frame, when there is no tree yet. */
    void update(const Arena& arena, NodeId root, const InputFrame& frame);

    // ---- what a component asks about itself ----------------------------
    bool isHovered(std::string_view tag) const { return !tag.empty() && tag == hovered_; }
    bool isPressed(std::string_view tag) const { return !tag.empty() && tag == pressed_; }

    /** True for the one frame the button went down on this tag.
     *
     *  `isPressed` is a level and `dragging` is the whole gesture; this is the
     *  edge, which is what a control needs to tell "the drag starts here" from
     *  "the drag continues" — a text field places its caret on the first and
     *  extends a selection on the rest. */
    bool pressStarted(std::string_view tag) const {
        return !tag.empty() && tag == pressStarted_;
    }
    bool isFocused(std::string_view tag) const { return !tag.empty() && tag == focused_; }

    /** Whether the focus ring should be drawn on this control — the CSS
     *  `:focus-visible` rule.
     *
     *  Focus and its ring are two different questions. A control clicked with
     *  the mouse has the keyboard, and pressing Space still activates it, but a
     *  ring around the thing the user just pointed at tells them nothing they
     *  did not already know; the same ring after Tab is the only thing saying
     *  where the keyboard went. So the ring follows how focus *arrived*.
     *
     *  Controls that take typing are the documented exception and use
     *  `isFocused` for their ring instead — see `textInput`. */
    bool isFocusVisible(std::string_view tag) const { return focusVisible_ && isFocused(tag); }

    /** Whether focus is on this node or anywhere inside it.
     *
     *  Keys are delivered to the focused control, but a container often wants
     *  them too: Page Down belongs to the scroll view even when the row the
     *  reader clicked is what actually holds focus. A tag alone cannot answer
     *  that, so the ancestry of the focused node is recorded each frame from
     *  the tree — which is the toolkit's equivalent of an event bubbling. */
    bool isFocusedWithin(std::string_view tag) const;

    /** True for one frame, when the button was released over the same tag it
     *  was pressed on — which is what a click means everywhere else, and why a
     *  press that slides off a button does not count. */
    bool clicked(std::string_view tag) const { return !tag.empty() && tag == clicked_; }

    /** Pointer position, for a control that tracks a drag — a slider. */
    Vec2 pointer() const { return pointer_; }
    /** How far the pointer moved since the last frame — what a drag applies. */
    Vec2 pointerDelta() const { return {pointer_.x - previousPointer_.x,
                                        pointer_.y - previousPointer_.y}; }
    bool pointerDown() const { return pointerDown_; }
    /** The tag the current press started on, held until release even when the
     *  pointer leaves it. A slider keeps following the pointer that way. */
    std::string_view dragging() const { return pressed_; }
    float wheel() const { return wheel_; }

    /**
     * The tag of the innermost node under the pointer that declared
     * `Overflow::Scroll`, or empty.
     *
     * A scrollable widget should react to the wheel only when this names *it*.
     * Testing "is the pointer somewhere inside me" instead is what makes a page
     * and the list inside it scroll together, and it is why a chart cannot
     * safely take the wheel without asking first.
     */
    std::string_view wheelTarget() const { return wheelTarget_; }

    /** The same, for the sideways wheel — Shift and a scroll. Resolved
     *  separately because the nearest thing that scrolls sideways is usually
     *  not the nearest thing that scrolls at all. */
    std::string_view wheelTargetX() const { return wheelTargetX_; }

    /** Modifiers held this frame, for gestures that are not key presses. */
    const Modifiers& modifiers() const { return modifiers_; }

    // ---- keyboard -------------------------------------------------------
    /** Keys for the focused control. Empty when nothing has focus. */
    const std::vector<KeyEvent>& keys() const { return keys_; }
    /** Text typed this frame, for the focused control. */
    std::string_view text() const { return text_; }

    std::string_view focused() const { return focused_; }
    std::string_view hovered() const { return hovered_; }

    /** True when the focused control, whichever it is, should show its ring. */
    bool focusVisible() const { return focusVisible_; }
    /** The modality the user last used, which is what `Program` resolves to. */
    bool keyboardModality() const { return keyboardModality_; }

    void focus(std::string_view tag, FocusSource source = FocusSource::Program) {
        focused_ = std::string(tag);
        focusVisible_ = source == FocusSource::Program ? keyboardModality_
                                                       : source == FocusSource::Keyboard;
    }
    void blur() {
        focused_.clear();
        focusVisible_ = false;
    }

    /** Where a tagged node was on the frame this was resolved against.
     *
     *  A control that needs geometry — a slider mapping a pointer to a value —
     *  cannot read it from the tree it is currently building, because layout
     *  has not run yet. Last frame's rectangle is the right answer: it is the
     *  one the user was pointing at. */
    Rect frameOf(std::string_view tag) const;

    /** The rectangle the last layout used. Floating components keep themselves
     *  inside it without the caller having to pass the window size around. */
    Rect viewport() const { return viewport_; }

    /** What the pointer should look like where it currently is, decided by the
     *  node under it. The platform reads this once a frame. */
    Cursor cursor() const { return cursor_; }

    /** Consumes one key so two controls cannot both act on it. A component
     *  that handles Tab, say, should take it. */
    bool take(Key key);

private:
    /** Walks up from the deepest hit node to the nearest tagged ancestor. */
    static std::string_view tagAt(const Arena& arena, NodeId root, Vec2 point);

    std::string hovered_;
    std::string pressed_;
    std::string pressStarted_;
    std::string focused_;
    std::string clicked_;
    /** The tagged ancestors of the focused node, innermost first. */
    std::vector<std::string> focusChain_;
    std::vector<std::string> focusables_;
    /** The tag of the node confining Tab right now, or empty. */
    std::string trap_;
    /** Where the keyboard was when that node appeared, so closing it can put
     *  the reader back rather than at the top of the page. */
    std::string focusBeforeTrap_;
    std::map<std::string, Rect, std::less<>> frames_;

    Vec2 pointer_{};
    Vec2 previousPointer_{};
    Rect viewport_{};
    Cursor cursor_ = Cursor::Default;
    bool pointerDown_ = false;
    bool wasDown_ = false;
    bool focusVisible_ = false;
    bool keyboardModality_ = false;
    float wheel_ = 0.0f;
    std::string wheelTarget_;
    std::string wheelTargetX_;
    Modifiers modifiers_{};
    std::vector<KeyEvent> keys_;
    std::string text_;
};

}  // namespace gbui
