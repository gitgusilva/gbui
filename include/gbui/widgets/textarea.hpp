// A multi-line plain text box.
//
// The gap between `textInput` and `richEditor`, and it is a wide one: a commit
// message, a description, a note. Plain text, so the value is a `std::string`
// and stays one — nothing here has marks, blocks or a document model, and a
// caller who wants those wants `richEditor`.
//
// Lines are the ones a `\n` makes. The box wraps long ones for reading, but Up,
// Down, Home and End move by *hard* line, because visual lines are decided in
// layout and the keys are applied before it. `richEditor` has the same limit
// and for the same reason; in a box wide enough for the text there is no
// difference, and in a narrow one the arrows skip a wrapped row.
#pragma once

#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/input/textEdit.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/scroll.hpp"

namespace gbui {

/** The text, and where the box is scrolled to. Owned by the application, like
 *  every other piece of state here — the same pairing `RichEditorState` makes,
 *  because a box with more lines than it can show has both. */
struct TextareaState {
    TextEditState edit{};
    ScrollState view{};
};

struct TextareaOptions {
    std::string_view placeholder{};
    bool disabled = false;
    bool readOnly = false;
    /**
     * How many lines tall the box is, before anything is typed.
     *
     * A count of lines rather than a height in pixels, because that is the unit
     * the answer is in — "about four lines" survives a change of type size and
     * "96 px" does not.
     */
    int rows = 4;
    /**
     * Grows with the content, to this many lines, then scrolls.
     *
     * Zero keeps the box at `rows` from the first frame, which is what a form
     * with a fixed layout wants. A chat composer or a commit message box wants
     * this set: the box starts small and opens up as the writing goes on, which
     * is the behaviour everyone now expects and nobody asks for.
     */
    int maxRows = 0;
    /** Ctrl+Return — Cmd+Return on macOS — reports `submitted`. Return itself
     *  belongs to the text. */
    bool submitOnModifiedReturn = true;
    float width = kAuto;
    float grow = 0.0f;
};

/**
 * Edits `state` in place when it has focus, and reports what happened.
 *
 * `submitted` is the modified Return; `changed` is any edit at all. The
 * component never writes to your model — the text in `state` is yours and this
 * is the only thing that touches it.
 *
 * The view follows the caret: typing at the bottom of a full box scrolls it,
 * and so does moving there with the arrows. Nothing else moves it on its own,
 * so a reader who has scrolled up to check something stays where they put
 * themselves until they type again.
 */
TextEditResult textarea(Ui& ui, const Interaction& input, std::string_view id,
                        TextareaState& state, const TextareaOptions& options = {});

}  // namespace gbui
