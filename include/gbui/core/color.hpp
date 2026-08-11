// Colour, in the two notations the GitBox theme ecosystem already speaks:
// "#RRGGBB" as written in a theme.json, and "30 30 30" as the CSS custom
// properties carry it so Tailwind can add an alpha channel.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace gbui {

struct Color {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    float a = 1.0f;

    constexpr Color() = default;
    constexpr Color(std::uint8_t r_, std::uint8_t g_, std::uint8_t b_, float a_ = 1.0f)
        : r(r_), g(g_), b(b_), a(a_) {}

    /** The same colour at a different opacity — how the design system builds
     *  overlays and disabled states from one token. */
    constexpr Color withAlpha(float alpha) const { return {r, g, b, alpha}; }

    /** "#rrggbb" for painters that want CSS syntax. Alpha travels separately,
     *  because SVG and Skia both take it as its own attribute. */
    std::string hex() const;

    /** Relative luminance, per WCAG. This is what decides whether text on a
     *  filled button is drawn light or dark — the theme supplies accentFg, but
     *  a community theme can get it wrong, and unreadable is worse than off-hue. */
    float luminance() const;

    /** WCAG contrast ratio, 1.0 (identical) to 21.0 (black on white). */
    static float contrast(Color a, Color b);

    /** Whichever of the two candidates is legible on this background. */
    Color readableOn(Color light, Color dark) const {
        return contrast(*this, light) >= contrast(*this, dark) ? light : dark;
    }

    friend bool operator==(const Color&, const Color&) = default;
};

/**
 * Hue, saturation and value — the space a colour picker is *edited* in.
 *
 * Kept as its own type rather than converted back and forth on every frame,
 * because the round trip is lossy in exactly the place it hurts: at zero
 * saturation every hue is the same grey, so dragging down the left edge of a
 * picker and back up would lose the hue the user had chosen. The picker holds
 * this and produces a `Color`, not the other way round.
 */
struct Hsv {
    float hue = 0.0f;         ///< degrees, 0..360
    float saturation = 0.0f;  ///< 0..1
    float value = 0.0f;       ///< 0..1
    float alpha = 1.0f;

    Color toColor() const;
    /** The nearest HSV. Hue is undefined for a grey and comes back as 0, which
     *  is why an editor should keep its own `Hsv` rather than round-trip. */
    static Hsv fromColor(Color color);
};

/** Parses "#RGB", "#RRGGBB", "#RRGGBBAA" or the "R G B" triplet form.
 *  Returns nothing rather than a default colour: a theme with a malformed token
 *  should be reported to its author, not silently painted black. */
std::optional<Color> parseColor(std::string_view text);

}  // namespace gbui
