#include "gbui/style/theme.hpp"

#include <string>

#include "gbui/core/json.hpp"
#include "harness.hpp"

using namespace gbui;

namespace {

/** A theme.json with every required field, matching the registry's schema. */
std::string validThemeJson(const std::string& overrides = {}) {
    std::string json = R"({
        "id": "test-theme",
        "name": "Test Theme",
        "type": "dark",
        "meta": { "version": "1.0.0" },
        "colors": {
            "bg": "#2E3440", "bgElevated": "#3B4252", "bgOverlay": "#434C5E",
            "surfaceHover": "#4C566A", "border": "#3B4252", "borderStrong": "#4C566A",
            "textStrong": "#ECEFF4", "text": "#D8DEE9", "textMuted": "#7B88A1",
            "accent": "#88C0D0", "accentHover": "#8FBCBB", "accentFg": "#2E3440",
            "added": "#A3BE8C", "removed": "#BF616A", "modified": "#81A1C1",
            "graph1": "#88C0D0", "graph2": "#81A1C1", "graph3": "#A3BE8C",
            "graph4": "#B48EAD", "graph5": "#BF616A", "graph6": "#D08770",
            "graph7": "#EBCB8B", "graph8": "#5E81AC", "graphMarker": "#2E3440"
        },
        "typography": {
            "uiFont": "IBM Plex Sans", "uiFontSize": 13,
            "monoFont": "IBM Plex Mono", "editorFont": "IBM Plex Mono",
            "editorFontSize": 13, "editorLineHeight": 0, "radius": 6
        }
    })";
    if (!overrides.empty()) json = overrides;
    return json;
}

}  // namespace

TEST("colour parsing covers every notation a theme can use") {
    const auto hexLong = parseColor("#2E3440");
    CHECK(hexLong.has_value());
    CHECK_EQ(hexLong->r, 0x2E);
    CHECK_EQ(hexLong->g, 0x34);
    CHECK_EQ(hexLong->b, 0x40);

    const auto shorthand = parseColor("#abc");
    CHECK(shorthand.has_value());
    CHECK_EQ(shorthand->r, 0xAA);
    CHECK_EQ(shorthand->b, 0xCC);

    // The CSS custom property form, so a theme can be read from a stylesheet.
    const auto triplet = parseColor("30 30 30");
    CHECK(triplet.has_value());
    CHECK_EQ(triplet->g, 30);

    const auto withAlpha = parseColor("#00000080");
    CHECK(withAlpha.has_value());
    CHECK_NEAR(withAlpha->a, 0.502f);

    // Malformed input is rejected rather than defaulted, so a broken theme is
    // reported to its author instead of silently painted.
    CHECK(!parseColor("not a colour").has_value());
    CHECK(!parseColor("#12345").has_value());
    CHECK(!parseColor("300 0 0").has_value());
    CHECK(!parseColor("").has_value());
}

TEST("a registry theme loads with every token") {
    json::ParseError parseError;
    const auto root = json::parse(validThemeJson(), &parseError);
    CHECK(root.has_value());

    std::string error;
    const auto theme = Theme::fromJson(*root, &error);
    CHECK(theme.has_value());
    CHECK(error.empty());
    if (!theme) return;

    CHECK_EQ(theme->id(), std::string("test-theme"));
    CHECK(theme->isDark());
    CHECK_EQ(theme->color(Token::Bg), (Color{0x2E, 0x34, 0x40}));
    CHECK_EQ(theme->color(Token::Accent), (Color{0x88, 0xC0, 0xD0}));
    CHECK_NEAR(theme->typography().radius, 6.0f);
    CHECK_EQ(theme->typography().uiFont, std::string("IBM Plex Sans"));
}

TEST("a missing token names itself instead of defaulting") {
    const std::string missingAccent = R"({
        "colors": { "bg": "#000000" },
        "typography": { "uiFont": "x", "uiFontSize": 13, "monoFont": "x",
                        "editorFont": "x", "editorFontSize": 13,
                        "editorLineHeight": 0, "radius": 6 }
    })";
    const auto root = json::parse(missingAccent);
    CHECK(root.has_value());

    std::string error;
    const auto theme = Theme::fromJson(*root, &error);
    CHECK(!theme.has_value());
    CHECK(error.find("bgElevated") != std::string::npos);
}

TEST("token names round-trip against the schema's spelling") {
    CHECK_EQ(tokenName(Token::BgElevated), std::string_view("bgElevated"));
    CHECK_EQ(tokenName(Token::GraphMarker), std::string_view("graphMarker"));
    CHECK(tokenFromName("surfaceHover") == std::optional<Token>(Token::SurfaceHover));
    CHECK(!tokenFromName("notAToken").has_value());

    // Every token has a name and every name maps back: the two lists cannot
    // drift apart without this failing.
    for (std::size_t i = 0; i < static_cast<std::size_t>(Token::Count); ++i) {
        const auto token = static_cast<Token>(i);
        CHECK(tokenFromName(tokenName(token)) == std::optional<Token>(token));
    }
}

TEST("graph lanes wrap around the eight tokens") {
    const Theme theme = Theme::dark();
    CHECK_EQ(theme.graphLane(0), theme.color(Token::Graph1));
    CHECK_EQ(theme.graphLane(8), theme.color(Token::Graph1));
    CHECK_EQ(theme.graphLane(9), theme.color(Token::Graph2));
}

TEST("an unreadable accentFg is overridden rather than drawn") {
    Theme theme = Theme::dark();

    // A theme that pairs white text with a pale accent: kept as authored would
    // be an unreadable button.
    theme.setColor(Token::Accent, Color{0xF5, 0xE0, 0xA3});
    theme.setColor(Token::AccentFg, Color{255, 255, 255});
    const Color chosen = theme.onAccent();
    CHECK(Color::contrast(theme.color(Token::Accent), chosen) >= 4.5f);

    // A theme that got it right is left alone.
    theme.setColor(Token::Accent, Color{37, 99, 235});
    theme.setColor(Token::AccentFg, Color{255, 255, 255});
    CHECK_EQ(theme.onAccent(), (Color{255, 255, 255}));
}

TEST("the json reader handles what a theme file contains") {
    const auto value = json::parse(R"({"a": [1, 2.5, true, null], "b": "xé\ty"})");
    CHECK(value.has_value());
    if (!value) return;

    const auto* array = value->find("a")->asArray();
    CHECK(array != nullptr);
    CHECK_EQ(array->size(), std::size_t{4});
    CHECK_NEAR(*(*array)[1].asNumber(), 2.5);
    CHECK(*(*array)[2].asBool());
    CHECK((*array)[3].isNull());

    // é is two bytes of UTF-8, and the tab survives as one character.
    CHECK_EQ(*value->find("b")->asString(), std::string("x\xc3\xa9\ty"));

    json::ParseError error;
    CHECK(!json::parse("{\"unterminated\": ", &error).has_value());
    CHECK(!error.message.empty());
    CHECK(!json::parse("{} trailing").has_value());
}
