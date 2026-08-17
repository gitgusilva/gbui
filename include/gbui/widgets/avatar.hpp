// A person, as a small round picture — or as their initials when there is none.
//
// The fallback is the whole component. Drawing a bitmap in a circle is four
// lines; deciding what to draw when the bitmap has not arrived, or does not
// exist, or the person has no photograph at all is the part every application
// writes badly. A grey circle with nothing in it is indistinguishable from a
// broken one, and a broken-image glyph is worse.
//
// So this always draws *something*: the picture when there is one, otherwise
// the initials on a colour derived from the name — which is stable, so the same
// person is the same colour on every screen and in every session, without
// anybody storing a colour.
#pragma once

#include <optional>
#include <string_view>

#include "gbui/core/image.hpp"
#include "gbui/scene/ui.hpp"

namespace gbui {

struct AvatarOptions {
    float size = 28.0f;
    /**
     * The picture. Nothing draws the initials instead.
     *
     * Borrowed for the frame like every other bitmap here — read when the frame
     * is painted, never copied. See `image`.
     */
    std::optional<Bitmap> picture{};
    /**
     * Square instead of round.
     *
     * A round avatar is a person and a square one is a thing — an organisation,
     * a repository, a bot. Worth the option because the shape is the only
     * signal a reader gets, and getting it backwards makes a list of teams look
     * like a list of people.
     */
    bool square = false;
    /**
     * Overrides the initials, which are otherwise taken from the name.
     *
     * For the cases the derivation gets wrong and only the caller can know:
     * a handle with no spaces in it, a name written family-first, an emoji.
     */
    std::string_view initials{};
    /** Announced instead of the name, when the name is already beside it. A
     *  row that reads out "Ada Lovelace, Ada Lovelace" has said it twice. */
    bool decorative = false;
};

/**
 * A person's picture, or their initials.
 *
 * `name` is what it is announced as and where the initials and the colour come
 * from, so it is a parameter rather than an option: an avatar without one is a
 * decoration, and this component would have nothing to draw.
 */
NodeId avatar(Ui& ui, std::string_view name, const AvatarOptions& options = {});

/**
 * The initials this would draw for a name.
 *
 * Exposed because a list that shows a picture *and* initials elsewhere — a
 * mention, a compact row — should show the same two letters, and because the
 * rule is worth being able to test on its own. First letter of the first word
 * and of the last, upper-cased; one word gives one letter; anything with no
 * letters in it gives nothing.
 */
std::string initialsFor(std::string_view name);

}  // namespace gbui
