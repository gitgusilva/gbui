#include "gbui/input/textEdit.hpp"

#include <algorithm>
#include <cctype>
#include <vector>

namespace gbui {
namespace {

/** A continuation byte is anything of the form 10xxxxxx; a caret must never
 *  come to rest on one. */
bool isContinuation(char c) {
    return (static_cast<unsigned char>(c) & 0xC0U) == 0x80U;
}

bool isWordCharacter(char c) {
    const auto value = static_cast<unsigned char>(c);
    // Bytes above ASCII belong to a multi-byte character; treating them as word
    // characters keeps accented words whole under Ctrl+Left.
    return value >= 0x80 || std::isalnum(value) != 0 || c == '_';
}

void deleteSelection(TextEditState& state) {
    const std::size_t start = state.selectionStart();
    const std::size_t end = state.selectionEnd();
    state.text.erase(start, end - start);
    state.caret = state.anchor = start;
}

}  // namespace

void TextEditState::clampToText() {
    caret = std::min(caret, text.size());
    anchor = std::min(anchor, text.size());
    while (caret > 0 && caret < text.size() && isContinuation(text[caret])) --caret;
    while (anchor > 0 && anchor < text.size() && isContinuation(text[anchor])) --anchor;
}

std::size_t previousCharacter(std::string_view text, std::size_t offset) {
    if (offset == 0) return 0;
    std::size_t index = offset - 1;
    while (index > 0 && isContinuation(text[index])) --index;
    return index;
}

std::size_t nextCharacter(std::string_view text, std::size_t offset) {
    if (offset >= text.size()) return text.size();
    std::size_t index = offset + 1;
    while (index < text.size() && isContinuation(text[index])) ++index;
    return index;
}

std::size_t previousWord(std::string_view text, std::size_t offset) {
    std::size_t index = offset;
    // Step over the run of separators first, then over the word itself, which
    // is what puts the caret at the start of the word rather than after it.
    while (index > 0 && !isWordCharacter(text[previousCharacter(text, index)])) {
        index = previousCharacter(text, index);
    }
    while (index > 0 && isWordCharacter(text[previousCharacter(text, index)])) {
        index = previousCharacter(text, index);
    }
    return index;
}

std::size_t nextWord(std::string_view text, std::size_t offset) {
    std::size_t index = offset;
    while (index < text.size() && isWordCharacter(text[index])) index = nextCharacter(text, index);
    while (index < text.size() && !isWordCharacter(text[index])) index = nextCharacter(text, index);
    return index;
}

std::size_t lineStart(std::string_view text, std::size_t offset) {
    offset = std::min(offset, text.size());
    const std::size_t newline = text.rfind('\n', offset == 0 ? 0 : offset - 1);
    return newline == std::string_view::npos || offset == 0 ? 0 : newline + 1;
}

std::size_t lineEnd(std::string_view text, std::size_t offset) {
    const std::size_t newline = text.find('\n', std::min(offset, text.size()));
    return newline == std::string_view::npos ? text.size() : newline;
}

TextEditResult insertText(TextEditState& state, std::string_view text) {
    if (text.empty()) return {};
    state.clampToText();
    if (state.hasSelection()) deleteSelection(state);
    state.text.insert(state.caret, text);
    state.caret += text.size();
    state.anchor = state.caret;
    return {.changed = true};
}

TextEditResult applyKey(TextEditState& state, const KeyEvent& event,
                        const TextEditOptions& options) {
    state.clampToText();
    const Modifiers& modifiers = event.modifiers;
    const bool extend = modifiers.shift;
    const bool byWord = modifiers.ctrl || modifiers.alt;

    /** Moves the caret, taking the selection with it or collapsing it. */
    const auto moveTo = [&](std::size_t offset) -> TextEditResult {
        state.caret = offset;
        if (!extend) state.anchor = offset;
        return {.moved = true};
    };

    // ---- the keys one line and many disagree about -------------------------
    //
    // Handled before the shared switch rather than inside it, so the
    // single-line path below is exactly the code it always was.
    if (options.multiline) {
        // How many *characters* into its line the caret is. Characters and not
        // bytes, because the column is carried to a line whose bytes are a
        // different length — an accented line and a plain one of the same
        // apparent width do not agree about byte counts.
        const auto columnOf = [&](std::size_t offset) {
            const std::size_t start = lineStart(state.text, offset);
            std::size_t column = 0;
            for (std::size_t at = start; at < offset; at = nextCharacter(state.text, at)) ++column;
            return column;
        };
        /** The offset `column` characters into the line starting at `start`,
         *  clamped to that line's end — a short line takes the caret to its
         *  end rather than into the next one. */
        const auto offsetInLine = [&](std::size_t start, std::size_t column) {
            const std::size_t end = lineEnd(state.text, start);
            std::size_t at = start;
            for (std::size_t i = 0; i < column && at < end; ++i) {
                at = nextCharacter(state.text, at);
            }
            return std::min(at, end);
        };

        switch (event.key) {
            case Key::Home:
                return moveTo(lineStart(state.text, state.caret));
            case Key::End:
                return moveTo(lineEnd(state.text, state.caret));

            case Key::Up: {
                const std::size_t start = lineStart(state.text, state.caret);
                // On the first line Up goes to the very beginning, the way it
                // does everywhere: a caret that refuses to move is read as the
                // key not working.
                if (start == 0) return moveTo(0);
                const std::size_t column = columnOf(state.caret);
                return moveTo(offsetInLine(lineStart(state.text, start - 1), column));
            }
            case Key::Down: {
                const std::size_t end = lineEnd(state.text, state.caret);
                if (end >= state.text.size()) return moveTo(state.text.size());
                const std::size_t column = columnOf(state.caret);
                return moveTo(offsetInLine(end + 1, column));
            }

            case Key::Return:
                // The modifier is what submits here, because Return belongs to
                // the text. Without the modifier — or with it turned off — a
                // newline is inserted like any other character.
                if (options.submitOnModifiedReturn && modifiers.command()) {
                    return {.submitted = true};
                }
                return insertText(state, "\n");

            default:
                break;
        }
    }

    switch (event.key) {
        case Key::Left:
            if (state.hasSelection() && !extend) return moveTo(state.selectionStart());
            return moveTo(byWord ? previousWord(state.text, state.caret)
                                 : previousCharacter(state.text, state.caret));
        case Key::Right:
            if (state.hasSelection() && !extend) return moveTo(state.selectionEnd());
            return moveTo(byWord ? nextWord(state.text, state.caret)
                                 : nextCharacter(state.text, state.caret));
        case Key::Home:
            return moveTo(0);
        case Key::End:
            return moveTo(state.text.size());

        case Key::Backspace:
            if (state.hasSelection()) {
                deleteSelection(state);
                return {.changed = true};
            }
            if (state.caret == 0) return {};
            {
                const std::size_t from =
                    byWord ? previousWord(state.text, state.caret)
                           : previousCharacter(state.text, state.caret);
                state.text.erase(from, state.caret - from);
                state.caret = state.anchor = from;
            }
            return {.changed = true};

        case Key::Delete:
            if (state.hasSelection()) {
                deleteSelection(state);
                return {.changed = true};
            }
            if (state.caret >= state.text.size()) return {};
            {
                const std::size_t to = byWord ? nextWord(state.text, state.caret)
                                              : nextCharacter(state.text, state.caret);
                state.text.erase(state.caret, to - state.caret);
            }
            return {.changed = true};

        case Key::A:
            if (!modifiers.command()) return {};
            state.selectAll();
            return {.moved = true};

        case Key::X:
            // Cut without a clipboard is still a delete; the platform hands the
            // text over separately when it has one.
            if (!modifiers.command() || !state.hasSelection()) return {};
            deleteSelection(state);
            return {.changed = true};

        case Key::Return:
            return {.submitted = true};
        case Key::Escape:
            return {.cancelled = true};

        default:
            return {};
    }
}

TextEditResult applyInput(TextEditState& state, const std::vector<KeyEvent>& keys,
                          std::string_view typed, const TextEditOptions& options) {
    TextEditResult result;
    for (const KeyEvent& event : keys) {
        const TextEditResult one = applyKey(state, event, options);
        result.changed |= one.changed;
        result.moved |= one.moved;
        result.submitted |= one.submitted;
        result.cancelled |= one.cancelled;
    }
    if (!typed.empty()) {
        // Control characters arrive as keys, not as text; letting them through
        // here would put a literal tab or newline in a single-line field.
        //
        // A newline is the exception once the box has more than one line, and
        // it is not a theoretical one: a paste arrives as typed text, so
        // filtering it here is what silently turns three pasted lines into one
        // long one. Return still comes through as a key either way.
        std::string printable;
        printable.reserve(typed.size());
        for (const char c : typed) {
            if (c == '\n' && options.multiline) {
                printable.push_back(c);
            } else if (static_cast<unsigned char>(c) >= 0x20) {
                printable.push_back(c);
            }
        }
        const TextEditResult one = insertText(state, printable);
        result.changed |= one.changed;
    }
    return result;
}

}  // namespace gbui
