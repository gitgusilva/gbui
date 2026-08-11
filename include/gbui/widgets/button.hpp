// A button, in the four variants the design system has.
#pragma once

#include <optional>
#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/icons.hpp"

namespace gbui {

enum class ButtonVariant { Primary, Secondary, Ghost, Danger };

struct ButtonOptions {
    ButtonVariant variant = ButtonVariant::Secondary;
    /** Drawn before the label, in the label's colour. */
    std::optional<Icon> leading;
    bool disabled = false;
    /** Stretches to fill the row it is in, the way a COMMIT button does. */
    bool block = false;
    /** Zero takes the active design's control height. */
    float height = 0.0f;
    std::string_view id{};
    /**
     * A circle of ink that grows from where the pointer went down.
     *
     * Unset — the normal case — asks the active `Design`: Material throws ink,
     * the others change the surface. Set, it overrides that decision for this
     * one button, which is the escape hatch and not the default. Either way it
     * needs the overload below (the ripple starts at the *point* the press
     * landed) and an `id`, since that is what the animation is keyed by.
     */
    std::optional<bool> ripple{};
};

/** Without an `Interaction` there is no pointer to start a ripple from, so this
 *  overload ignores `options.ripple`. Everything else is identical. */
NodeId button(Ui& ui, std::string_view text, const ButtonOptions& options = {});

/** The same button, given what it needs to draw a press. */
NodeId button(Ui& ui, const Interaction& input, std::string_view text,
              const ButtonOptions& options = {});

}  // namespace gbui
