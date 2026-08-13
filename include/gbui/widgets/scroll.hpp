// A scroll container.
#pragma once

#include <cstddef>
#include <optional>
#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/style/style.hpp"

namespace gbui {

/** Where a scroll container is, and how far it can go.
 *
 * Owned by the application, like every other piece of state: the toolkit reads
 * it, reports what changed, and never keeps a copy. `contentSize` and
 * `viewportSize` are written by the component from the last frame's geometry,
 * so a caller can show "40 of 5000" without measuring anything itself. */
struct ScrollState {
    float offset = 0.0f;
    float contentSize = 0.0f;
    float viewportSize = 0.0f;

    float maxOffset() const {
        const float slack = contentSize - viewportSize;
        return slack > 0.0f ? slack : 0.0f;
    }
    bool scrollable() const { return maxOffset() > 0.0f; }
    /** 0 at the top, 1 at the bottom. */
    float progress() const { return scrollable() ? offset / maxOffset() : 0.0f; }
};

/**
 * Which way a view scrolls, independently of how its content is laid out.
 *
 * `None` still clips: a box that must not grow past a size but must not scroll
 * either is a real thing — a dropdown pinned to a maximum, a cell in a table —
 * and it is not the same as no container at all.
 *
 * Both axes at once is *not* built. It needs a second offset in `ScrollState`
 * and a second bar, and every caller so far scrolls one way; saying so here is
 * better than a value that silently scrolls in one direction only.
 */
enum class ScrollAxis { None, Vertical, Horizontal };

/**
 * Draws the bar for a scroll view that is not where the bar belongs.
 *
 * A bar normally lives inside the view it drives, and `beginScroll` puts it
 * there. A table is the case that forced this one out: its rows scroll
 * vertically *inside* a box that scrolls horizontally, so the row view's own
 * right-hand edge is out at the end of the widest column, and the bar drawn
 * against it is a bar the reader has to scroll sideways to find. A browser has
 * no such problem, because both bars belong to the visible box; this is how a
 * caller says the same thing here.
 *
 * `box` is in the current container's coordinates — where the bar should be, not
 * where the content is. The ids are the view's own, so the press, the drag and
 * the paging are still handled by `beginScroll`: this draws, it does not
 * behave. Turn the view's own bar off with `ScrollOptions::scrollbar` when you
 * use it, or there will be two.
 */
void scrollbar(Ui& ui, const Interaction& input, std::string_view id, const ScrollState& state,
               Rect box, ScrollAxis axis = ScrollAxis::Vertical, float width = 10.0f,
               bool autoHide = true);

struct ScrollOptions {
    Direction direction = Direction::Column;
    /** Which way it scrolls. Auto follows `direction`, which is what a column
     *  of rows or a row of chips almost always wants. */
    std::optional<ScrollAxis> axis{};
    /** Pixels per wheel notch. */
    float step = 48.0f;
    /** Draws the bar. Turning it off leaves wheel and keyboard scrolling. */
    bool scrollbar = true;
    float scrollbarWidth = 10.0f;
    /** Hides the bar when the content fits, the way a desktop does. */
    bool autoHideScrollbar = true;
    /** Whether Tab can land on the viewport. A pane the reader scrolls for
     *  itself wants that; a list inside a control that already holds the
     *  keyboard — a select — does not, or Tab walks into the popup. */
    bool focusable = true;
    Edges padding{};
    float gap = 0.0f;
    float grow = 1.0f;
    float width = kAuto;
    float height = kAuto;
    /** Bounds on the viewport itself. A maximum is what turns "as tall as its
     *  content" into "as tall as its content, then scroll" — which is the rule
     *  a dropdown needs and could not express before. */
    float minWidth = kAuto;
    float maxWidth = kAuto;
    float minHeight = kAuto;
    float maxHeight = kAuto;

    /** The axis this actually scrolls along, resolved from `axis` and
     *  `direction`. */
    ScrollAxis resolvedAxis() const {
        if (axis) return *axis;
        return direction == Direction::Column ? ScrollAxis::Vertical : ScrollAxis::Horizontal;
    }
};

/**
 * Opens a scroll container.
 *
 * The content is laid out at its natural size and clipped to the viewport, and
 * the offset moves it. Wheel over the container scrolls it; Page Up, Page Down,
 * Home and End do too when it has focus; the bar can be dragged.
 *
 * Everything drawn inside costs a node, so this is a *scrolling* view and not a
 * *virtualised* one — 50 000 rows here really do build 50 000 nodes. When the
 * rows are uniform, `virtualList` builds only the visible ones — see
 * virtualList.hpp.
 */
Ui::Scope beginScroll(Ui& ui, const Interaction& input, std::string_view id, ScrollState& state,
                      const ScrollOptions& options = {});

/** Rows of one height, laid out one after another. Two things reason about
 *  that shape — the virtualised list, and anything scrolling a highlighted row
 *  into view — so it is one struct rather than two sets of arguments. */
struct RowMetrics {
    float height = 28.0f;
    float gap = 0.0f;
    /** Padding above the first row, which offsets every row after it. */
    float top = 0.0f;

    /** Top of one row to the top of the next. */
    float pitch() const { return height + gap; }
};

/** Scrolls the least distance that brings a row fully into view, and does
 *  nothing when it already is: a row one line below the fold moves one line
 *  rather than jumping the list under the reader. What arrow-key navigation
 *  over a list needs, whether that list is virtualised or not. */
void revealRow(ScrollState& state, const RowMetrics& rows, std::size_t index);

}  // namespace gbui
