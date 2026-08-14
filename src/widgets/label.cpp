#include "gbui/widgets/label.hpp"

#include "detail.hpp"
#include "gbui/widgets/text.hpp"

namespace gbui {

// What this shares with its siblings, rather than a copy in each.
using namespace detail;

std::optional<std::string_view> label(Ui& ui, const Interaction& input, std::string_view id,
                                      std::string_view text, const LabelOptions& options) {
    Style box;
    box.direction = Direction::Row;
    box.align = Align::Center;
    box.gap = 2.0f;
    box.width = options.width;
    box.shrink = 0.0f;
    box.opacity = opacityFor(options.disabled);
    // A label that focuses something is clickable and should say so; one that
    // names a disabled or read-only control is just text.
    const bool actionable = !options.forId.empty() && !options.disabled && !options.readOnly;
    box.cursorHint = actionable ? Cursor::Pointer : Cursor::Default;

    auto scope = ui.scope(box);
    ui.tag(id).cursor(box.cursorHint);
    gbui::text(ui, text, {.color = options.color, .size = options.size});
    if (options.required) {
        gbui::text(ui, "*", {.color = Token::Removed, .size = options.size});
    }
    (void)scope;

    if (actionable && input.clicked(id)) return options.forId;
    return std::nullopt;
}

}  // namespace gbui
