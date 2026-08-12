// Style: what a node wants, before layout decides what it gets.
//
// The vocabulary is deliberately CSS flexbox, because the UI this library has
// to reproduce is written in it. Anything CSS calls `auto` is NaN here (see
// geometry.hpp), which keeps Style an aggregate that can be written inline at a
// call site without a builder.
#pragma once

#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "gbui/core/color.hpp"
#include "gbui/core/geometry.hpp"
#include "gbui/core/cursor.hpp"
#include "gbui/style/theme.hpp"

namespace gbui {

enum class Direction { Row, Column };
enum class Justify { Start, Center, End, SpaceBetween, SpaceAround, SpaceEvenly };
enum class Align { Start, Center, End, Stretch, Baseline };

/** How the *lines* of a wrapping container are distributed across it. Only
 *  consulted when `wrap` is on and there is more than one line — with a single
 *  line there is nothing to distribute. */
enum class AlignContent { Start, Center, End, Stretch, SpaceBetween, SpaceAround };
/**
 * What happens to a child that does not fit.
 *
 * `Scroll` clips exactly as `Hidden` does — the difference is not visual. It
 * declares that this node *consumes the wheel*, which is what lets the pointer
 * route a scroll to the innermost thing under it rather than to everything
 * under it at once. A page that scrolls, containing a list that scrolls,
 * containing a chart that zooms, is three nodes competing for one wheel event,
 * and without a declaration there is no way to tell which one meant it.
 */
enum class Overflow {
    Visible,
    Hidden,
    /** Clips, and takes the plain wheel. */
    Scroll,
    /** Clips, and takes Shift and the wheel.
     *
     * A separate value rather than a flag beside it because the pointer has to
     * resolve *two* targets under itself — the innermost thing that scrolls up
     * and down, and the innermost that scrolls side to side — and they are
     * rarely the same node. A table is exactly that: the rows scroll
     * vertically inside a box whose columns scroll horizontally. */
    ScrollX,
};

/** Whether a node clips its children, however it says so. */
constexpr bool clips(Overflow overflow) { return overflow != Overflow::Visible; }

/** Where a node sits in the flow, and what it is positioned against.
 *
 * The distinction is CSS's, and it is the one that matters: a caret belongs to
 * the field it blinks in and should travel with it, while a menu has to escape
 * whatever built it. Anchoring the first to the window is what makes it lag a
 * frame behind whenever its container moves. */
enum class Position {
    Relative,  ///< in the flex flow, like everything else
    Absolute,  ///< out of the flow, at `left`/`top` inside the parent's content box
    Fixed,     ///< out of the flow, at `left`/`top` in the window
};

/** Whether a node is out of the flex flow, whichever way it is anchored. */
constexpr bool isFloating(Position position) { return position != Position::Relative; }

/**
 * Which layer a subtree is painted in.
 *
 * Content is everything ordinary. Overlay is drawn after all content, in the
 * order the overlays were built, which is what lets a menu escape the panel
 * that opened it. Modal is drawn after that, so a dialog covers a menu.
 *
 * Layers exist because painting order in a tree is depth-first, and a popup
 * opened deep in a sidebar would otherwise be painted under the pane beside it.
 */
enum class Layer { Content, Overlay, Modal };

/** A colour that usually comes from the theme and occasionally does not.
 *  Naming the token rather than the value is what lets a whole tree re-theme
 *  without being rebuilt. */
struct Fill {
    std::optional<Token> token{};
    std::optional<Color> color{};  ///< Overrides the token when both are set.
    float alpha = 1.0f;

    constexpr Fill() = default;
    constexpr Fill(Token t, float a = 1.0f) : token(t), alpha(a) {}
    constexpr Fill(Color c, float a = 1.0f) : color(c), alpha(a) {}

    bool empty() const { return !token && !color; }
    Color resolve(const Theme& theme) const {
        const Color base = color ? *color : (token ? theme.color(*token) : Color{});
        return base.withAlpha(base.a * alpha);
    }
};


/** How a gradient's stops are laid across the shape it fills. */
enum class GradientKind {
    Linear,  ///< along `angle`
    Radial,  ///< out from the centre
};

/** A colour at a position along the gradient, 0 to 1. */
struct GradientStop {
    float position = 0.0f;
    Fill color{};
};

/**
 * A gradient, as a property of a Fill rather than a separate kind of paint.
 *
 * Stops carry `Fill`s, not colours, so a gradient is themeable like everything
 * else: `accent -> accent at 0%` is a fade a theme can restyle, where two
 * literals would not be.
 *
 * `angle` is in degrees and follows CSS: 0 points up, 90 points right, so
 * `180` is the familiar top-to-bottom.
 */
struct Gradient {
    GradientKind kind = GradientKind::Linear;
    float angle = 180.0f;
    std::vector<GradientStop> stops;

    bool empty() const { return stops.size() < 2; }

    /** Two stops, the common case. */
    static Gradient linear(Fill from, Fill to, float angle = 180.0f) {
        return {GradientKind::Linear, angle, {{0.0f, from}, {1.0f, to}}};
    }
    static Gradient radial(Fill centre, Fill edge) {
        return {GradientKind::Radial, 0.0f, {{0.0f, centre}, {1.0f, edge}}};
    }
};

/** Which of the theme's font families a run of text belongs to. The concrete
 *  family and size come from Typography, so a theme change moves them all. */
enum class FontRole { Ui, Mono, Editor };
/** Numeric weights, as CSS names them, so a face can be matched by distance
 *  rather than by an exact name nobody's font files agree on. */
enum class FontWeight {
    Thin = 100, ExtraLight = 200, Light = 300, Regular = 400, Medium = 500,
    SemiBold = 600, Bold = 700, ExtraBold = 800, Black = 900,
};

/** Upright or italic. `Oblique` is not a separate value: no UI needs to insist
 *  on a slanted roman rather than a true italic, and every face that has one
 *  has the other. */
enum class FontSlant { Normal, Italic };
enum class TextAlign { Start, Center, End };

/** What to do with a run that does not fit its box. Ellipsis is the default
 *  because almost every string in a Git client is longer than its column — a
 *  commit subject, a branch name, a path. */
enum class TextOverflow { Ellipsis, Clip, Wrap };

/**
 * Where a line is allowed to end.
 *
 * CSS splits this across `word-break` and `overflow-wrap`, which between them
 * have five values and three that mean roughly the same thing. The three here
 * are the three that behave differently.
 */
enum class WordBreak {
    /**
     * At spaces, and after a hyphen or a slash. A word too long for the line is
     * broken between characters rather than left to overflow.
     *
     * The hyphen and slash opportunities are what make a path or a URL wrap
     * where a reader would expect — after a separator — instead of being cut
     * mid-segment by the emergency rule.
     */
    Normal,
    /**
     * Only at spaces. A word longer than the line overflows its box.
     *
     * For identifiers a reader has to be able to select and copy whole: a
     * commit hash broken across two lines is no longer a commit hash.
     */
    KeepAll,
    /**
     * Between any two characters.
     *
     * For text with no spaces to break at — base64, a long hash, CJK — where
     * filling the box matters more than keeping runs together.
     */
    Anywhere,
};


/**
 * A line through a run: under it, across it, or both.
 *
 * A separate struct rather than two bools on `TextStyle` because a decoration
 * has its own colour and weight in every system that has one — CSS spells them
 * `text-decoration-color` and `-thickness` — and a strikethrough in the removed
 * colour over text in the normal colour is exactly what a diff wants.
 */
struct TextDecoration {
    bool underline = false;
    bool strikeThrough = false;
    /** Zero derives it from the font size, which keeps a rule under 11-pixel
     *  text from being as heavy as one under 18-pixel text. */
    float thickness = 0.0f;
    /** Empty takes the run's own colour. */
    Fill color{};

    bool any() const { return underline || strikeThrough; }
};

struct TextStyle {
    FontRole role = FontRole::Ui;
    FontWeight weight = FontWeight::Regular;
    FontSlant slant = FontSlant::Normal;
    TextAlign align = TextAlign::Start;
    /** NaN takes the size from the theme's typography for this role. */
    float size = kAuto;
    Fill color{Token::Text};
    /** Painted instead of `color` when it has two or more stops — a heading
     *  that fades, a value that shifts hue across its range. */
    Gradient colorGradient{};
    /** Multiplier over the font size; 0 lets the painter use its own default. */
    float lineHeight = 0.0f;
    TextOverflow overflow = TextOverflow::Ellipsis;
    /** Only consulted when overflow is Wrap. Zero or less is unlimited; a
     *  positive count clamps the run and ends the last line with an ellipsis,
     *  as CSS's `line-clamp` does. */
    int maxLines = 0;
    /** Where a wrapped line may end. Only consulted when overflow is Wrap. */
    WordBreak wordBreak = WordBreak::Normal;
    TextDecoration decoration{};
};

struct Border {
    float width = 0.0f;
    Fill color{Token::Border};

    bool visible() const { return width > 0.0f && !color.empty(); }
};

/** An outline drawn *outside* the border box.
 *
 * Separate from `Border` on purpose, exactly as CSS separates them: a border
 * takes space and moves the content inside it; an outline does not, so showing
 * one on focus cannot make the control jump — which is the single most common
 * way a focus ring gets implemented badly.
 *
 * The default is two pixels at two pixels of offset, which clears WCAG 2.2's
 * focus-appearance minimum on a control of any size. */
struct Outline {
    float width = 0.0f;
    float offset = 2.0f;
    Fill color{Token::Accent};

    bool visible() const { return width > 0.0f && !color.empty(); }
};

struct Style {
    // ---- flex container -------------------------------------------------
    Direction direction = Direction::Row;
    Justify justify = Justify::Start;
    Align align = Align::Stretch;
    float gap = 0.0f;
    /**
     * Lets items that do not fit start a new line, as CSS's `flex-wrap: wrap`.
     *
     * Off by default and deliberately: a toolbar that silently becomes two rows
     * when it runs out of room is usually a bug, and everything built so far
     * expects to overflow or shrink instead. Turning it on is what makes a row
     * of tags, chips or theme cards reflow rather than crush.
     */
    bool wrap = false;
    AlignContent alignContent = AlignContent::Start;
    /** Space between lines. Auto uses `gap`, so one number is enough for the
     *  usual grid. */
    float crossGap = kAuto;

    // ---- flex item ------------------------------------------------------
    float grow = 0.0f;
    float shrink = 1.0f;
    /** Main-axis starting size; auto falls back to width/height, then content.
     *  A percentage resolves against the container's content box. */
    Length basis = kAuto;
    /** Overrides the container's align for this item alone. */
    std::optional<Align> alignSelf{};

    // ---- box ------------------------------------------------------------
    //
    // Each of these is a `Length`, so a plain number is still pixels and
    // `Length::percent(25)` is a quarter of the container — which is what lets
    // a split pane finally say "a quarter, up to 320".
    Length width = kAuto;
    Length height = kAuto;
    /** Auto means the content minimum, as `min-width: auto` does in CSS: an
     *  item does not shrink below what it needs. Write 0 to allow it to. */
    Length minWidth = kAuto;
    Length minHeight = kAuto;
    Length maxWidth = std::numeric_limits<float>::infinity();
    Length maxHeight = std::numeric_limits<float>::infinity();
    Edges padding{};
    Edges margin{};

    // ---- position -------------------------------------------------------
    Position position = Position::Relative;
    Layer layer = Layer::Content;
    /**
     * Order among siblings, painted low to high; ties keep tree order.
     *
     * `Layer` and this answer different questions. A layer is *which pass* a
     * subtree is drawn in — it is how a menu escapes the panel that opened it,
     * and it applies to a whole subtree. This is ordering **within** one pass,
     * among children of the same parent, which is what CSS's `z-index` is: a
     * badge over an avatar, a highlight under a row, a handle over a track.
     * Reaching for a layer to do that would lift the node out over everything
     * else on screen.
     *
     * Hit testing follows the same order in reverse, so what looks on top is
     * what the pointer finds.
     */
    int zIndex = 0;
    /** Only read when `position` is Absolute. */
    float left = kAuto;
    float top = kAuto;

    // ---- paint ----------------------------------------------------------
    Fill background{};
    /** Painted instead of `background` when it has two or more stops. */
    Gradient backgroundGradient{};
    Border border{};
    Outline outline{};
    /** NaN takes the theme's radius, which is what almost every surface wants. */
    float radius = kAuto;
    Overflow overflow = Overflow::Visible;
    float opacity = 1.0f;
    /** What the pointer should look like here. Copied onto the node by
     *  `Ui::cursor`, and kept in Style so a component can decide it in one
     *  place beside the rest of its appearance. */
    Cursor cursorHint = Cursor::Default;
};

}  // namespace gbui
