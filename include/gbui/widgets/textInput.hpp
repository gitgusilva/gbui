// A single-line input, and a type that says what it accepts.
//
// The HTML shape on purpose: one element with a `type`, rather than a family of
// near-identical components. A password box and a number box *are* a text box
// that draws its content differently and refuses some of what is typed into it,
// and the two files that used to say so separately drifted exactly as two
// copies do — `numberField` never grew a caret, so the one control in the set
// that most wants typing was the one that could not be typed into.
//
// This replaces `textField` and `numberField`. Neither is a rename:
//
// **It cannot be called `input`.** Every call site in this tree names its
// `Interaction` parameter `input`, and a local of that name hides a
// namespace-scope function of the same name completely — `input(ui, input, …)`
// fails with *no match for call to `(const gbui::Interaction)`*, which is a
// confusing error a long way from its cause. `textInput` is React Native's name
// for the same thing.
//
// **Number entry is text entry**, and that is why `numberField` could not
// become a wrapper over this. A caret needs a string, and while a number is
// being typed that string is not yet a number: `-`, `1.`, and empty are all
// states a reader passes through on the way to a value. So the state is a
// `TextEditState` like any other input's, the text is the source of truth while
// the box has the keyboard, and nothing rewrites it underneath the caret. On
// blur it is normalised from the clamped value, which is the only moment the
// two can disagree without confusing somebody.
//
// ---- what `type` deliberately does not carry -------------------------------
//
// HTML also has `email`, `url`, `tel` and `search`. They are absent because
// here they would do nothing: their whole effect is to pick a soft keyboard and
// to hand the browser a validator, and this toolkit has neither. An enumerator
// that changes no behaviour is worse than a gap — a caller writes it, believes
// something was checked, and nothing was. `search` will earn its place when
// there is a clear affordance to draw and a filterable `select` to use it.
#pragma once

#include <optional>
#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/input/textEdit.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/icons.hpp"

namespace gbui {

/** What a box accepts, and therefore how it draws what is in it. */
enum class InputType {
    /** Anything printable, unfiltered. */
    Text,
    /** Bullets instead of the characters, and an eye to hold them visible. The
     *  state still holds the real string — this hides it, it does not protect
     *  it. */
    Password,
    /** Digits only, with a range, the wheel, arrow keys and step buttons. */
    Number,
};

/** Where the step buttons go.
 *
 * `Sides` reads best when there is room. `Stacked` is the classic spin box —
 * two half-height arrows on one side — and is what a narrow field wants,
 * because the value keeps the whole width instead of sharing it with two
 * buttons. `None` leaves the wheel and the arrow keys. */
enum class StepperPlacement { Sides, Stacked, None };

struct TextInputOptions {
    /** What the box accepts. Everything below marked for one type is ignored by
     *  the others. */
    InputType type = InputType::Text;
    /**
     * What this is called, for a reader who cannot see the caption beside it.
     *
     * Unnecessary when a `label` or a `field` names it — those attach the
     * relation, and a name given twice is a name read out twice. Necessary the
     * rest of the time, and the placeholder is not a substitute: a box named by
     * its placeholder loses its name the moment somebody types in it.
     */
    std::string_view name{};
    /** Shown while the text is empty. It steps aside once the box has the
     *  keyboard: a hint you are being asked to type over is noise the moment
     *  you start. */
    std::string_view placeholder{};
    bool disabled = false;
    bool readOnly = false;
    /**
     * Draws the box in the error colour.
     *
     * The control's half of the state `field` owns the message for. A component
     * does not reach into another component's options, so a field with an error
     * on it does not restyle its control by remote control — the caller says so
     * twice, once in each place, and both are readable on their own.
     */
    bool invalid = false;
    /** A glyph at the leading edge, inside the box. */
    std::optional<Icon> leading{};

    // ---- Password --------------------------------------------------------
    /**
     * The eye at the trailing edge that shows the text while it is held.
     *
     * On by default, because a field you cannot read back is a field people
     * mistype into; turn it off where a shoulder-surfer is the threat being
     * designed against.
     */
    bool revealToggle = true;
    /** Whether the text is currently shown. The caller owns it, like every
     *  other piece of state here, and flips it when the result says so. */
    bool revealed = false;

    // ---- Number ----------------------------------------------------------
    double minimum = -1e18;
    double maximum = 1e18;
    /** What one arrow, one wheel notch or one step button is worth. */
    double step = 1.0;
    /** Digits after the point. Zero also stops a point being typed at all. */
    int decimals = 0;
    /** Drawn after the value, and never part of it: `" min"`, `"px"`, `"%"`.
     *  A suffix inside the editable text would be a suffix the caret can be put
     *  in the middle of. */
    std::string_view suffix{};
    StepperPlacement steppers = StepperPlacement::Sides;
    /** Below this width the steppers drop to `Stacked` automatically, so a box
     *  in a narrow column shows its value rather than two buttons. */
    float stackedBelow = 110.0f;

    // ---- geometry --------------------------------------------------------
    /** Zero takes the active design's control height. */
    float height = 0.0f;
    /** `kAuto` fits the content — except on a number, which takes 120 px: a box
     *  sized to its digits changes width as they are typed, and the steppers
     *  walk out from under the pointer clicking them. */
    float width = kAuto;
    float grow = 0.0f;
};

/** What an input reports back beyond the edit itself. */
struct TextInputResult : TextEditResult {
    /** The eye was clicked this frame: flip the `revealed` you passed in. */
    bool toggledReveal = false;
    /**
     * Number only: the text read as a number, clamped to the range.
     *
     * Clamped even while the text is not, because the two answer different
     * questions — the box shows what is being typed, and this is what the
     * application should store. Typing `500` into a box that stops at `60`
     * leaves `500` on screen until blur and returns `60` throughout.
     */
    double value = 0.0;
    /** Whether the text was a number at all. False for an empty box, which is
     *  how a number input says "no value" — there is no other way to say it. */
    bool hasValue = false;
};

/**
 * Edits `state` in place when it has focus and returns what happened, so a
 * caller can react to a submit without diffing the text.
 *
 * The pointer places the caret: a press puts it at the character it landed on,
 * and holding and moving drags a selection out from there. Both measure the run
 * the same way the caret is drawn, so what is clicked is where it lands.
 */
TextInputResult textInput(Ui& ui, const Interaction& input, std::string_view id,
                          TextEditState& state, const TextInputOptions& options = {});

}  // namespace gbui
