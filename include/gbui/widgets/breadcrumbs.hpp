// Where you are, and every step back to the top.
//
// A path — `src / widgets / treeView.cpp` — where every step but the last is
// somewhere you can go. Two jobs at once, and the second is the one people
// forget: it *says where you are* as much as it navigates, which is why the last
// crumb is not a link and must not be styled as one.
//
// ---- the collapse is the whole problem --------------------------------------
//
// A trail is only useful while it fits, and paths do not fit: `src/widgets/…` is
// three steps and a repository is often ten. Given a `maxVisible`, the **middle**
// collapses to an ellipsis and the ends stay, because those are the two a reader
// needs — where they are, and the root they can get back to. Dropping the tail
// instead would hide the answer to "where am I", and dropping the head would
// leave a trail with no way home.
#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/icons.hpp"

namespace gbui {

struct Crumb {
    std::string_view label{};
    std::optional<Icon> icon{};
    /** What it is called when the label is an abbreviation or a glyph. */
    std::string_view name{};
};

struct BreadcrumbsOptions {
    /**
     * How many crumbs may be drawn before the middle collapses. Zero draws all
     * of them.
     *
     * Counted in *crumbs*, not pixels, because a trail that measured itself
     * would need last frame's width and would therefore be one frame wrong
     * every time the window changed — visibly, since the thing that changes is
     * how many steps you can see.
     */
    std::size_t maxVisible = 0;
    /** Drawn between crumbs. A glyph rather than a character so it cannot be
     *  mistaken for part of a name. */
    Icon separator = Icon::ChevronRight;
    /** What the trail is, for a reader who arrives in the middle of a page.
     *  "Breadcrumb" is what every screen reader's user expects to hear. */
    std::string_view name = "Breadcrumb";
    float size = 12.5f;
};

struct BreadcrumbsResult {
    /** The crumb pressed this frame. Never the last one: that is where the
     *  reader already is, so it is not a link and takes no press. */
    std::optional<std::size_t> chosen{};
    /** The ellipsis was pressed. A caller can answer it by raising
     *  `maxVisible`, or by opening a menu of the hidden steps — which is the
     *  better answer and is the caller's to build, since only it knows where
     *  there is room to put one. */
    bool expanded = false;
};

/**
 * A path, with everything but the last step navigable.
 *
 * The last crumb takes `current`, which is ARIA's `aria-current` and not
 * `selected`: a reader is not being told they picked it, they are being told it
 * is where they are.
 */
BreadcrumbsResult breadcrumbs(Ui& ui, const Interaction& input, std::string_view id,
                              const std::vector<Crumb>& trail,
                              const BreadcrumbsOptions& options = {});

}  // namespace gbui
