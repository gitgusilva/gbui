// A pill you can press, or take off.
//
// `badge` and this look almost identical and are not the same thing: a badge is
// *output* — a count, a status, a label the reader cannot act on — and a chip is
// *input*. A filter that is on, a branch that has been picked, a tag that can be
// taken off again. The difference is entirely in what happens when you press it,
// which is why they are two components rather than one with a flag: a badge has
// no press, no focus ring and nothing to announce, and giving it those would
// make every status pill on every screen a stop on the Tab route.
//
// Reach for `badge` when it only says something. Reach for this when the reader
// can change it.
#pragma once

#include <optional>
#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/icons.hpp"

namespace gbui {

struct ChipOptions {
    /**
     * Drawn as chosen: filled rather than outlined, and announced as pressed.
     *
     * This is what a filter chip is for. A chip that only ever looks the same
     * is a label with a cursor on it.
     */
    bool selected = false;
    /** An × at the trailing edge, and a second thing to press. */
    bool removable = false;
    /** A glyph before the label. */
    std::optional<Icon> leading{};
    bool disabled = false;
    /** What it is called, when the label is not enough on its own — "main" on a
     *  branch chip is a word, not a sentence. */
    std::string_view name{};
    Token color = Token::Accent;
};

struct ChipResult {
    /** The chip itself was pressed. */
    bool pressed = false;
    /** The × was pressed. Never both: a press on the × is not a press on the
     *  chip, or removing one would also toggle it on the way out. */
    bool removed = false;
};

/**
 * A pressable pill.
 *
 * Enter and Space press it; when it is removable, Delete and Backspace remove
 * it while it has focus — which is the gesture every tag field on the web has
 * and the reason a removable chip needs no pointer at all.
 */
ChipResult chip(Ui& ui, const Interaction& input, std::string_view id, std::string_view label,
                const ChipOptions& options = {});

}  // namespace gbui
