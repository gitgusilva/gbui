// A key, drawn as a key.
//
// HTML's `<kbd>`, and the reason it exists there too: "press Ctrl+Shift+P"
// written as prose is a sentence a reader has to parse, and the same thing drawn
// as three keycaps is a picture they recognise. Shortcut lists, menus, empty
// states and tooltips all want it, and every application draws it slightly
// differently until something says how.
//
// The splitting is the useful part. `kbd(ui, "Ctrl+Shift+P")` draws three caps
// with the pluses between them, because a caller should be able to write the
// shortcut the way they would say it.
#pragma once

#include <string_view>

#include "gbui/scene/ui.hpp"

namespace gbui {

struct KbdOptions {
    float size = 11.0f;
    /**
     * What separates the keys in the string, and what is drawn between the
     * caps.
     *
     * `+` is the desktop convention and `-` is Emacs's. Setting it to nothing
     * draws the whole string as one cap, which is what a key with a plus in its
     * *name* needs.
     */
    std::string_view separator = "+";
    /** Announced instead of the keys. A reader hears "Control Shift P" rather
     *  than three letters when the caller spells it out; empty reads the caps
     *  as they are written. */
    std::string_view name{};
};

/**
 * One or more keycaps.
 *
 * The string is split on `separator`, so `"Ctrl+P"` is two caps and `"Ctrl"` is
 * one. Whitespace around each part is trimmed, because `"Ctrl + P"` is how
 * people write it.
 */
NodeId kbd(Ui& ui, std::string_view keys, const KbdOptions& options = {});

}  // namespace gbui
