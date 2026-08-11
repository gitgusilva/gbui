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

/** Inserts text at the caret, replacing the selection. */
TextEditResult insertText(TextEditState& state, std::string_view text);

/** Applies one key. `select` extends the selection instead of collapsing it,
 *  which is what Shift does everywhere. */
TextEditResult applyKey(TextEditState& state, const KeyEvent& event);

/** Applies a whole frame of input: every key, then any typed text. */
TextEditResult applyInput(TextEditState& state, const std::vector<KeyEvent>& keys,
                          std::string_view typed);

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
