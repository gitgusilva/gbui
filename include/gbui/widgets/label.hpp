// A caption for a control.
#pragma once

#include <optional>
#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"

namespace gbui {

struct LabelOptions {
    /** The control this names. Clicking the label focuses it, exactly as
     *  `<label for>` does, unless that control is disabled or read-only. */
    std::string_view forId{};
    bool disabled = false;
    bool readOnly = false;
    Token color = Token::TextMuted;
    float size = kAuto;
    float width = kAuto;
    /** Marks the field as required, with the usual asterisk. */
    bool required = false;
};

/**
 * Returns the id to focus when the label was clicked, or nothing. The caller
 * hands it to `Interaction::focus`, which keeps the toolkit from mutating
 * interaction state behind a component's back:
 *
 *     if (const auto target = label(ui, input, "f.name", "Repository", {.forId = "name"}))
 *         interaction.focus(*target);
 */
std::optional<std::string_view> label(Ui& ui, const Interaction& input, std::string_view id,
                                      std::string_view text, const LabelOptions& options = {});

}  // namespace gbui
