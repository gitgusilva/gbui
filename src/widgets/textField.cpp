#include "gbui/widgets/textField.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>

#include "detail.hpp"
#include "gbui/widgets/icon.hpp"

namespace gbui {

// What this shares with its siblings, rather than a copy in each.
using namespace detail;

TextFieldResult textField(Ui& ui, const Interaction& input, std::string_view id,
                          TextEditState& state, const TextFieldOptions& options) {
    const bool hovered = input.isHovered(id);
    const bool focused = input.isFocused(id);
    const bool editable = !options.disabled && !options.readOnly;
    const std::string runId = std::string(id) + ".run";

    const FieldPalette palette = paletteForField(options.disabled, options.readOnly, hovered);

    // What is drawn, which is not always what is stored: a password field shows
    // bullets, and an empty one shows its placeholder.
    // A revealed password is just text: the bullets are a view, not the value,
    // and every measurement below has to agree about which one is on screen.
    const bool masked = options.password && !options.revealed;
    const auto shownFor = [&](const std::string& text) -> std::string {
        // The placeholder steps aside once the field has the keyboard, leaving
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

    TextFieldResult result;
    if (focused && editable) {
        static_cast<TextEditResult&>(result) = applyInput(state, input.keys(), input.text());
    }
    state.clampToText();

    // Recomputed after the edit, not before: this frame draws what the user has
    // just typed.
    const bool empty = state.text.empty();
    const std::string shown = shownFor(state.text);
    runStyle.color = Fill{empty ? Token::TextMuted : palette.label};

    Style box;
    box.direction = Direction::Row;
    box.align = Align::Center;
    box.minHeight = options.height;
    box.width = options.width;
    box.grow = options.grow;
    // A field never squeezes below something readable; the row overflows first.
    box.minWidth = 64.0f;
    box.padding = Edges::symmetric(0.0f, 10.0f);
    box.gap = 6.0f;
    box.background = palette.background;
    box.border = Border{1.0f, Fill{palette.border}};
    // `isFocused`, not `isFocusVisible`: a field that takes typing shows its
    // ring however focus arrived. This is the exception `:focus-visible` names,
    // and the reason is the caret — a box that will swallow the next keystroke
    // has to say so even when the user clicked into it.
    if (focused && editable) box.outline = Outline{2.0f, 2.0f, Fill{Token::Accent}};
    box.overflow = Overflow::Hidden;
    box.cursorHint = editable ? Cursor::Text : Cursor::Default;

    auto scope = ui.begin(box);
    ui.tag(id).focusable(!options.disabled).cursor(box.cursorHint);

    if (options.leading) {
        icon(ui, *options.leading, {.color = Token::TextMuted, .size = 14.0f});
    }

    {
        // The run and the caret share a box, so the caret can be placed at a
        // measured offset inside it rather than after the whole string. The box
        // is tagged for its rectangle alone \u2014 `ignoresPointer`, so a click on
        // the text still belongs to the field.
        // The line's height, needed *before* the box is opened so the box can
        // be given it. An empty run measures zero high, and a zero-high box
        // centres the caret at minus half its own height — outside the box,
        // where this node's own clip removes it. That is why an empty field
        // that had the keyboard showed nothing at all: the caret was built,
        // positioned and then clipped away.
        const float lineHeight =
            ui.canMeasure() ? ui.measure("Ag", runStyle).height : options.height * 0.7f;

        Style field;
        field.grow = 1.0f;
        field.basis = 0.0f;
        field.align = Align::Center;
        field.minHeight = lineHeight;
        field.overflow = Overflow::Hidden;
        auto fieldScope = ui.begin(field);
        ui.tag(runId).ignoresPointer();
        ui.label(shown, runStyle);

        // Both marks are `Position::Absolute`, which is measured from **this
        // box's content box** and not from the window. Adding the run's own
        // origin, as this did, pushed the caret and the selection a whole pane
        // to the right of the text they belong to, where the field's own clip
        // swallowed them. Only the run's *size* comes from the last frame.
        // The caret is sized from the *text*, not from the control, so it
        // still fits the line when the type is 22 px and the box grew to suit.
        // Sized from the font alone, never from `runFrame`.
        //
        // Reading last frame's box height made the caret's *size* depend on
        // geometry that lags a frame, so anything that nudged the box — a
        // resize, a font change, the row growing — resized the caret for a
        // frame or two and it visibly pulsed. A caret is the height of the line
        // it sits in, which is a property of the type and of nothing else.
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

            // The blink, and the reason it is a clock rather than an
            // animation: it loops and never arrives. The phase is measured from
            // the last time anything happened — typing, or moving the caret —
            // so the bar is solid while the reader works and only starts
            // flashing once they stop, which is what every editor does and what
            // stops a caret vanishing mid-keystroke.
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

    // ---- the eye -----------------------------------------------------------
    // Trailing, and only on a password: it is a view control, so it sits at the
    // end of the field rather than beside it, and it never takes the keyboard —
    // Tab through a form should not stop on a reveal button.
    if (options.password && options.revealToggle && !options.disabled) {
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
        auto eyeScope = ui.begin(eye);
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
