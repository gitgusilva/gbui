// A message that stays, in the flow, where the thing it is about is.
//
// The other half of `toast`, and the distinction is worth stating because
// applications reach for the wrong one constantly. A toast is *transient and
// global*: it appears in a corner, it says what just happened, it goes away. A
// banner is *persistent and local*: it sits in the layout above the thing it is
// about, and it is still there because the condition is still true.
//
// "Merge in progress — 3 conflicts remain" is a banner. It cannot be a toast,
// because a reader who looked away has lost it and the merge is still going on.
// "Pushed 3 commits" is a toast. It cannot be a banner, because there is nothing
// left for the reader to do about it and it would sit there forever.
//
// The rule: if dismissing it would lose information the reader still needs, it
// is a banner.
#pragma once

#include <optional>
#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/icons.hpp"

namespace gbui {

/** How much attention it is owed, which decides the colour, the glyph, and
 *  whether a screen reader is interrupted. */
enum class BannerKind { Info, Success, Warning, Danger };

struct BannerOptions {
    /** How much attention it is owed — and therefore whether a screen reader
     *  is interrupted by it. See `BannerKind`. */
    BannerKind kind = BannerKind::Info;
    /** A second line under the title, for the part that does not fit in one. */
    std::string_view detail{};
    /** Overrides the glyph the kind would pick. */
    std::optional<Icon> icon{};
    /** An × at the trailing edge. A banner whose condition the reader cannot
     *  clear should not have one — an × that puts the message back next frame
     *  is a control that does not work. */
    bool closable = false;
    /** The label of a single action at the trailing edge — "Resolve",
     *  "Retry", "Reload". Empty draws none. */
    std::string_view action{};
};

struct BannerResult {
    /** The × was pressed. */
    bool dismissed = false;
    /** The action was pressed. */
    bool acted = false;
};

/**
 * A message in the flow.
 *
 * `Danger` and `Warning` take ARIA's `alert`, which interrupts a screen reader
 * mid-sentence; `Info` and `Success` take `status`, which waits for a pause.
 * That difference is the whole reason the kind is not just a colour: a
 * conflict a reader has to know about now and a note they can hear in a moment
 * are not the same message.
 */
BannerResult banner(Ui& ui, const Interaction& input, std::string_view id,
                    std::string_view title, const BannerOptions& options = {});

}  // namespace gbui
