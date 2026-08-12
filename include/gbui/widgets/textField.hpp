// A single-line text field.
#pragma once

#include <optional>
#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/input/textEdit.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/icons.hpp"

namespace gbui {

struct TextFieldOptions {
    std::string_view placeholder{};
    bool disabled = false;
    bool readOnly = false;
    /** Draws bullets instead of the text. The state still holds the real
     *  string — this hides it, it does not protect it. */
    bool password = false;
    /**
     * The eye at the trailing edge that shows the password while it is held.
     *
     * On by default, because a field you cannot read back is a field people
     * mistype into; turn it off where a shoulder-surfer is the threat being
     * designed against. Ignored unless `password` is set.
     */
    bool revealToggle = true;
    /** Whether the text is currently shown. The caller owns it, like every
     *  other piece of state here, and flips it when the result says so. */
    bool revealed = false;
    std::optional<Icon> leading{};
    float height = 32.0f;
    float width = kAuto;
    float grow = 0.0f;
};

/**
 * Edits `state` in place when it has focus and returns what happened, so a
 * caller can react to a submit without diffing the text.
 *
 * The pointer places the caret: a press puts it at the character it landed on,
 * and holding and moving drags a selection out from there. Both measure the run
 * the same way the caret is drawn, so what is clicked is where it lands.
 */
/** What a text field reports back beyond the edit itself. */
struct TextFieldResult : TextEditResult {
    /** The eye was clicked this frame: flip the `revealed` you passed in. */
    bool toggledReveal = false;
};

TextFieldResult textField(Ui& ui, const Interaction& input, std::string_view id,
                          TextEditState& state, const TextFieldOptions& options = {});

}  // namespace gbui
