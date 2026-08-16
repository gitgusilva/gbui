// One picture at a time, out of a set, with the rest along the bottom.
//
// The viewer half of what PrimeVue calls Gallery: a main image, a way through
// the set, a caption, and a strip of thumbnails that doubles as the way to jump
// straight to one.
//
// ---- what it is not, and why -----------------------------------------------
//
// PrimeVue's also zooms, rotates, flips, downloads and goes fullscreen. None of
// those four is missing because nobody wanted them; each is missing because the
// thing under it does not exist yet, and shipping a rotate button that does not
// rotate would be worse than not shipping one:
//
//   * **zoom and rotate** need a transform on a node. The painter has none —
//     `Animator`'s own header admits the same gap for `scale` — and faking it by
//     resampling the bitmap per frame would be a decoder's job done badly in a
//     rasteriser.
//   * **download** needs a native file dialog. Nothing in this library touches
//     the filesystem, and the platform layer that would is Tier 1 and not
//     built.
//   * **fullscreen** is a second window, or a window mode, and `platform/`
//     owns one window today.
//
// All four are named in ROADMAP against the work that unblocks them. When a
// transform lands, zoom and rotate are an afternoon here.
//
// ---- the caller owns the pixels ---------------------------------------------
//
// `Bitmap` is eight-bit RGBA the application already has, from whichever
// decoder it already links — see `gbui/core/image.hpp` for why that line is
// where it is. The pixels are **borrowed for the frame**: they are read when
// the frame is painted, not when this is called.
#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "gbui/core/image.hpp"
#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/scroll.hpp"

namespace gbui {

/** One picture in the set. */
struct GalleryItem {
    Bitmap image{};
    /** Drawn under the main picture. Empty draws none. */
    std::string_view caption{};
    /**
     * What the picture *is*, for a reader who cannot see it — HTML's `alt`.
     *
     * Falls back to the caption, which is usually right: a caption that
     * describes the photograph is the alt text, and one that credits the
     * photographer is not. Where they differ, say both.
     */
    std::string_view alt{};
};

/** What the application remembers between frames. */
struct GalleryState {
    std::size_t current = 0;
    /** Where the thumbnail strip is scrolled to. The current thumbnail is kept
     *  in view, so a set of forty is walkable from the keyboard. */
    ScrollState thumbnails{};
};

struct GalleryOptions {
    /** The strip along the bottom, which is also how a reader jumps straight to
     *  one. Off leaves the arrows and the keyboard. */
    bool thumbnails = true;
    /** How big one thumbnail is. Square, because a strip of mixed shapes is a
     *  strip whose targets move as the reader scrolls it. */
    float thumbnailSize = 56.0f;
    /** The line under the main picture. */
    bool captions = true;
    /** The two arrows over the picture. */
    bool navigators = true;
    /** Past the last picture is the first. */
    bool loop = false;
    /** How the main picture meets a box that is not its shape. `Contain`, so
     *  nothing is cropped: a gallery is for looking at whole pictures. */
    ImageFit fit = ImageFit::Contain;
    /** What the set is of — "Screenshots", "Site survey, March". */
    std::string_view name{};
    /** The main picture's box. The thumbnails and the caption are extra. */
    float height = 320.0f;
    float width = kAuto;
    float grow = 0.0f;
};

struct GalleryResult {
    std::size_t current = 0;
    bool changed = false;
};

/** Draws the set and shows `state.current`. An empty set draws nothing at all,
 *  rather than an empty frame that looks like a picture failed to load. */
GalleryResult gallery(Ui& ui, const Interaction& input, std::string_view id,
                      const std::vector<GalleryItem>& items, GalleryState& state,
                      const GalleryOptions& options = {});

}  // namespace gbui
