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

TextEditResult insertText(TextEditState& state, std::string_view text) {
    if (text.empty()) return {};
    state.clampToText();
    if (state.hasSelection()) deleteSelection(state);
    state.text.insert(state.caret, text);
    state.caret += text.size();
    state.anchor = state.caret;
    return {.changed = true};
}

TextEditResult applyKey(TextEditState& state, const KeyEvent& event) {
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
                          std::string_view typed) {
    TextEditResult result;
    for (const KeyEvent& event : keys) {
        const TextEditResult one = applyKey(state, event);
        result.changed |= one.changed;
        result.moved |= one.moved;
        result.submitted |= one.submitted;
        result.cancelled |= one.cancelled;
    }
    if (!typed.empty()) {
        // Control characters arrive as keys, not as text; letting them through
        // here would put a literal tab or newline in a single-line field.
        std::string printable;
        printable.reserve(typed.size());
        for (const char c : typed) {
            if (static_cast<unsigned char>(c) >= 0x20 || static_cast<unsigned char>(c) >= 0x80) {
                printable.push_back(c);
            }
        }
        const TextEditResult one = insertText(state, printable);
        result.changed |= one.changed;
    }
    return result;
}

}  // namespace gbui
