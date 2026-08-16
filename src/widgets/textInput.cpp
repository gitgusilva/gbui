#include "gbui/widgets/textInput.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#include "detail.hpp"
#include "gbui/widgets/icon.hpp"
#include "gbui/widgets/text.hpp"

namespace gbui {

// What this shares with its siblings, rather than a copy in each.
using namespace detail;

namespace {

/**
 * Whether `text` could still become a number as more is typed.
 *
 * Not "is a number": empty, a lone `-` and a trailing `.` are all states a
 * reader passes through, and a box that refuses the keystroke that produces one
 * of them cannot be typed into at all. The check is what a number box uses to
 * reject a letter without owning a parser — anything this says no to is put
 * back the way it was, so the text is never left in a shape the caret has to
 * be rescued from.
 */
bool isPartialNumber(std::string_view text, bool allowSign, bool allowPoint) {
    bool point = false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '-' || c == '+') {
            if (i != 0 || !allowSign) return false;
        } else if (c == '.') {
            if (point || !allowPoint) return false;
            point = true;
        } else if (c < '0' || c > '9') {
            return false;
        }
    }
    return true;
}

/**
 * The number `text` spells, or nothing when it does not spell one.
 *
 * `from_chars` rather than `strtod` because the decimal point is a locale
 * decision everywhere else and must not be one here: the same string has to
 * mean the same value on a machine set to German, where `strtod` reads `1.5`
 * as `1`.
 *
 * A leading `+` and a trailing `.` are trimmed first — both are legal to have
 * typed so far, and neither changes the value.
 */
std::optional<double> numberIn(std::string_view text) {
    if (!text.empty() && text.front() == '+') text.remove_prefix(1);
    if (!text.empty() && text.back() == '.') text.remove_suffix(1);
    if (text.empty()) return std::nullopt;

    double value = 0.0;
    const char* const first = text.data();
    const char* const last = first + text.size();
    const std::from_chars_result parsed = std::from_chars(first, last, value);
    if (parsed.ec != std::errc{} || parsed.ptr != last) return std::nullopt;
    return value;
}

}  // namespace

TextInputResult textInput(Ui& ui, const Interaction& input, std::string_view id,
                          TextEditState& state, const TextInputOptions& options) {
    const bool hovered = input.isHovered(id);
    const bool focused = input.isFocused(id);
    const bool editable = !options.disabled && !options.readOnly;
    const bool number = options.type == InputType::Number;
    const std::string runId = std::string(id) + ".run";
    const std::string decrementId = std::string(id) + ".decrement";
    const std::string incrementId = std::string(id) + ".increment";

    const FieldPalette palette = paletteForField(options.disabled, options.readOnly, hovered);

    // What is drawn, which is not always what is stored: a password shows
    // bullets, and an empty box shows its placeholder.
    // A revealed password is just text: the bullets are a view, not the value,
    // and every measurement below has to agree about which one is on screen.
    const bool masked = options.type == InputType::Password && !options.revealed;
    const auto shownFor = [&](const std::string& text) -> std::string {
        // The placeholder steps aside once the box has the keyboard, leaving
        // the caret alone in an empty box, and comes back on blur if nothing
        // was typed. A hint you are being asked to type over is noise the
        // moment you start.
        if (text.empty()) {
            return focused && editable ? std::string{} : std::string(options.placeholder);
        }
        return masked ? bulletsFor(text) : text;
    };

    TextStyle runStyle;
    runStyle.overflow = TextOverflow::Clip;

    // ---- the pointer places the caret --------------------------------------
    // The run's rectangle is last frame's: absolute positions are in the
    // viewport and layout has not run yet. It is also the rectangle the user
    // was pointing at, which is the one that should answer.
    const Rect runFrame = input.frameOf(runId);
    if (editable && !state.text.empty() && runFrame.width > 0.0f &&
        (input.pressStarted(id) || input.dragging() == id)) {
        const float x = input.pointer().x - runFrame.x;
        // A password maps the click against the bullets and then counts that
        // many characters into the secret, because the two are not the same
        // number of bytes.
        const std::size_t at =
            masked
                ? characterAt(state.text, countCharacters(shownFor(state.text), offsetAtX(
                                              ui, shownFor(state.text), runStyle, x)))
                : offsetAtX(ui, state.text, runStyle, x);
        state.caret = at;
        // The press collapses the selection where it landed; every frame after
        // it drags one end of a selection out from there.
        if (input.pressStarted(id)) state.anchor = at;
    }

    // ---- the edit ----------------------------------------------------------
    // Kept so a number box can put the text back: the cheapest correct filter
    // is to let the shared editing model do the work and then refuse the
    // result, rather than a second implementation of insert-with-a-predicate
    // that would have to know about selections and multi-byte characters.
    const std::string beforeEdit = state.text;
    const std::size_t caretBefore = state.caret;
    const std::size_t anchorBefore = state.anchor;

    TextInputResult result;
    if (focused && editable) {
        static_cast<TextEditResult&>(result) = applyInput(state, input.keys(), input.text());
    }
    if (number && !isPartialNumber(state.text, options.minimum < 0.0, options.decimals > 0)) {
        state.text = beforeEdit;
        state.caret = caretBefore;
        state.anchor = anchorBefore;
        result.changed = false;
    }
    state.clampToText();

    // ---- stepping ----------------------------------------------------------
    //
    // The arrows, the wheel and the two buttons are one delta, resolved before
    // anything is built: `clicked` answers from last frame's tree, so the box
    // can draw the value the button has just produced instead of the one it had
    // before it was pressed.
    //
    // Home and End are *not* here, though the old `numberField` sent them to
    // the bounds. This takes typing now, and in anything that takes typing they
    // belong to the caret; a box where End jumps to the maximum instead of the
    // end of what you are typing is a box that fights you.
    const double snap = options.decimals > 0 ? 0.0 : 1.0;
    if (number && editable) {
        double delta = 0.0;
        if (focused) {
            for (const KeyEvent& event : input.keys()) {
                if (event.key == Key::Up) delta += options.step;
                if (event.key == Key::Down) delta -= options.step;
            }
        }
        // The wheel steps the box under the pointer, which is what makes a spin
        // box usable without clicking into it first.
        if (hovered && input.wheel() != 0.0f) {
            delta += options.step * static_cast<double>(input.wheel());
        }
        if (input.clicked(incrementId)) delta += options.step;
        if (input.clicked(decrementId)) delta -= options.step;

        if (delta != 0.0) {
            const double from = numberIn(state.text).value_or(0.0);
            const double stepped =
                clampAndSnap(from + delta, options.minimum, options.maximum, snap);
            state.text = formatNumber(stepped, options.decimals, {});
            state.moveToEnd();
            result.changed = true;
        }
    }

    // ---- blur normalises ---------------------------------------------------
    //
    // The one moment the text and the value are allowed to disagree without
    // confusing anybody. While the box has the keyboard the text is the truth
    // and nothing rewrites it; once it does not, `500` in a box that stops at
    // `60` has to become `60`, and `-` has to become nothing at all.
    //
    // An empty box stays empty. A number box with nothing in it has no value,
    // and there is no other way for it to say so — inventing a zero on blur is
    // how a form quietly fills itself in.
    if (number && editable && !focused) {
        const std::optional<double> parsed = numberIn(state.text);
        const std::string normalised =
            parsed ? formatNumber(clampAndSnap(*parsed, options.minimum, options.maximum, snap),
                                  options.decimals, {})
                   : std::string{};
        if (state.text != normalised) {
            state.text = normalised;
            state.moveToEnd();
            result.changed = true;
        }
    }

    if (number) {
        if (const std::optional<double> parsed = numberIn(state.text)) {
            result.hasValue = true;
            result.value = clampAndSnap(*parsed, options.minimum, options.maximum, snap);
        }
    }

    // A box narrower than the side-by-side form needs stacks instead: the value
    // squeezing to "7…" between two buttons is worse than smaller arrows.
    StepperPlacement steppers = number ? options.steppers : StepperPlacement::None;
    if (steppers == StepperPlacement::Sides) {
        const Rect known = input.frameOf(id);
        const float measuredWidth = known.width > 0.0f ? known.width : options.width;
        if (measuredWidth < options.stackedBelow) steppers = StepperPlacement::Stacked;
    }
    if (!editable) steppers = StepperPlacement::None;

    // Recomputed after the edit, not before: this frame draws what the user has
    // just typed.
    const bool empty = state.text.empty();
    const std::string shown = shownFor(state.text);
    runStyle.color = Fill{empty ? Token::TextMuted : palette.label};

    const float controlHeight =
        options.height > 0.0f ? options.height : ui.design().controlHeight;

    Style box;
    box.direction = Direction::Row;
    box.align = Align::Center;
    box.minHeight = controlHeight;
    // A number box that fits its digits changes width as they are typed — 5,
    // then 50, then 500 — and the steppers walk out from under the pointer that
    // is clicking them. So an unspecified width is a fixed one here, and only
    // here; `kAuto` is a NaN, which is why this is a call and not a comparison.
    box.width = number && std::isnan(options.width) ? 120.0f : options.width;
    box.grow = options.grow;
    // A box never squeezes below something readable; the row overflows first.
    box.minWidth = number ? 72.0f : 64.0f;
    // The steppers bring their own breathing room, so the trailing padding that
    // a plain box wants would push them off the edge.
    box.padding = steppers == StepperPlacement::None ? Edges::symmetric(0.0f, 10.0f)
                                                     : Edges::symmetric(0.0f, 4.0f);
    box.gap = steppers == StepperPlacement::None ? 6.0f : 2.0f;
    box.background = palette.background;
    box.border = Border{1.0f, Fill{options.invalid ? Token::Removed : palette.border}};
    // `isFocused`, not `isFocusVisible`: a box that takes typing shows its ring
    // however focus arrived. This is the exception `:focus-visible` names, and
    // the reason is the caret — a box that will swallow the next keystroke has
    // to say so even when the user clicked into it.
    if (focused && editable) {
        box.outline = Outline{2.0f, 2.0f, Fill{options.invalid ? Token::Removed : Token::Accent}};
    }
    box.overflow = Overflow::Hidden;
    box.cursorHint = editable ? Cursor::Text : Cursor::Default;

    auto scope = ui.scope(box);
    ui.tag(id).focusable(!options.disabled).cursor(box.cursorHint);
    // `SpinButton` for a number, `TextInput` for the other two: ARIA draws the
    // line in the same place, and a reader on a spin button is told the arrows
    // are there. The password form has no role of its own anywhere — a screen
    // reader learns it is a password from the platform, which is stage 5's job
    // and not something a role can say.
    //
    // The placeholder is the description and never the name. A box named by its
    // placeholder is a box that loses its name the moment somebody types in it,
    // which is the single most common way this is got wrong on the web.
    ui.accessible({
        .role = number ? Role::SpinButton : Role::TextInput,
        .name = options.name,
        .description = options.placeholder,
        .state = {.disabled = flag(options.disabled),
                  .readOnly = flag(options.readOnly),
                  .invalid = flag(options.invalid)},
        // A number carries its bounds; the other two have none, which
        // `minimum == maximum` says. A password reports no value at all — the
        // whole point of the bullets is that the string is not on offer, and a
        // tree that carried it would hand back what the screen refuses to show.
        .value = {.present = options.type != InputType::Password,
                  .now = number ? result.value : 0.0,
                  .minimum = number ? options.minimum : 0.0,
                  .maximum = number ? options.maximum : 0.0,
                  .text = state.text},
    });

    if (options.leading) {
        icon(ui, *options.leading, {.color = Token::TextMuted, .size = 14.0f});
    }

    /** One step button: a target, a glyph and a delta the click already
     *  produced above, so this only has to draw. */
    const auto stepButton = [&](const std::string& buttonId, Icon glyph, float width,
                                float height) {
        Style button;
        button.width = width;
        button.height = height;
        button.shrink = 0.0f;
        button.justify = Justify::Center;
        button.align = Align::Center;
        button.radius = 3.0f;
        button.background = input.isHovered(buttonId) ? Fill{Token::SurfaceHover} : Fill{};
        button.cursorHint = Cursor::Pointer;
        auto buttonScope = ui.scope(button);
        ui.tag(buttonId).cursor(Cursor::Pointer);
        icon(ui, glyph, {.color = Token::TextMuted, .size = std::min(12.0f, height - 2.0f)});
        (void)buttonScope;
    };

    if (steppers == StepperPlacement::Sides) {
        stepButton(decrementId, Icon::Minus, 20.0f, controlHeight - 8.0f);
    }

    {
        // The run and the caret share a box, so the caret can be placed at a
        // measured offset inside it rather than after the whole string. The box
        // is tagged for its rectangle alone — `ignoresPointer`, so a click on
        // the text still belongs to the box around it.
        //
        // The line's height is needed *before* the box is opened so the box can
        // be given it. An empty run measures zero high, and a zero-high box
        // centres the caret at minus half its own height — outside the box,
        // where this node's own clip removes it. That is why an empty field
        // that had the keyboard showed nothing at all: the caret was built,
        // positioned and then clipped away.
        const float lineHeight =
            ui.canMeasure() ? ui.measure("Ag", runStyle).height : controlHeight * 0.7f;

        Style field;
        field.grow = 1.0f;
        field.basis = 0.0f;
        field.align = Align::Center;
        field.minHeight = lineHeight;
        field.overflow = Overflow::Hidden;
        auto fieldScope = ui.scope(field);
        ui.tag(runId).ignoresPointer();
        ui.label(shown, runStyle);

        // The unit sits against the number rather than at the far side of the
        // box, and it is outside the editable text on purpose: a suffix inside
        // it is a suffix the caret can be put in the middle of, and " min" is
        // not something a reader should be able to delete one letter of.
        //
        // Not drawn over a placeholder — "Repository name min" is nonsense, and
        // an empty box has no value for a unit to qualify.
        if (number && !options.suffix.empty() && !empty) {
            text(ui, options.suffix, {.color = Token::TextMuted});
        }

        // Both marks are `Position::Absolute`, which is measured from **this
        // box's content box** and not from the window. Adding the run's own
        // origin, as this did, pushed the caret and the selection a whole pane
        // to the right of the text they belong to, where the box's own clip
        // swallowed them. Only the run's *size* comes from the last frame.
        //
        // The caret is sized from the *text*, not from the control, so it still
        // fits the line when the type is 22 px and the box grew to suit. Reading
        // last frame's box height made the caret's size depend on geometry that
        // lags a frame, so anything that nudged the box — a resize, a font
        // change, the row growing — resized the caret for a frame or two and it
        // visibly pulsed. A caret is the height of the line it sits in, which is
        // a property of the type and of nothing else.
        const float caretHeight = lineHeight * 0.92f;
        // Centred in whichever is real: the measured box once there is one, the
        // line height on the first frame or when the run is empty.
        const float lineBox = std::max(runFrame.height, lineHeight);
        const float markTop = (lineBox - caretHeight) / 2.0f;

        if (focused && editable && runFrame.width > 0.0f) {
            // Where the caret sits: the width of everything before it. The
            // password form measures bullets, not the secret.
            const std::string prefix =
                masked ? bulletsFor(std::string_view(state.text).substr(0, state.caret))
                       : state.text.substr(0, state.caret);
            const float offset = ui.canMeasure() ? ui.measure(prefix, runStyle).width : 0.0f;

            if (state.hasSelection()) {
                const std::string before =
                    masked ? bulletsFor(
                                 std::string_view(state.text).substr(0, state.selectionStart()))
                           : state.text.substr(0, state.selectionStart());
                const std::string inside =
                    masked ? bulletsFor(state.selectedText()) : std::string(state.selectedText());
                const float start = ui.canMeasure() ? ui.measure(before, runStyle).width : 0.0f;
                const float width = ui.canMeasure() ? ui.measure(inside, runStyle).width : 0.0f;
                Style selection;
                selection.position = Position::Absolute;
                selection.left = start;
                selection.top = markTop;
                selection.width = width;
                selection.height = caretHeight;
                selection.radius = 2.0f;
                selection.background = Fill{Token::Accent, 0.30f};
                ui.add(selection);
            }

            // The blink, and the reason it is a clock rather than an animation:
            // it loops and never arrives. The phase is measured from the last
            // time anything happened — typing, or moving the caret — so the bar
            // is solid while the reader works and only starts flashing once they
            // stop, which is what every editor does and what stops a caret
            // vanishing mid-keystroke.
            constexpr float kBlinkPeriod = 1.06f;
            // "Something happened" is: text arrived, a key was pressed, the
            // pointer is dragging a selection, or the caret moved at all.
            const float caretNow = static_cast<float>(state.caret);
            const bool moved = ui.latch(id, "caret.pos", caretNow, false) != caretNow;
            ui.latch(id, "caret.pos", caretNow, true);
            const bool active = moved || !input.text().empty() || !input.keys().empty() ||
                                input.dragging() == id;
            const float since = ui.now() - ui.latch(id, "caret.beat", ui.now(), active);
            const bool visible = !ui.animator() || std::fmod(since, kBlinkPeriod) <
                                                       kBlinkPeriod / 2.0f;

            if (visible) {
                // A caret is a hairline, and hairlines have to land on whole
                // pixels.
                //
                // At 1.5 px on a fractional x it is antialiased across two
                // columns, and how much falls in each depends on where the text
                // before it happened to end. Typing one character moves it by a
                // fraction, the split changes, and the bar looks a pixel wider —
                // then narrower on the next keystroke. Snapping the *absolute*
                // position (the parent's origin plus the offset) and using a
                // whole number of pixels makes it identical wherever it lands.
                const float caretWidth = std::max(1.0f, std::round(lineHeight / 14.0f));
                const float snapped = std::round(runFrame.x + offset) - runFrame.x;

                Style caret;
                caret.position = Position::Absolute;
                caret.left = snapped;
                caret.top = markTop;
                caret.width = caretWidth;
                caret.height = caretHeight;
                caret.radius = 0.0f;
                caret.background = Fill{Token::Accent};
                ui.add(caret);
            }
        }
        (void)fieldScope;
    }

    if (steppers == StepperPlacement::Sides) {
        stepButton(incrementId, Icon::Plus, 20.0f, controlHeight - 8.0f);
    } else if (steppers == StepperPlacement::Stacked) {
        // Two half-height arrows on the right: the classic spin box, and the
        // narrowest arrangement that still affords clicking.
        //
        // The top one points *up*. `numberField` drew a `ChevronDown` for both,
        // which shipped: a spin box whose increment points downwards reads as
        // two ways to go down.
        Style column;
        column.direction = Direction::Column;
        column.width = 16.0f;
        column.shrink = 0.0f;
        column.gap = 1.0f;
        auto columnScope = ui.scope(column);
        const float half = (controlHeight - 10.0f) / 2.0f;
        stepButton(incrementId, Icon::ChevronUp, 16.0f, half);
        stepButton(decrementId, Icon::ChevronDown, 16.0f, half);
        (void)columnScope;
    }

    // ---- the eye -----------------------------------------------------------
    // Trailing, and only on a password: it is a view control, so it sits at the
    // end of the box rather than beside it, and it never takes the keyboard —
    // Tab through a form should not stop on a reveal button.
    if (options.type == InputType::Password && options.revealToggle && !options.disabled) {
        const std::string eyeId = std::string(id) + ".reveal";
        Style eye;
        eye.width = 22.0f;
        eye.height = 22.0f;
        eye.shrink = 0.0f;
        eye.justify = Justify::Center;
        eye.align = Align::Center;
        eye.radius = 4.0f;
        if (input.isHovered(eyeId)) eye.background = Fill{Token::SurfaceHover};
        eye.cursorHint = Cursor::Pointer;
        auto eyeScope = ui.scope(eye);
        ui.tag(eyeId).cursor(Cursor::Pointer);
        icon(ui, options.revealed ? Icon::EyeOff : Icon::Eye,
             {.color = options.revealed ? Token::Text : Token::TextMuted, .size = 15.0f});
        (void)eyeScope;
        if (input.clicked(eyeId)) result.toggledReveal = true;
    }
    (void)scope;

    return result;
}

}  // namespace gbui
