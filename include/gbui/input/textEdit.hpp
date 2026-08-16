// The editing model behind a text field.
//
// It is deliberately separate from the component that draws one: editing is
// where the fiddly, testable logic lives — caret movement over UTF-8, selection
// anchors, word jumps — and none of it needs a window to be exercised.
//
// Offsets are byte offsets into UTF-8. Every operation here keeps them on a
// character boundary, so a caret can never land inside a multi-byte sequence.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "gbui/input/keys.hpp"

namespace gbui {

/** A field's contents and where the cursor is in them. Owned by the
 *  application: the toolkit holds no state of its own. */
struct TextEditState {
    std::string text;
    /** Where the caret is, in bytes. */
    std::size_t caret = 0;
    /** The other end of the selection. Equal to `caret` when nothing is
     *  selected, which is why there is no separate "has selection" flag. */
    std::size_t anchor = 0;

    bool hasSelection() const { return caret != anchor; }
    std::size_t selectionStart() const { return caret < anchor ? caret : anchor; }
    std::size_t selectionEnd() const { return caret < anchor ? anchor : caret; }
    std::string_view selectedText() const {
        return std::string_view(text).substr(selectionStart(), selectionEnd() - selectionStart());
    }

    /** Puts the caret at the end and drops any selection. */
    void moveToEnd() { caret = anchor = text.size(); }
    void selectAll() {
        anchor = 0;
        caret = text.size();
    }
    void clampToText();
};

/** What an edit did, so a caller can react without diffing the string. */
struct TextEditResult {
    bool changed = false;   ///< the text is different
    bool moved = false;     ///< only the caret or selection moved
    bool submitted = false; ///< Return was pressed
    bool cancelled = false; ///< Escape was pressed
};

/**
 * How the model reads the keys, which is the only thing one line and many have
 * to disagree about.
 */
struct TextEditOptions {
    /**
     * Return inserts a newline instead of submitting; Home and End go to the
     * ends of the *line*; Up and Down move between lines.
     *
     * A mode rather than a second model, because everything else — offsets on
     * character boundaries, word jumps, selection anchors — is identical, and a
     * copy of it would be a copy to keep in step.
     *
     * **Lines here are hard lines**, the ones a `\n` makes, and not the lines a
     * box happens to wrap the text into. Visual-line movement needs the wrap,
     * which is decided in layout and does not exist yet when the keys are
     * applied; `richEditor` has the same limit and says so. In a box wide
     * enough for the text it is the same thing, and in a narrow one Up and Down
     * skip a wrapped row.
     */
    bool multiline = false;
    /**
     * Submits on Ctrl+Return (Cmd+Return on macOS) while `multiline` is set.
     *
     * A multi-line box has to give Return to the text, so the gesture that
     * means "done" moves to the modifier — which is what every commit message
     * box, chat composer and comment field does.
     */
    bool submitOnModifiedReturn = true;
};

/** Inserts text at the caret, replacing the selection. */
TextEditResult insertText(TextEditState& state, std::string_view text);

/** Applies one key. Shift extends the selection instead of collapsing it. */
TextEditResult applyKey(TextEditState& state, const KeyEvent& event,
                        const TextEditOptions& options = {});

/** Applies a whole frame of input: every key, then any typed text. */
TextEditResult applyInput(TextEditState& state, const std::vector<KeyEvent>& keys,
                          std::string_view typed, const TextEditOptions& options = {});

// ---- lines, for a box that has more than one -------------------------------

/** The offset just after the newline before `offset`, or 0. */
std::size_t lineStart(std::string_view text, std::size_t offset);
/** The offset of the newline at or after `offset`, or the end. */
std::size_t lineEnd(std::string_view text, std::size_t offset);

// ---- offsets, on character boundaries -------------------------------------

/** The offset one character before `offset`, or 0. */
std::size_t previousCharacter(std::string_view text, std::size_t offset);
/** The offset one character after `offset`, or the end. */
std::size_t nextCharacter(std::string_view text, std::size_t offset);
/** The start of the word at or before `offset` — what Ctrl+Left reaches. */
std::size_t previousWord(std::string_view text, std::size_t offset);
/** The end of the word at or after `offset`. */
std::size_t nextWord(std::string_view text, std::size_t offset);

}  // namespace gbui
