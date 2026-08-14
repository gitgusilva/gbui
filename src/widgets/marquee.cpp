#include "gbui/widgets/marquee.hpp"

#include <cmath>

namespace gbui {

bool marquee(Ui& ui, const Interaction& input, std::string_view id, MarqueeState& state,
             float delta, const std::function<void(Ui&)>& content,
             const MarqueeOptions& options) {
    if (!content) return false;

    const std::string passId = std::string(id) + ".pass";
    // How wide one pass came out last frame. There is no other way to know: the
    // content is the caller's and is measured by laying it out, which has not
    // happened yet this frame. It settles on the second frame and is stable
    // after that, which is the same bargain every geometry-dependent component
    // here makes.
    const float pass = input.frameOf(passId).width;
    const float stride = pass + options.gap;

    Style strip;
    strip.direction = Direction::Row;
    strip.height = options.height;
    strip.padding = options.padding;
    strip.grow = options.grow;
    if (options.grow > 0.0f) strip.basis = 0.0f;
    // And the same argument in the other direction: with nothing in the flow
    // there is no content to take a height from either, so an unsized strip in
    // a row that centres its children collapsed to nothing and drew an empty
    // band. Told to stretch, it is as tall as the row.
    if (isAuto(options.height)) strip.alignSelf = Align::Stretch;
    strip.shrink = 1.0f;
    strip.minWidth = 0.0f;
    // The strip is a window. Without this the two passes would be drawn over
    // whatever is beside it, and the one arriving from the right would be
    // visible long before it was due.
    strip.overflow = Overflow::Hidden;
    auto scope = ui.scope(strip);
    ui.tag(id);

    // Advanced, not derived. Wrapped at the stride so the number stays small
    // however long the application has been open — a float counting pixels
    // since launch loses a pixel of precision an hour in, and the strip starts
    // to stutter.
    //
    // The wrap is also where a change in the content's width lands, and it
    // lands harmlessly: the strip carries on from where it was and the next
    // turn comes a few pixels earlier or later. Computing the position from a
    // clock instead put that change *under* the strip, which threw it sideways
    // every time a number gained a digit.
    const float before = state.offset;
    state.offset += delta * options.speed;
    bool wrapped = false;
    if (stride > 1.0f) {
        state.offset = std::fmod(state.offset, stride);
        if (state.offset < 0.0f) state.offset += stride;
        // Came round: the offset went backwards without the strip doing so.
        wrapped = delta != 0.0f && ((options.speed >= 0.0f && state.offset < before) ||
                                    (options.speed < 0.0f && state.offset > before));
    }
    const float travelled = state.offset;

    for (int copy = 0; copy < 2; ++copy) {
        Style lane;
        lane.direction = Direction::Row;
        lane.align = Align::Center;
        lane.position = Position::Absolute;
        lane.left = -travelled + static_cast<float>(copy) * stride;
        lane.top = 0.0f;
        lane.height = Length::percent(100);
        auto laneScope = ui.scope(lane);
        // Only the first is measured. The second is the same content and would
        // report the same width, and two nodes answering to one tag is a
        // question with two answers.
        if (copy == 0) ui.tag(passId).ignoresPointer();
        content(ui);
        (void)laneScope;
    }
    (void)scope;
    return wrapped;
}

}  // namespace gbui
