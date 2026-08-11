// The layout engine: CSS flexbox, the subset this UI is written in.
//
// It is deliberately pure — arena in, frames out, no window, no GPU, no font
// file — which is what makes it unit-testable. Text is the one thing layout
// cannot compute on its own, so measuring is a callback the caller supplies;
// the default is a metric approximation good enough for tests and previews, and
// a real backend replaces it with the shaper's answer.
//
// Not implemented, and left out on purpose rather than half-done: aspect-ratio,
// baseline alignment (Align::Baseline currently behaves as Align::Start),
// `right`/`bottom` for out-of-flow boxes, and percentages on anything but a
// size — padding, margin, gap and left/top are absolute.
#pragma once

#include <functional>

#include "gbui/style/theme.hpp"
#include "gbui/scene/tree.hpp"

namespace gbui {

/** What a run of text occupies. `baseline` is the distance from the top of the
 *  line box to the alphabetic baseline. */
struct TextMetrics {
    float width = 0.0f;
    float height = 0.0f;
    float baseline = 0.0f;
};

/**
 * Measures a string. `maxWidth` is the space available — infinity when the
 * caller wants the intrinsic width. A backend with a real font engine is
 * expected to shape here; the built-in one approximates.
 */
using MeasureText = std::function<TextMetrics(std::string_view text, const TextStyle&,
                                              const Typography&, float maxWidth)>;

/** Advance-width approximation used when no font engine is available. It is
 *  honest about being an estimate: proportional text is assumed to average 0.52
 *  em and mono 0.60 em, which is close enough to lay out a preview and stable
 *  enough to assert on in a test. */
TextMetrics approximateTextMetrics(std::string_view text, const TextStyle& style,
                                   const Typography& typography, float maxWidth);

struct LayoutContext {
    const Theme* theme = nullptr;
    MeasureText measure = &approximateTextMetrics;
};

/**
 * Positions `root` and everything under it inside `viewport`, writing absolute
 * frames into each node. Runs in two passes over the tree — intrinsic sizes,
 * then placement — and allocates nothing.
 */
void layout(Arena& arena, NodeId root, const Rect& viewport, const LayoutContext& context);

/** Intrinsic main-axis size of a node, ignoring grow and shrink. Exposed
 *  because a scroll container needs its content's natural height before it can
 *  decide whether a scrollbar is warranted. */
float intrinsicMainSize(const Arena& arena, NodeId id, const LayoutContext& context);

/** The deepest node whose frame contains `point`, or an invalid id. Respects
 *  Overflow::Hidden, so a child scrolled out of its parent is not hit. */
NodeId hitTest(const Arena& arena, NodeId root, Vec2 point);

}  // namespace gbui
