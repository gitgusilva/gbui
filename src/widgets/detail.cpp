#include "detail.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "gbui/input/textEdit.hpp"
#include "gbui/widgets/text.hpp"

namespace gbui::detail {

/**
 * Every control dims the same way, so the rule lives in one place.
 *
 * Not as far as it used to. Opacity alone was doing the whole job at 45%, and
 * that fails twice: a checked checkbox at 45% still reads as checked, because
 * the accent is still the loudest thing in the row — and the label under it
 * stops being readable, which is the one thing a disabled control still has to
 * be. So the colour now carries the meaning (see `disabledPalette`) and the
 * dimming is only the finishing touch.
 */
float opacityFor(bool disabled) { return disabled ? 0.7f : 1.0f; }

/** What a control wears when it is disabled, whatever it looked like enabled:
 *  the elevated surface, a plain border and muted text. One rule, so a disabled
 *  checkbox, select and button are recognisably the same state. */
FieldPalette disabledPalette() {
    return {Fill{Token::BgElevated}, Token::TextMuted, Token::Border};
}

/** The ring that says where the keyboard is. Drawn as the control's own border
 *  in the accent rather than as a separate outline, because a second rectangle
 *  around a 16-pixel checkbox reads as noise. */
Border ringFor(bool ring, bool hovered) {
    if (ring) return Border{2.0f, Fill{Token::Accent}};
    return Border{1.0f, Fill{hovered ? Token::BorderStrong : Token::Border}};
}

/** One bullet per character, for a password field. */
std::string bulletsFor(std::string_view text) {
    std::string out;
    for (std::size_t i = 0; i < text.size(); i = nextCharacter(text, i)) out += "•";
    return out;
}

/** How many characters — not bytes — come before `offset`. */
std::size_t countCharacters(std::string_view text, std::size_t offset) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < offset && i < text.size(); i = nextCharacter(text, i)) ++count;
    return count;
}

/** The byte offset `count` characters into `text`. The inverse of the above. */
std::size_t characterAt(std::string_view text, std::size_t count) {
    std::size_t offset = 0;
    for (std::size_t i = 0; i < count && offset < text.size(); ++i) {
        offset = nextCharacter(text, offset);
    }
    return offset;
}

/**
 * Which character boundary a click at `x` lands on, `x` measured from the start
 * of the run.
 *
 * The same measurement the caret is drawn with, run the other way: prefixes are
 * measured until one passes the point. A click lands on the nearer edge of the
 * character it is inside, which is what makes clicking the right half of a
 * letter put the caret after it rather than before.
 *
 * Prefix by prefix rather than character by character, because the width of a
 * run is not the sum of its glyphs — the same reason the caret is measured that
 * way. A field holds a line, so the cost is a click's worth of measuring.
 */
std::size_t offsetAtX(const Ui& ui, std::string_view text, const TextStyle& style, float x) {
    if (text.empty() || !ui.canMeasure()) return 0;
    if (x <= 0.0f) return 0;

    float previous = 0.0f;
    std::size_t start = 0;
    for (std::size_t end = nextCharacter(text, 0); end <= text.size();
         end = nextCharacter(text, end)) {
        const float width = ui.measure(text.substr(0, end), style).width;
        if (x < (previous + width) / 2.0f) return start;
        if (end >= text.size()) break;
        previous = width;
        start = end;
    }
    return text.size();
}

/** Space between a control and its label. */
void labelGap(Ui& ui) {
    Style spacing;
    spacing.width = 8.0f;
    spacing.shrink = 0.0f;
    ui.add(spacing);
}

std::string formatNumber(double value, int decimals, std::string_view suffix) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*f", std::max(0, decimals), value);
    std::string out(buffer);
    out.append(suffix);
    return out;
}

double clampAndSnap(double value, double minimum, double maximum, double step) {
    if (step > 0.0) value = std::round(value / step) * step;
    return std::clamp(value, minimum, maximum);
}

/** A control is activated by clicking it or by pressing Space or Return while
 *  it has the keyboard. Both paths land here so no control invents its own. */
bool activated(const Interaction& input, std::string_view id, bool disabled) {
    if (disabled) return false;
    if (input.clicked(id)) return true;
    if (!input.isFocused(id)) return false;
    for (const KeyEvent& event : input.keys()) {
        if (event.key == Key::Space || event.key == Key::Return) return true;
    }
    return false;
}

/** Opens the row a control and its label share. */
Ui::Scope beginControlRow(Ui& ui, std::string_view id, bool disabled) {
    Style row;
    row.direction = Direction::Row;
    row.align = Align::Center;
    row.opacity = opacityFor(disabled);
    // The whole row is the target — clicking the label toggles the control —
    // so the whole row says so, and says when it will not.
    row.cursorHint = disabled ? Cursor::NotAllowed : Cursor::Pointer;
    auto scope = ui.begin(row);
    ui.tag(id).focusable(!disabled).cursor(row.cursorHint);
    return scope;
}

// Disabled is not just dimmer: a control that only fades still looks editable
// on a dark theme. It gets its own surface and muted text, so the difference
// reads at a glance rather than by comparison. Read-only keeps normal text —
// the value matters and is meant to be read — but drops the editable surface,
// because a field you cannot type in should not look like one you can.
FieldPalette paletteForField(bool disabled, bool readOnly, bool hovered) {
    if (disabled) return {Fill{Token::BgElevated}, Token::TextMuted, Token::Border};
    if (readOnly) return {Fill{Token::BgElevated}, Token::Text, Token::Border};
    return {Fill{Token::Bg}, Token::Text, hovered ? Token::BorderStrong : Token::Border};
}

/** The window to keep a floating box inside. An empty `bounds` means "whatever
 *  the last layout used", which the interaction layer remembers as the root's
 *  own frame. */
Rect boundsFor(const Interaction& input, const FloatingOptions& options) {
    if (!options.bounds.empty()) return options.bounds;
    const Rect root = input.viewport();
    return root.empty() ? Rect{0, 0, 1920, 1080} : root;
}

PlacementOptions placementOptionsFrom(const FloatingOptions& options) {
    return {options.placement, options.gap, options.margin, options.flip, options.shift};
}

/** Opens an absolutely positioned surface in a layer above the content. */
Ui::Scope beginFloating(Ui& ui, const Rect& rect, Layer layer, const Edges& padding,
                        float gapBetweenItems, Direction direction, float maxHeight) {
    Style surface;
    surface.position = Position::Fixed;
    surface.layer = layer;
    surface.left = rect.x;
    surface.top = rect.y;
    surface.width = rect.width;
    if (rect.height > 0.0f) surface.height = rect.height;
    surface.direction = direction;
    surface.padding = padding;
    surface.gap = gapBetweenItems;
    surface.maxHeight = maxHeight;
    surface.background = Fill{Token::BgOverlay};
    surface.border = Border{1.0f, Fill{Token::BorderStrong}};
    return ui.begin(surface);
}

/** Rough size of a floating box before layout has run.
 *
 * Placement needs a size and the tree does not have one yet, so the first frame
 * estimates and the second corrects. The estimate is deliberately close: a
 * tooltip that jumps by a pixel on its second frame is invisible, one that
 * jumps by fifty is not. */
Vec2 estimateSize(const Interaction& input, std::string_view id, Vec2 fallback) {
    const Rect known = input.frameOf(id);
    if (known.width > 0.0f && known.height > 0.0f) return {known.width, known.height};
    return fallback;
}

float measureTextWidth(std::string_view text, float fontSize) {
    // The same 0.52 em the layout engine falls back to. Only used until the
    // real geometry arrives on the next frame.
    std::size_t glyphs = 0;
    for (const char c : text) {
        if ((static_cast<unsigned char>(c) & 0xC0U) != 0x80U) ++glyphs;
    }
    return static_cast<float>(glyphs) * fontSize * 0.52f;
}

}  // namespace gbui::detail
