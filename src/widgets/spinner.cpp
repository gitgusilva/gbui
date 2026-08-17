#include "gbui/widgets/spinner.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "gbui/core/path.hpp"

namespace gbui {

namespace {

/** A ring segment with flat ends, as one closed contour: out along the arc,
 *  across, back along the inner arc, across again. Stroking a single arc would
 *  be simpler and gives round joins the painter has no way to square off. */
Path ringSegment(Vec2 centre, float outer, float inner, float from, float to) {
    Path path;
    path.arcTo(centre, outer, from, to);
    path.arcTo(centre, inner, to, from);
    path.close();
    return path;
}

}  // namespace

NodeId spinner(Ui& ui, const SpinnerOptions& options) {
    const float size = std::max(6.0f, options.size);
    const float thickness =
        std::clamp(options.thickness > 0.0f ? options.thickness : size / 6.0f, 1.0f, size / 2.0f);
    const float outer = size / 2.0f;
    const float inner = outer - thickness;
    const Vec2 centre{outer, outer};

    // The caller's clock, and only its fractional part: a phase of 3.25 and a
    // phase of 0.25 are the same picture.
    const float from = (options.phase - std::floor(options.phase)) * 360.0f;

    Style box;
    box.width = size;
    box.height = size;
    box.minWidth = 0.0f;
    box.minHeight = 0.0f;
    box.shrink = 0.0f;
    box.radius = 0.0f;

    std::vector<Shape> shapes;
    shapes.reserve(2);
    // The whole ring first, faint, so the arc reads as a piece of something
    // rather than as a comet with nothing behind it.
    shapes.push_back(Shape{ringSegment(centre, outer, inner, 0.0f, 360.0f), Fill{options.track}});
    // Three quarters of a turn is the arc every platform draws. A shorter one
    // reads as a dot going round and a longer one as a ring that is merely
    // rotating, which is harder to see.
    shapes.push_back(
        Shape{ringSegment(centre, outer, inner, from, from + 270.0f), Fill{options.color}});

    const NodeId id = ui.draw(box, std::move(shapes));
    // A `ProgressBar` with no value is ARIA's indeterminate progress, which is
    // exactly what this is. Without a name it is hidden instead: a turning
    // circle that announces itself and says nothing is worse than one that
    // says nothing at all, because the reader now has to find out what it was.
    if (options.name.empty()) {
        ui.accessible(id, {.hidden = true});
    } else {
        ui.accessible(id, {.role = Role::ProgressBar,
                           .name = options.name,
                           .state = {.busy = Flag::True}});
    }
    return id;
}

}  // namespace gbui
