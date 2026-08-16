#include "gbui/widgets/field.hpp"

#include <string>

#include "detail.hpp"
#include "gbui/widgets/label.hpp"
#include "gbui/widgets/text.hpp"

namespace gbui {

// What this shares with its siblings, rather than a copy in each.
using namespace detail;

FieldResult field(Ui& ui, const Interaction& input, std::string_view id,
                  const FieldOptions& options, const std::function<void(Ui&)>& control) {
    FieldResult result;
    const bool invalid = !options.error.empty();

    Style column;
    column.direction = Direction::Column;
    column.gap = options.gap;
    column.width = options.width;
    column.grow = options.grow;
    // A field gives horizontally like anything else in a form, but never below
    // what the control inside it can be used at.
    column.minWidth = 0.0f;
    auto scope = ui.scope(column);
    ui.tag(id);

    if (!options.label.empty()) {
        result.focus = label(ui, input, std::string(id) + ".label", options.label,
                             {.forId = options.forId,
                              .disabled = options.disabled,
                              // The caption turns with the message: a field that
                              // is wrong should read as wrong from its heading,
                              // not only from the small print under it.
                              .color = invalid ? Token::Removed : Token::TextMuted,
                              .required = options.required});
    }

    control(ui);

    // The error replaces the help rather than joining it. Advice and a
    // complaint in the same place at the same time is two things asking to be
    // read first, and the complaint is always the one that matters now.
    const std::string_view message = invalid ? options.error : options.help;
    if (!message.empty()) {
        text(ui, message,
             {.color = invalid ? Token::Removed : Token::TextMuted,
              .size = 11.5f,
              .overflow = TextOverflow::Wrap});
        // Tagged, and pointed at the control it is about: a message nothing
        // refers to is a message a screen reader never reaches. `describes`
        // rather than the control's own `describedBy`, because the message is
        // built after the control and a component never reaches into another
        // component's node — the same edge, stated from the end that knows it.
        ui.tag(std::string(id) + (invalid ? ".error" : ".help")).ignoresPointer().accessible({
            .role = invalid ? Role::Alert : Role::Label,
            .name = message,
            .relations = {.describes = options.forId},
        });
    }

    (void)scope;
    return result;
}

}  // namespace gbui
