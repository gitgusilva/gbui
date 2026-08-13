// A strip whose contents slide past and come round again.
#pragma once

#include <functional>
#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"

namespace gbui {

struct MarqueeOptions {
    /** Pixels a second. Negative runs the other way, which is what a right-to
     *  -left reader expects of the same strip. */
    float speed = 42.0f;
    /** Between the end of one pass and the start of the next, so a short strip
     *  does not read as one long word repeated. */
    float gap = 32.0f;
    /** The strip's own height. Auto takes the content's. */
    float height = kAuto;
    /**
     * Takes the space left in the row it is in, and does so by default.
     *
     * It has to be told. Both passes are out of the flow — that is how they
     * slide — so the strip has no content to be measured from and would
     * otherwise collapse to nothing and draw an empty band, which is exactly
     * what it did.
     */
    float grow = 1.0f;
    Edges padding{};
};

/**
 * Draws `content` twice, side by side, sliding.
 *
 * Twice is the whole trick. One copy leaves a hole behind it while it travels;
 * a second, exactly a content's width behind, fills that hole — and because the
 * offset wraps at that same width, the seam between them never arrives at a
 * moment the reader could see it. Anything narrower than the strip would show
 * the gap on its own, which is what `gap` is measured from rather than added to.
 *
 * **The clock is the caller's**, like every other piece of state here. That is
 * not ceremony: it is what lets a strip stop. Freeze the number while the
 * pointer is over it and the ticker holds still to be read, which is the one
 * interaction a ticker has; pass a running one and it never stops. A component
 * holding its own clock could do neither on request.
 *
 * The content is built twice per frame, so it should be cheap — a row of
 * labels, not a table. It is laid out in one line: this is a strip, and a strip
 * that wrapped would be a paragraph that moves.
 */
void marquee(Ui& ui, const Interaction& input, std::string_view id, float seconds,
             const std::function<void(Ui&)>& content, const MarqueeOptions& options = {});

}  // namespace gbui
