// A progress bar, determinate or not.
#pragma once

#include "gbui/scene/ui.hpp"

namespace gbui {

struct ProgressOptions {
    /** Below zero draws the indeterminate form. */
    double value = 0.0;
    float height = 6.0f;
    float width = kAuto;
    float grow = 1.0f;
    Token color = Token::Accent;
    /** Phase for the indeterminate form, in turns. Feed it a clock. */
    float phase = 0.0f;
};

/** Not an input, but it belongs with them: the same track, the same tokens. */
NodeId progressBar(Ui& ui, const ProgressOptions& options = {});

}  // namespace gbui
