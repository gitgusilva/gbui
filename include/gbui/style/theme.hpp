// The design system, loaded from the same theme.json the Vue app and the
// gitbox-themes registry already publish. Sharing the file is the point: a
// theme written for the Electron app has to look right here without being
// ported, or the registry stops being an asset the moment a native UI exists.
//
// The token list mirrors gitbox-themes/schema/theme.schema.json exactly — all
// 24 colours are required there, so a Theme either has every one of them or
// fails to load and says which is missing.
#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "gbui/core/color.hpp"
#include "gbui/core/json.hpp"

namespace gbui {

enum class Token {
    Bg, BgElevated, BgOverlay, SurfaceHover,
    Border, BorderStrong,
    TextStrong, Text, TextMuted,
    Accent, AccentHover, AccentFg,
    Added, Removed, Modified,
    Graph1, Graph2, Graph3, Graph4, Graph5, Graph6, Graph7, Graph8, GraphMarker,
    Count,
};

/** The schema's name for a token, e.g. Token::BgElevated -> "bgElevated". */
std::string_view tokenName(Token token);

/** The reverse; nothing when the name is not part of the contract. */
std::optional<Token> tokenFromName(std::string_view name);

struct Typography {
    std::string uiFont;
    float uiFontSize = 13.0f;
    std::string monoFont;
    std::string editorFont;
    float editorFontSize = 13.0f;
    /** 0 means "let the renderer choose", which is what the CSS variable does. */
    float editorLineHeight = 0.0f;
    float radius = 6.0f;
};

class Theme {
public:
    /** The colours GitBox ships with, so a caller can draw before any file is
     *  read — tests, previews, and the first frame while a theme loads. */
    static Theme dark();
    static Theme light();

    /**
     * The palettes of the three design systems everyone recognises, mapped onto
     * this schema's 24 tokens.
     *
     * They are approximations *by necessity*: each of those systems has a
     * hundred-odd roles and this has twenty-four, so the mapping picks the
     * nearest one — Material's `surface-container` for `bgElevated`, its
     * `on-surface-variant` for `textMuted`, and so on. The colours themselves
     * are each system's published values. They exist to prove the toolkit
     * re-themes convincingly and to compare the same components side by side,
     * not to certify anything as a compliant implementation.
     */
    static Theme material(bool darkMode = true);
    static Theme cupertino(bool darkMode = true);
    static Theme fluent(bool darkMode = true);

    /** Reads a parsed theme.json. On failure `error` names the field at fault
     *  rather than the file, because the file is usually community-authored and
     *  the author needs to know which token is wrong. */
    static std::optional<Theme> fromJson(const json::Value& root, std::string* error = nullptr);
    static std::optional<Theme> fromFile(const std::string& path, std::string* error = nullptr);

    Color color(Token token) const { return colors_[static_cast<std::size_t>(token)]; }
    void setColor(Token token, Color value) { colors_[static_cast<std::size_t>(token)] = value; }

    const Typography& typography() const { return typography_; }
    Typography& typography() { return typography_; }

    const std::string& id() const { return id_; }
    const std::string& name() const { return name_; }
    bool isDark() const { return dark_; }

    /** The lane colour for a graph column, wrapping the way the commit graph
     *  does — eight tokens, any number of lanes. */
    Color graphLane(std::size_t index) const {
        constexpr std::size_t kLanes = 8;
        const auto first = static_cast<std::size_t>(Token::Graph1);
        return colors_[first + (index % kLanes)];
    }

    /** Foreground that stays legible on a filled surface. The theme's own
     *  accentFg is preferred and only overridden when it fails against the
     *  fill — a community theme that pairs white on pale yellow should not
     *  render an unreadable button. */
    Color onAccent() const;

    /**
     * The colour a focus ring is drawn in.
     *
     * The accent is the obvious choice and the wrong one on its own: a focus
     * ring drawn in the accent, around a control filled with the accent, is
     * invisible — which is precisely the control a keyboard user is most likely
     * to be on. So the ring is the accent when it stands out against the
     * surface behind it, and the strongest text colour when it does not.
     *
     * `surface` is what the ring will be drawn over.
     */
    Color focusRing(Color surface) const;

private:
    std::array<Color, static_cast<std::size_t>(Token::Count)> colors_{};
    Typography typography_{};
    std::string id_;
    std::string name_;
    bool dark_ = true;
};

}  // namespace gbui
