#include "gbui/style/theme.hpp"

#include <algorithm>
#include <array>

namespace gbui {
namespace {

// Index-aligned with Token, so tokenName is a lookup and not a switch that can
// drift out of order.
constexpr std::array<std::string_view, static_cast<std::size_t>(Token::Count)> kNames{
    "bg", "bgElevated", "bgOverlay", "surfaceHover",
    "border", "borderStrong",
    "textStrong", "text", "textMuted",
    "accent", "accentHover", "accentFg",
    "added", "removed", "modified",
    "graph1", "graph2", "graph3", "graph4", "graph5", "graph6", "graph7", "graph8", "graphMarker",
};

bool readNumber(const json::Value& parent, std::string_view key, float& out, std::string* error) {
    const auto* value = parent.find(key);
    if (!value) {
        if (error) *error = std::string("typography.") + std::string(key) + " is missing";
        return false;
    }
    const auto number = value->asNumber();
    if (!number) {
        if (error) *error = std::string("typography.") + std::string(key) + " is not a number";
        return false;
    }
    out = static_cast<float>(*number);
    return true;
}

bool readString(const json::Value& parent, std::string_view key, std::string& out, std::string* error) {
    const auto* value = parent.find(key);
    const auto* text = value ? value->asString() : nullptr;
    if (!text) {
        if (error) *error = std::string("typography.") + std::string(key) + " is missing";
        return false;
    }
    out = *text;
    return true;
}

}  // namespace

std::string_view tokenName(Token token) {
    return kNames[static_cast<std::size_t>(token)];
}

std::optional<Token> tokenFromName(std::string_view name) {
    for (std::size_t i = 0; i < kNames.size(); ++i) {
        if (kNames[i] == name) return static_cast<Token>(i);
    }
    return std::nullopt;
}

Color Theme::onAccent() const {
    const Color fill = color(Token::Accent);
    const Color declared = color(Token::AccentFg);
    constexpr float kMinimumContrast = 4.5f;  // WCAG AA for body text
    if (Color::contrast(fill, declared) >= kMinimumContrast) return declared;
    return fill.readableOn(Color{255, 255, 255}, Color{17, 17, 17});
}

Color Theme::focusRing(Color surface) const {
    constexpr float kMinimumContrast = 3.0f;  // WCAG 2.2 for a non-text indicator
    const Color accent = color(Token::Accent);
    if (Color::contrast(surface, accent) >= kMinimumContrast) return accent;

    // Whichever of the theme's extremes stands out most against the surface.
    const Color strong = color(Token::TextStrong);
    const Color background = color(Token::Bg);
    return Color::contrast(surface, strong) >= Color::contrast(surface, background) ? strong
                                                                                   : background;
}

Theme Theme::dark() {
    Theme t;
    t.id_ = "gitbox-dark";
    t.name_ = "GitBox Dark";
    t.dark_ = true;
    // The values the app ships as :root in style.css.
    t.setColor(Token::Bg, {30, 30, 30});
    t.setColor(Token::BgElevated, {37, 37, 38});
    t.setColor(Token::BgOverlay, {45, 45, 45});
    t.setColor(Token::SurfaceHover, {42, 42, 43});
    t.setColor(Token::Border, {45, 45, 45});
    t.setColor(Token::BorderStrong, {58, 58, 58});
    t.setColor(Token::TextStrong, {242, 242, 242});
    t.setColor(Token::Text, {204, 204, 204});
    t.setColor(Token::TextMuted, {138, 138, 138});
    t.setColor(Token::Accent, {37, 99, 235});
    t.setColor(Token::AccentHover, {59, 130, 246});
    t.setColor(Token::AccentFg, {255, 255, 255});
    t.setColor(Token::Added, {34, 197, 94});
    t.setColor(Token::Removed, {239, 68, 68});
    t.setColor(Token::Modified, {87, 176, 255});
    t.setColor(Token::Graph1, {30, 136, 229});
    t.setColor(Token::Graph2, {255, 171, 0});
    t.setColor(Token::Graph3, {0, 230, 118});
    t.setColor(Token::Graph4, {213, 0, 249});
    t.setColor(Token::Graph5, {255, 61, 0});
    t.setColor(Token::Graph6, {0, 176, 255});
    t.setColor(Token::Graph7, {29, 233, 182});
    t.setColor(Token::Graph8, {245, 0, 87});
    t.setColor(Token::GraphMarker, {255, 255, 255});
    t.typography_ = Typography{
        "IBM Plex Sans", 13.0f, "IBM Plex Mono", "IBM Plex Mono", 13.0f, 0.0f, 6.0f,
    };
    return t;
}

namespace {

/** The graph lanes and the diff colours a palette does not define for itself.
 *  Every design system below names an accent and a set of surfaces; none of
 *  them has an opinion about eight commit-graph lanes, so they share one. */
void applyGraphAndDiff(Theme& t, bool darkMode) {
    t.setColor(Token::Added, darkMode ? Color{77, 182, 122} : Color{22, 128, 78});
    t.setColor(Token::Removed, darkMode ? Color{237, 106, 106} : Color{200, 40, 40});
    t.setColor(Token::Modified, darkMode ? Color{99, 168, 245} : Color{28, 110, 200});
    t.setColor(Token::Graph1, {30, 136, 229});
    t.setColor(Token::Graph2, {255, 171, 0});
    t.setColor(Token::Graph3, {0, 230, 118});
    t.setColor(Token::Graph4, {213, 0, 249});
    t.setColor(Token::Graph5, {255, 61, 0});
    t.setColor(Token::Graph6, {0, 176, 255});
    t.setColor(Token::Graph7, {29, 233, 182});
    t.setColor(Token::Graph8, {245, 0, 87});
    t.setColor(Token::GraphMarker, darkMode ? Color{255, 255, 255} : Color{20, 20, 20});
}

}  // namespace

Theme Theme::material(bool darkMode) {
    Theme t;
    t.id_ = darkMode ? "material-dark" : "material-light";
    t.name_ = darkMode ? "Material Dark" : "Material Light";
    t.dark_ = darkMode;
    if (darkMode) {
        // Material 3's baseline dark scheme.
        t.setColor(Token::Bg, {20, 18, 24});             // surface
        t.setColor(Token::BgElevated, {29, 27, 32});     // surface-container
        t.setColor(Token::BgOverlay, {43, 41, 48});      // surface-container-high
        t.setColor(Token::SurfaceHover, {54, 52, 59});
        t.setColor(Token::Border, {73, 69, 79});         // outline-variant
        t.setColor(Token::BorderStrong, {147, 143, 153});// outline
        t.setColor(Token::TextStrong, {230, 224, 233});  // on-surface
        t.setColor(Token::Text, {202, 196, 208});
        t.setColor(Token::TextMuted, {147, 143, 153});   // on-surface-variant
        t.setColor(Token::Accent, {208, 188, 255});      // primary
        t.setColor(Token::AccentHover, {221, 206, 255});
        t.setColor(Token::AccentFg, {56, 30, 114});      // on-primary
    } else {
        t.setColor(Token::Bg, {255, 251, 254});
        t.setColor(Token::BgElevated, {247, 242, 250});
        t.setColor(Token::BgOverlay, {236, 230, 240});
        t.setColor(Token::SurfaceHover, {231, 224, 236});
        t.setColor(Token::Border, {202, 196, 208});
        t.setColor(Token::BorderStrong, {121, 116, 126});
        t.setColor(Token::TextStrong, {28, 27, 31});
        t.setColor(Token::Text, {49, 48, 51});
        t.setColor(Token::TextMuted, {73, 69, 79});
        t.setColor(Token::Accent, {103, 80, 164});
        t.setColor(Token::AccentHover, {122, 98, 184});
        t.setColor(Token::AccentFg, {255, 255, 255});
    }
    applyGraphAndDiff(t, darkMode);
    t.typography_ = Typography{"Roboto", 14.0f, "Roboto Mono", "Roboto Mono", 13.0f, 0.0f, 12.0f};
    return t;
}

Theme Theme::cupertino(bool darkMode) {
    Theme t;
    t.id_ = darkMode ? "cupertino-dark" : "cupertino-light";
    t.name_ = darkMode ? "Cupertino Dark" : "Cupertino Light";
    t.dark_ = darkMode;
    if (darkMode) {
        // iOS system colours, dark appearance.
        t.setColor(Token::Bg, {0, 0, 0});                // systemBackground
        t.setColor(Token::BgElevated, {28, 28, 30});     // secondarySystemBackground
        t.setColor(Token::BgOverlay, {44, 44, 46});      // tertiary
        t.setColor(Token::SurfaceHover, {58, 58, 60});
        t.setColor(Token::Border, {56, 56, 58});         // separator
        t.setColor(Token::BorderStrong, {84, 84, 88});
        t.setColor(Token::TextStrong, {255, 255, 255});  // label
        t.setColor(Token::Text, {235, 235, 245});
        t.setColor(Token::TextMuted, {152, 152, 157});   // secondaryLabel
        t.setColor(Token::Accent, {10, 132, 255});       // systemBlue, dark
        t.setColor(Token::AccentHover, {64, 156, 255});
        t.setColor(Token::AccentFg, {255, 255, 255});
    } else {
        t.setColor(Token::Bg, {242, 242, 247});          // systemGroupedBackground
        t.setColor(Token::BgElevated, {255, 255, 255});
        t.setColor(Token::BgOverlay, {242, 242, 247});
        t.setColor(Token::SurfaceHover, {229, 229, 234});
        t.setColor(Token::Border, {209, 209, 214});
        t.setColor(Token::BorderStrong, {174, 174, 178});
        t.setColor(Token::TextStrong, {0, 0, 0});
        t.setColor(Token::Text, {28, 28, 30});
        t.setColor(Token::TextMuted, {109, 109, 114});
        t.setColor(Token::Accent, {0, 122, 255});        // systemBlue
        t.setColor(Token::AccentHover, {10, 132, 255});
        t.setColor(Token::AccentFg, {255, 255, 255});
    }
    applyGraphAndDiff(t, darkMode);
    t.typography_ = Typography{"SF Pro Text", 13.0f, "SF Mono", "SF Mono", 12.0f, 0.0f, 10.0f};
    return t;
}

Theme Theme::fluent(bool darkMode) {
    Theme t;
    t.id_ = darkMode ? "fluent-dark" : "fluent-light";
    t.name_ = darkMode ? "Fluent Dark" : "Fluent Light";
    t.dark_ = darkMode;
    if (darkMode) {
        // Windows 11 dark: layered greys under a blue accent.
        t.setColor(Token::Bg, {32, 32, 32});
        t.setColor(Token::BgElevated, {43, 43, 43});
        t.setColor(Token::BgOverlay, {55, 55, 55});
        t.setColor(Token::SurfaceHover, {61, 61, 61});
        t.setColor(Token::Border, {66, 66, 66});
        t.setColor(Token::BorderStrong, {90, 90, 90});
        t.setColor(Token::TextStrong, {255, 255, 255});
        t.setColor(Token::Text, {222, 222, 222});
        t.setColor(Token::TextMuted, {160, 160, 160});
        t.setColor(Token::Accent, {96, 205, 255});       // accent light for dark
        t.setColor(Token::AccentHover, {130, 216, 255});
        t.setColor(Token::AccentFg, {0, 32, 47});
    } else {
        t.setColor(Token::Bg, {243, 243, 243});
        t.setColor(Token::BgElevated, {251, 251, 251});
        t.setColor(Token::BgOverlay, {255, 255, 255});
        t.setColor(Token::SurfaceHover, {234, 234, 234});
        t.setColor(Token::Border, {214, 214, 214});
        t.setColor(Token::BorderStrong, {176, 176, 176});
        t.setColor(Token::TextStrong, {23, 23, 23});
        t.setColor(Token::Text, {38, 38, 38});
        t.setColor(Token::TextMuted, {96, 96, 96});
        t.setColor(Token::Accent, {0, 120, 212});
        t.setColor(Token::AccentHover, {0, 103, 192});
        t.setColor(Token::AccentFg, {255, 255, 255});
    }
    applyGraphAndDiff(t, darkMode);
    t.typography_ = Typography{"Segoe UI Variable", 14.0f, "Cascadia Mono", "Cascadia Mono",
                               13.0f, 0.0f, 8.0f};
    return t;
}

Theme Theme::light() {
    Theme t = dark();
    t.id_ = "gitbox-light";
    t.name_ = "GitBox Light";
    t.dark_ = false;
    t.setColor(Token::Bg, {255, 255, 255});
    t.setColor(Token::BgElevated, {246, 248, 250});
    t.setColor(Token::BgOverlay, {255, 255, 255});
    t.setColor(Token::SurfaceHover, {238, 241, 244});
    t.setColor(Token::Border, {208, 215, 222});
    t.setColor(Token::BorderStrong, {175, 184, 193});
    t.setColor(Token::TextStrong, {31, 35, 40});
    t.setColor(Token::Text, {87, 96, 106});
    t.setColor(Token::TextMuted, {110, 119, 129});
    return t;
}

std::optional<Theme> Theme::fromJson(const json::Value& root, std::string* error) {
    const auto* colors = root.find("colors");
    if (!colors || !colors->isObject()) {
        if (error) *error = "colors is missing or not an object";
        return std::nullopt;
    }

    Theme theme;
    for (std::size_t i = 0; i < kNames.size(); ++i) {
        const std::string_view name = kNames[i];
        const auto* entry = colors->find(name);
        const auto* text = entry ? entry->asString() : nullptr;
        if (!text) {
            if (error) *error = "colors." + std::string(name) + " is missing";
            return std::nullopt;
        }
        const auto parsed = parseColor(*text);
        if (!parsed) {
            if (error) *error = "colors." + std::string(name) + " is not a colour: " + *text;
            return std::nullopt;
        }
        theme.colors_[i] = *parsed;
    }

    const auto* typography = root.find("typography");
    if (!typography || !typography->isObject()) {
        if (error) *error = "typography is missing or not an object";
        return std::nullopt;
    }
    Typography typo;
    if (!readString(*typography, "uiFont", typo.uiFont, error)) return std::nullopt;
    if (!readNumber(*typography, "uiFontSize", typo.uiFontSize, error)) return std::nullopt;
    if (!readString(*typography, "monoFont", typo.monoFont, error)) return std::nullopt;
    if (!readString(*typography, "editorFont", typo.editorFont, error)) return std::nullopt;
    if (!readNumber(*typography, "editorFontSize", typo.editorFontSize, error)) return std::nullopt;
    if (!readNumber(*typography, "editorLineHeight", typo.editorLineHeight, error)) return std::nullopt;
    if (!readNumber(*typography, "radius", typo.radius, error)) return std::nullopt;
    theme.typography_ = std::move(typo);

    if (const auto* id = root.find("id"); id && id->asString()) theme.id_ = *id->asString();
    if (const auto* name = root.find("name"); name && name->asString()) theme.name_ = *name->asString();
    if (const auto* type = root.find("type"); type && type->asString()) {
        theme.dark_ = (*type->asString() != "light");
    }
    return theme;
}

std::optional<Theme> Theme::fromFile(const std::string& path, std::string* error) {
    json::ParseError parseError;
    const auto root = json::parseFile(path, &parseError);
    if (!root) {
        if (error) *error = parseError.message.empty() ? ("cannot read " + path) : parseError.message;
        return std::nullopt;
    }
    return fromJson(*root, error);
}

}  // namespace gbui
