#include "gbui/widgets/virtualList.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace gbui {

namespace {

/** The height a spacer needs so the node after it starts `distance` below the
 *  node before it. The container's own gap is already one of those pixels, so
 *  the spacer only supplies the rest. */
float spacerFor(float distance, float gap) { return std::max(0.0f, distance - gap); }

}  // namespace

VirtualSlice virtualList(Ui& ui, const Interaction& input, std::string_view id,
                         ScrollState& state, const VirtualListOptions& options,
                         const std::function<void(Ui&, std::size_t)>& row) {
    ScrollOptions scroll;
    scroll.direction = Direction::Column;
    scroll.step = options.step;
    scroll.scrollbar = options.scrollbar;
    scroll.scrollbarWidth = options.scrollbarWidth;
    scroll.autoHideScrollbar = options.autoHideScrollbar;
    scroll.focusable = options.focusable;
    scroll.padding = options.padding;
    scroll.gap = options.gap;
    scroll.grow = options.grow;
    scroll.width = options.width;
    scroll.height = options.height;

    // Opening the scroll view first is what resolves the wheel, the drag and
    // the keys into `state.offset`, so the slice below is chosen from where the
    // list has just been moved to rather than from where it was.
    auto content = scrollArea(ui, input, id, state, scroll);

    VirtualSlice slice;
    slice.total = options.count;
    slice.pitch = options.rowHeight + options.gap;
    if (options.count == 0 || slice.pitch <= 0.0f) return slice;

    // The first frame has no measured viewport. Falling back to the window's
    // height overshoots — it builds rows a small pane will clip — but a list
    // that renders nothing at all on its first frame is worse, and it is what
    // an offscreen single-frame render would otherwise get.
    const float viewport = state.viewportSize > 0.0f ? state.viewportSize
                                                     : input.viewport().height;
    // Content-space coordinates: row i's top is `padding.top + i * pitch`.
    const float top = std::max(0.0f, state.offset - options.padding.top);
    const float bottom = top + std::max(0.0f, viewport);

    const auto rowAt = [&](float y) {
        return static_cast<std::size_t>(std::max(0.0f, std::floor(y / slice.pitch)));
    };
    const std::size_t firstVisible = rowAt(top);
    const std::size_t lastVisible = std::min(options.count, rowAt(bottom) + 1);

    slice.first = firstVisible > options.overscan ? firstVisible - options.overscan : 0;
    const std::size_t last = std::min(options.count, lastVisible + options.overscan);
    slice.count = last > slice.first ? last - slice.first : 0;

    // Everything above the slice, as one node.
    if (slice.first > 0) {
        Style above;
        above.height = spacerFor(static_cast<float>(slice.first) * slice.pitch, options.gap);
        above.shrink = 0.0f;
        ui.add(above);
    }

    for (std::size_t index = slice.first; index < last; ++index) {
        // The slot owns the height, so a row that measures differently — a
        // wrapped label, a taller badge — cannot walk the list out of step with
        // its own scrollbar.
        Style slot;
        slot.direction = Direction::Column;
        slot.height = options.rowHeight;
        slot.shrink = 0.0f;
        auto scope = ui.scope(slot);
        row(ui, index);
        (void)scope;
    }

    // …and everything below it.
    if (last < options.count) {
        Style below;
        below.height =
            spacerFor(static_cast<float>(options.count - last) * slice.pitch, options.gap);
        below.shrink = 0.0f;
        ui.add(below);
    }

    (void)content;
    return slice;
}

}  // namespace gbui
