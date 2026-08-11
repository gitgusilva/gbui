#include "gbui/core/color.hpp"

#include <algorithm>
#include <cmath>

#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>

namespace gbui {
namespace {

std::optional<int> hexPair(std::string_view s) {
    int value = 0;
    const auto* end = s.data() + s.size();
    const auto result = std::from_chars(s.data(), end, value, 16);
    if (result.ec != std::errc{} || result.ptr != end) return std::nullopt;
    return value;
}

std::string_view trim(std::string_view s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

/** sRGB channel to linear light, the transfer function WCAG's luminance uses. */
float linearize(std::uint8_t channel) {
    const float c = static_cast<float>(channel) / 255.0f;
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

}  // namespace

std::string Color::hex() const {
    std::array<char, 8> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "#%02x%02x%02x", r, g, b);
    return std::string(buffer.data());
}

float Color::luminance() const {
    return 0.2126f * linearize(r) + 0.7152f * linearize(g) + 0.0722f * linearize(b);
}

float Color::contrast(Color a, Color b) {
    const float la = a.luminance();
    const float lb = b.luminance();
    const float lighter = la > lb ? la : lb;
    const float darker = la > lb ? lb : la;
    return (lighter + 0.05f) / (darker + 0.05f);
}

Color Hsv::toColor() const {
    const float h = std::fmod(std::fmod(hue, 360.0f) + 360.0f, 360.0f) / 60.0f;
    const float s = std::clamp(saturation, 0.0f, 1.0f);
    const float v = std::clamp(value, 0.0f, 1.0f);

    const float chroma = v * s;
    const float second = chroma * (1.0f - std::fabs(std::fmod(h, 2.0f) - 1.0f));
    const float match = v - chroma;

    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    switch (static_cast<int>(h)) {
        case 0: r = chroma; g = second; break;
        case 1: r = second; g = chroma; break;
        case 2: g = chroma; b = second; break;
        case 3: g = second; b = chroma; break;
        case 4: r = second; b = chroma; break;
        default: r = chroma; b = second; break;
    }
    const auto byte = [&](float channel) {
        return static_cast<std::uint8_t>(std::lround(std::clamp(channel + match, 0.0f, 1.0f) * 255.0f));
    };
    return Color{byte(r), byte(g), byte(b), std::clamp(alpha, 0.0f, 1.0f)};
}

Hsv Hsv::fromColor(Color color) {
    const float r = static_cast<float>(color.r) / 255.0f;
    const float g = static_cast<float>(color.g) / 255.0f;
    const float b = static_cast<float>(color.b) / 255.0f;
    const float high = std::max({r, g, b});
    const float low = std::min({r, g, b});
    const float chroma = high - low;

    Hsv out;
    out.value = high;
    out.saturation = high > 0.0f ? chroma / high : 0.0f;
    out.alpha = color.a;
    if (chroma <= 0.0f) return out;  // a grey has no hue to report

    if (high == r) out.hue = 60.0f * std::fmod((g - b) / chroma, 6.0f);
    else if (high == g) out.hue = 60.0f * ((b - r) / chroma + 2.0f);
    else out.hue = 60.0f * ((r - g) / chroma + 4.0f);
    if (out.hue < 0.0f) out.hue += 360.0f;
    return out;
}

std::optional<Color> parseColor(std::string_view text) {
    const std::string_view s = trim(text);
    if (s.empty()) return std::nullopt;

    if (s.front() == '#') {
        const std::string_view digits = s.substr(1);
        // "#abc" is the CSS shorthand: each digit is doubled, not zero-padded.
        if (digits.size() == 3) {
            Color out;
            std::uint8_t* channels[3] = {&out.r, &out.g, &out.b};
            for (std::size_t i = 0; i < 3; ++i) {
                const auto v = hexPair(digits.substr(i, 1));
                if (!v) return std::nullopt;
                *channels[i] = static_cast<std::uint8_t>(*v * 17);
            }
            return out;
        }
        if (digits.size() == 6 || digits.size() == 8) {
            Color out;
            std::uint8_t* channels[3] = {&out.r, &out.g, &out.b};
            for (std::size_t i = 0; i < 3; ++i) {
                const auto v = hexPair(digits.substr(i * 2, 2));
                if (!v) return std::nullopt;
                *channels[i] = static_cast<std::uint8_t>(*v);
            }
            if (digits.size() == 8) {
                const auto alpha = hexPair(digits.substr(6, 2));
                if (!alpha) return std::nullopt;
                out.a = static_cast<float>(*alpha) / 255.0f;
            }
            return out;
        }
        return std::nullopt;
    }

    // The "30 30 30" form the CSS custom properties use, so a theme can be read
    // straight out of a stylesheet as well as out of a theme.json.
    Color out;
    std::uint8_t* channels[3] = {&out.r, &out.g, &out.b};
    std::size_t index = 0;
    std::size_t cursor = 0;
    while (cursor < s.size() && index < 3) {
        while (cursor < s.size() && (s[cursor] == ' ' || s[cursor] == ',')) ++cursor;
        const std::size_t start = cursor;
        while (cursor < s.size() && s[cursor] >= '0' && s[cursor] <= '9') ++cursor;
        if (cursor == start) return std::nullopt;
        int value = 0;
        const auto result = std::from_chars(s.data() + start, s.data() + cursor, value);
        if (result.ec != std::errc{} || value < 0 || value > 255) return std::nullopt;
        *channels[index++] = static_cast<std::uint8_t>(value);
    }
    if (index != 3) return std::nullopt;
    while (cursor < s.size() && (s[cursor] == ' ' || s[cursor] == ',')) ++cursor;
    if (cursor != s.size()) return std::nullopt;
    return out;
}

}  // namespace gbui
