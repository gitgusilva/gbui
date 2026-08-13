// A picture made of pixels, and how it is fitted into the box it is given.
//
// **The toolkit decodes nothing.** There is no PNG reader here and there will
// not be one: a decoder is a dependency, and the one rule this library keeps
// above all others is that it has none outside `platform/`. What it takes is
// what a decoder produces — eight-bit RGBA, row-major — from whichever one the
// application already has, or from pixels it drew itself.
//
// That is the same division `platform/font` works under: the toolkit lays text
// out and something else turns a file into glyphs.
#pragma once

#include <cstdint>

#include "gbui/core/geometry.hpp"

namespace gbui {

/**
 * Pixels the caller owns.
 *
 * **Borrowed for the frame, never copied.** The buffer has to outlive the
 * frame it is handed to, exactly as the `std::string_view` a label carries
 * does — the tree is rebuilt every frame and owns nothing in it. An application
 * holding its images in a member, a cache or a static array satisfies that
 * without thinking about it; one decoding into a local and drawing from it does
 * not, and will draw whatever the stack has since put there.
 *
 * Straight RGBA, not premultiplied: it is what every decoder hands back, and
 * premultiplying is a loop the caller would have to write to satisfy an
 * argument they never had.
 */
struct Bitmap {
    const std::uint8_t* pixels = nullptr;
    int width = 0;
    int height = 0;
    /** Bytes per row. Zero means `width * 4`, which is what a tightly packed
     *  buffer has — and what a sub-image of a larger one does not. */
    int stride = 0;

    bool valid() const { return pixels != nullptr && width > 0 && height > 0; }
    int pitch() const { return stride > 0 ? stride : width * 4; }
    Vec2 size() const { return {static_cast<float>(width), static_cast<float>(height)}; }
};

/** How a picture is fitted into its box. CSS's `object-fit`, and named after
 *  it, because that is the vocabulary anyone reaching for this already has. */
enum class ImageFit {
    /** Stretched to the box, aspect ratio be damned. */
    Fill,
    /** As large as fits, whole, centred — the default, and the only one that
     *  never lies about a logo's proportions. */
    Contain,
    /** As small as covers, centred and cropped. What a banner wants. */
    Cover,
    /** Its own size, centred, cropped if it does not fit. */
    None,
};

/**
 * Where a picture of `source` lands inside `box`.
 *
 * Returns the destination rectangle, which for `Cover` and `None` may be larger
 * than the box — the caller clips. One function so the widget, the painter and
 * anyone laying out by hand agree, since three answers to "where does it go"
 * is how a picture ends up drawn in one place and clipped against another.
 */
constexpr Rect fitted(const Rect& box, Vec2 source, ImageFit fit) {
    if (source.x <= 0.0f || source.y <= 0.0f || box.empty()) return box;
    float width = box.width;
    float height = box.height;
    switch (fit) {
        case ImageFit::Fill:
            return box;
        case ImageFit::None:
            width = source.x;
            height = source.y;
            break;
        case ImageFit::Contain:
        case ImageFit::Cover: {
            const float byWidth = box.width / source.x;
            const float byHeight = box.height / source.y;
            const float scale = fit == ImageFit::Contain ? (byWidth < byHeight ? byWidth : byHeight)
                                                         : (byWidth > byHeight ? byWidth : byHeight);
            width = source.x * scale;
            height = source.y * scale;
            break;
        }
    }
    return Rect{box.x + (box.width - width) / 2.0f, box.y + (box.height - height) / 2.0f, width,
                height};
}

}  // namespace gbui
