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
    std::optional<Icon> leading{};
    bool disabled = false;
    /** Stretches to fill the row it is in, the way a COMMIT button does. */
    bool block = false;
    /** Zero takes the active design's control height. */
    float height = 0.0f;
    /** The glyph's size. Zero matches the label's, which is what a button with
     *  a label wants; one with only an icon in it usually wants more, since the
     *  glyph is the whole button rather than a mark beside a word. */
    float iconSize = 0.0f;
    std::string_view id{};
    /**
     * What a reader who cannot see it is told this button is called.
     *
     * The label, unless it is given — which leaves exactly one case where the
     * caller must say something: **an icon-only button, which has no label to
     * borrow.** Nothing is guessed from the glyph; a name inferred from
     * `Icon::Trash` would be a guess the reader has no way to check, and
     * "button, button, button" across a toolbar is the failure this exists to
     * stop.
     */
    std::string_view name{};
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
