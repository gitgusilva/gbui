// A modal dialog, with a backdrop and a header you can drag it by.
#pragma once

#include <optional>
#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/icons.hpp"

namespace gbui {

struct ModalOptions {
    float width = 420.0f;
    float height = kAuto;
    /** Dims what is behind it. A modal without one is a floating panel. */
    bool backdrop = true;
    /** Lets the user drag it by its header, kept inside the window. */
    bool draggable = true;
    /** Beside the title. A dialog that asks something usually wants one; one
     *  that reports something usually does not. */
    std::optional<Icon> icon{};
    /** The destructive kind. Recolours the title, and takes `AlertDialog` in
     *  the accessibility tree — which is what tells a reader they were
     *  interrupted rather than that they asked. */
    bool danger = false;
};

struct ModalResult {
    /** Where the dialog is now. Hand it back next frame; the first frame may
     *  pass an empty position to have it centred. */
    Vec2 position{};
    bool dismissed = false;   ///< the close button, the backdrop or Escape
};

struct Modal {
    Ui::Scope body;
    ModalResult result;
};

/** The caller supplies the position so dragging survives the tree being
 *  rebuilt, and gets it back updated. */
Modal modal(Ui& ui, const Interaction& input, std::string_view id, std::string_view title,
            Vec2 position, const ModalOptions& options = {});

/** The row of buttons at the foot of a dialog, pushed to the right. */
Ui::Scope modalActions(Ui& ui);

}  // namespace gbui
