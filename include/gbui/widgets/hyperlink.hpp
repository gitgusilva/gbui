// A hyperlink.
#pragma once

#include <optional>
#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/input/keys.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/icons.hpp"

namespace gbui {

struct HyperlinkOptions {
    /**
     * Where it points. Following it hands this to `openUrl`.
     *
     * Empty follows nothing and only reports the click, which is what a link
     * that scrolls somewhere inside the application wants.
     */
    std::string_view href{};
    /**
     * Modifiers that must be held for a click to count.
     *
     * All false — the default — means a plain click follows it, which is what a
     * link is. Requiring a chord is for a link inside editable text, where a
     * plain click has to place the caret instead; that is the bargain every
     * editor makes, and it is a decision about the surrounding document rather
     * than about the link, so it is the caller's.
     */
    Modifiers chord{};
    bool disabled = false;
    /** Drawn after the label — an arrow for "this leaves the application". */
    std::optional<Icon> trailing{};
    Token color = Token::Accent;
    float size = kAuto;
    /** Underlines always, or only under the pointer. Always is the accessible
     *  default: colour alone is not a cue for anyone who cannot see it. */
    bool underlineOnHover = false;
};

/**
 * Returns true on the frame it was followed — by click, or by Return while
 * focused.
 *
 * `hyperlink` rather than `link`, because `link` in a C++ codebase already
 * means the linker, a linked list, or the act of linking; this is the one thing
 * it does not usually mean.
 */
[[nodiscard]] bool hyperlink(Ui& ui, const Interaction& input, std::string_view id, std::string_view label,
          const HyperlinkOptions& options = {});

}  // namespace gbui
