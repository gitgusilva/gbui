#include "gbui/widgets/floating.hpp"

namespace gbui {

bool pressedOutside(const Interaction& input, std::initializer_list<std::string_view> tags) {
    if (!input.pointerPressed()) return false;
    const Vec2 at = input.pointer();
    for (const std::string_view tag : tags) {
        if (tag.empty()) continue;
        // Last frame's rectangle, which is the one the reader was looking at
        // when they pressed — the same frame every other hit test here uses.
        if (input.frameOf(tag).contains(at)) return false;
    }
    return true;
}

}  // namespace gbui
