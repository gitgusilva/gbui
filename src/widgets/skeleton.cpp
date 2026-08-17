#include "gbui/widgets/skeleton.hpp"

#include <cmath>

namespace gbui {

NodeId skeleton(Ui& ui, const SkeletonOptions& options) {
    Style box;
    box.grow = options.grow;
    box.shrink = 1.0f;
    box.minWidth = 0.0f;
    box.minHeight = 0.0f;

    switch (options.shape) {
        case SkeletonShape::Text:
            box.width = options.width;
            box.height = options.height > 0.0f ? options.height : 12.0f;
            box.radius = options.radius > 0.0f ? options.radius : 4.0f;
            break;
        case SkeletonShape::Block:
            box.width = options.width;
            box.height = options.height > 0.0f ? options.height : 64.0f;
            box.radius = options.radius > 0.0f ? options.radius : 8.0f;
            break;
        case SkeletonShape::Circle: {
            const float side = isAuto(options.width) ? 28.0f : options.width;
            box.width = side;
            box.height = side;
            box.radius = side / 2.0f;
            box.grow = 0.0f;
            box.shrink = 0.0f;
            break;
        }
    }

    // A sine rather than a sweep: a gradient travelling across the box needs a
    // gradient per frame and a stop position nobody can see the point of, and
    // the whole effect is "this rectangle is not real". Breathing says that.
    const float phase = options.phase - std::floor(options.phase);
    const float pulse = 0.5f - 0.5f * std::cos(phase * 6.28318531f);
    box.background = Fill{Token::BgOverlay, 0.45f + 0.35f * pulse};

    const NodeId id = ui.add(box);
    // Hidden unless it is the one carrying the message. Six placeholders that
    // each announce themselves are six announcements of nothing; one `Status`
    // saying "Loading commits" is the whole of what a reader needs.
    if (options.name.empty()) {
        ui.accessible(id, {.hidden = true});
    } else {
        ui.accessible(id, {.role = Role::Status,
                           .name = options.name,
                           .state = {.busy = Flag::True}});
    }
    return id;
}

}  // namespace gbui
