#include "gbui/widgets/avatar.hpp"

#include <algorithm>
#include <cctype>
#include <string>

#include "gbui/widgets/text.hpp"

namespace gbui {

namespace {

/** Whether a byte begins a UTF-8 sequence. Initials are taken per *character*,
 *  and a name starting with a two-byte letter would otherwise contribute half
 *  of one. */
bool startsCharacter(char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; }

/** The first whole character of `word`, upper-cased when it is ASCII. Anything
 *  else is passed through as it is: there is no locale here to case a letter
 *  with, and a wrongly-cased initial is worse than an unchanged one. */
std::string firstCharacter(std::string_view word) {
    if (word.empty()) return {};
    std::size_t end = 1;
    while (end < word.size() && !startsCharacter(word[end])) ++end;
    std::string out(word.substr(0, end));
    if (out.size() == 1) {
        out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
    }
    return out;
}

/**
 * A hue from a name, stable across machines and sessions.
 *
 * FNV-1a, the same hash the accessibility tree uses for its ids, taken modulo
 * the circle. Stability is the point: an avatar that changed colour between
 * runs would be worse than a grey one, because a reader learns colours.
 */
float hueFor(std::string_view name) {
    std::uint32_t hash = 2166136261u;
    for (const char c : name) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 16777619u;
    }
    return static_cast<float>(hash % 360u);
}

}  // namespace

std::string initialsFor(std::string_view name) {
    // Trim, then take the first word and the last. Two letters is the most a
    // circle this size can hold legibly, and three is what turns a monogram
    // into a smudge.
    std::size_t begin = 0;
    while (begin < name.size() && std::isspace(static_cast<unsigned char>(name[begin]))) ++begin;
    std::size_t end = name.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(name[end - 1]))) --end;
    const std::string_view trimmed = name.substr(begin, end - begin);
    if (trimmed.empty()) return {};

    const std::size_t space = trimmed.find_last_of(" \t");
    const std::string first = firstCharacter(trimmed);
    if (space == std::string_view::npos) return first;

    std::size_t after = space;
    while (after < trimmed.size() && std::isspace(static_cast<unsigned char>(trimmed[after]))) {
        ++after;
    }
    return first + firstCharacter(trimmed.substr(after));
}

NodeId avatar(Ui& ui, std::string_view name, const AvatarOptions& options) {
    const float size = std::max(12.0f, options.size);
    const float radius = options.square ? size * 0.22f : size / 2.0f;

    Style box;
    box.width = size;
    box.height = size;
    box.minWidth = 0.0f;
    box.minHeight = 0.0f;
    box.shrink = 0.0f;
    box.radius = radius;
    box.justify = Justify::Center;
    box.align = Align::Center;
    // Clipped rather than merely rounded: the picture inside is a child, and a
    // square child in a round parent is a square unless the parent cuts it.
    box.overflow = Overflow::Hidden;

    const std::string letters =
        options.initials.empty() ? initialsFor(name) : std::string(options.initials);

    if (!options.picture) {
        // A wash dark enough for white text at any hue, rather than the hue
        // itself: a full-saturation yellow behind white is unreadable, and a
        // component that picks its own colour has to pick one that works.
        box.background = Fill{Hsv{hueFor(name), 0.42f, 0.46f}.toColor()};
    }

    auto scope = ui.scope(box);
    const NodeId id = scope.id();

    if (options.picture) {
        Style fill;
        fill.width = size;
        fill.height = size;
        ui.picture(*options.picture, fill, ImageFit::Cover);
    } else if (!letters.empty()) {
        text(ui, letters,
             {.color = Token::AccentFg,
              .weight = FontWeight::SemiBold,
              // Just under half the circle: two capitals at half the diameter
              // touch the edges of it.
              .size = size * 0.4f});
    }
    scope.close();

    // An image with a name, or nothing at all when the name is already beside
    // it. `Image` rather than `None` even for the initials form: what it stands
    // for is the person, and "AL" read out as text is not that.
    if (options.decorative) {
        ui.accessible(id, {.hidden = true});
    } else {
        ui.accessible(id, {.role = Role::Image, .name = name});
    }
    return id;
}

}  // namespace gbui
