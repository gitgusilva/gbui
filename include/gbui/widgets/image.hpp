// A picture in a box — HTML's `<img>`, with the parts of it that are a
// toolkit's business.
//
// What it is *not* is a loader. `Bitmap` is eight-bit RGBA the caller already
// has, from whichever decoder their application already links, or from pixels
// they generated themselves. See `gbui/core/image.hpp` for why that line is
// where it is.
#pragma once

#include <string_view>

#include "gbui/core/image.hpp"
#include "gbui/scene/ui.hpp"

namespace gbui {

struct ImageOptions {
    /** Its box. Either left `kAuto` takes the picture's own size in that
     *  direction, so an image with neither set is drawn at 1:1. */
    Length width = kAuto;
    Length height = kAuto;
    /** How the picture meets a box that is not its shape. */
    ImageFit fit = ImageFit::Contain;
    /** Rounded corners, cut from the picture itself — an avatar is this and a
     *  radius of half its size. */
    float radius = 0.0f;
    float opacity = 1.0f;
    /** Behind the picture, which is what shows through a transparent one and
     *  what fills the letterboxing `Contain` leaves. */
    std::optional<Fill> background{};
    Edges padding{};
    /** Kept out of the flow's stretch, like an icon: a picture has a size and a
     *  container has no business overriding it. */
    float grow = 0.0f;
    float shrink = 0.0f;
    /**
     * Drawn instead when there are no pixels — HTML's `alt`, doing the job
     * `alt` actually does in a browser rather than the one it is credited with.
     *
     * An empty one leaves the box empty, which is right for decoration. A
     * missing logo in a list of companies is not decoration, and a row that
     * silently loses its mark is worse than one showing three letters.
     */
    std::string_view alt{};
};

/**
 * Draws `source` inside a box.
 *
 * The picture is sampled bilinearly and cut by the same rounded corners the
 * boxes use, so an image in a card and the card agree about the curve.
 *
 * The pixels are **borrowed for the frame**: they are read when the frame is
 * painted, not when this is called, and nothing here copies them. That is the
 * same contract a label's `string_view` has, and the same one it breaks in the
 * same way — a buffer that dies at the end of the enclosing scope is a picture
 * of whatever the stack holds by the time anyone looks.
 */
NodeId image(Ui& ui, const Bitmap& source, const ImageOptions& options = {});

}  // namespace gbui
