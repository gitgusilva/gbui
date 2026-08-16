#include "gbui/widgets/scroll.hpp"

#include <algorithm>
#include <string>

namespace gbui {

void scrollbar(Ui& ui, const Interaction& input, std::string_view id, const ScrollState& state,
               Rect box, ScrollAxis axis, float width, bool autoHide) {
    const bool vertical = axis != ScrollAxis::Horizontal;
    if (box.empty() || (!state.scrollable() && autoHide)) return;

    const std::string thumbId = std::string(id) + ".thumb";
    const std::string trackId = std::string(id) + ".track";

    const float length = vertical ? box.height : box.width;
    const float ratio = state.contentSize > 0.0f
                            ? std::clamp(state.viewportSize / state.contentSize, 0.0f, 1.0f)
                            : 1.0f;
    // A thumb shorter than this is a target nobody can hit.
    const float thumbLength = std::max(28.0f, length * ratio);
    const float thumbTravel = std::max(0.0f, length - thumbLength);
    const float thumbAt = thumbTravel * state.progress();

    // A bar the pointer is over is a bar being used: the track darkens and the
    // thumb thickens, both by the same number, so the two read as one object
    // waking up rather than as two things changing.
    const bool overBar = input.isHovered(trackId) || input.isHovered(thumbId) ||
                         input.dragging() == thumbId;
    const float wake =
        ui.animate(id, "bar", overBar ? 1.0f : 0.0f, {.duration = 0.12f, .easing = Easing::EaseOut});
    const bool held = input.dragging() == thumbId;

    Style track;
    track.position = Position::Absolute;
    track.left = box.x + (vertical ? box.width - width : 0.0f);
    track.top = box.y + (vertical ? 0.0f : box.height - width);
    track.width = vertical ? width : box.width;
    track.height = vertical ? box.height : width;
    track.radius = 0.0f;
    track.background = Fill{Token::Bg, 0.4f + 0.35f * wake};
    // The track is a target, not a decoration — clicking it pages, which is
    // what the arrow-less bars every desktop now ships do.
    track.cursorHint = Cursor::Default;
    ui.add(track);
    ui.tag(trackId);

    // The thumb grows out of the track's centre line, so thickening it does not
    // make it drift sideways.
    const float thickness = width - 4.0f + 2.0f * wake;
    const float sideways = (width - thickness) / 2.0f;

    Style thumb;
    thumb.position = Position::Absolute;
    thumb.left = box.x + (vertical ? box.width - width + sideways : thumbAt);
    thumb.top = box.y + (vertical ? thumbAt : box.height - width + sideways);
    thumb.width = vertical ? thickness : thumbLength;
    thumb.height = vertical ? thumbLength : thickness;
    thumb.radius = thickness / 2.0f;
    thumb.background = Fill{held || wake > 0.5f ? Token::TextMuted : Token::BorderStrong};
    thumb.cursorHint = held ? Cursor::Grabbing : Cursor::Grab;
    ui.add(thumb);
    ui.tag(thumbId).cursor(thumb.cursorHint);
}

Ui::Scope scrollArea(Ui& ui, const Interaction& input, std::string_view id, ScrollState& state,
                     const ScrollOptions& options) {
    const ScrollAxis axis = options.resolvedAxis();
    const bool scrolls = axis != ScrollAxis::None;
    const bool vertical = axis == ScrollAxis::Vertical;
    const std::string contentId = std::string(id) + ".content";
    const std::string thumbId = std::string(id) + ".thumb";
    const std::string trackId = std::string(id) + ".track";

    // Last frame's geometry: the viewport it was given, and how far its content
    // reached. Both are unknown on the first frame, which simply means nothing
    // scrolls until there is something to scroll.
    const Rect viewportFrame = input.frameOf(id);
    const Rect contentFrame = input.frameOf(contentId);
    state.viewportSize = vertical ? viewportFrame.height : viewportFrame.width;
    state.contentSize = vertical ? contentFrame.height : contentFrame.width;

    // The wheel goes to the innermost scrollable under the pointer, and only to
    // it. Reacting to "the pointer is somewhere inside me" is what made a page
    // and the list on it scroll together, at twice the speed and in lockstep.
    //
    // Shift picks the sideways one instead. That is the gesture every desktop
    // uses for a horizontal scroll, and without it a box whose only overflow is
    // sideways can be reached by nothing but its own bar.
    const bool shiftHeld = input.modifiers().shift;
    const std::string_view target = shiftHeld ? input.wheelTargetX() : input.wheelTarget();
    if (scrolls && !target.empty() && target == id && input.wheel() != 0.0f) {
        state.offset -= input.wheel() * options.step;
    }
    // Focus inside the view counts as focus on it: clicking a row in a list
    // should not be what stops Page Down from working.
    if (scrolls && input.isFocusedWithin(id)) {
        for (const KeyEvent& event : input.keys()) {
            if (event.key == Key::PageDown) state.offset += state.viewportSize * 0.9f;
            if (event.key == Key::PageUp) state.offset -= state.viewportSize * 0.9f;
            if (event.key == Key::Home) state.offset = 0.0f;
            if (event.key == Key::End) state.offset = state.maxOffset();
        }
    }
    // Clicking the track pages towards the click, which is what a bar with no
    // arrows is for. The thumb is a separate target and is hit first, so this
    // only ever fires on the empty part.
    if (scrolls && input.clicked(trackId) && state.scrollable() && state.viewportSize > 0.0f) {
        const Rect trackFrame = input.frameOf(trackId);
        const float along = vertical ? input.pointer().y - trackFrame.y
                                     : input.pointer().x - trackFrame.x;
        const float length = vertical ? trackFrame.height : trackFrame.width;
        const float page = state.viewportSize * 0.9f;
        // Which side of the thumb was clicked, in the track's own coordinates.
        state.offset += along < length * state.progress() ? -page : page;
    }
    // Dragging the bar moves the content by the inverse ratio: a thumb half the
    // track's length travels half as far as the content it represents.
    if (scrolls && input.dragging() == thumbId && state.scrollable() && state.viewportSize > 0.0f) {
        const float travel = vertical ? input.pointerDelta().y : input.pointerDelta().x;
        state.offset += travel * (state.contentSize / state.viewportSize);
    }
    // Only once there is something to clamp against. On the first frame both
    // sizes are zero, so `maxOffset` is zero too, and clamping would throw away
    // an offset the application had restored — a list reopened where the reader
    // left it would jump to the top for no reason anybody could see.
    if (!scrolls) {
        // Nothing moves, so nothing is offset. The clip still applies, which is
        // the difference between this and no container at all.
        state.offset = 0.0f;
    } else if (state.viewportSize > 0.0f && state.contentSize > 0.0f) {
        state.offset = std::clamp(state.offset, 0.0f, state.maxOffset());
    } else {
        state.offset = std::max(0.0f, state.offset);
    }

    // ---- the viewport -----------------------------------------------------
    Style viewport;
    viewport.direction = options.direction;
    viewport.grow = options.grow;
    // A growing viewport starts from nothing and takes what is left. One that
    // was given a size keeps it: a basis of zero would win over the height and
    // clip the whole pane away, which is the same rule `text` follows.
    if (options.grow > 0.0f) viewport.basis = 0.0f;
    viewport.width = options.width;
    viewport.height = options.height;
    viewport.minWidth = options.minWidth;
    viewport.maxWidth = options.maxWidth;
    viewport.minHeight = options.minHeight;
    viewport.maxHeight = options.maxHeight;
    // Which wheel this one answers to. It clips either way; the value is how
    // the pointer tells a box that scrolls up and down from one that scrolls
    // side to side, which matters when they are nested.
    viewport.overflow = axis == ScrollAxis::None ? Overflow::Hidden
                        : vertical               ? Overflow::Scroll
                                                 : Overflow::ScrollX;

    // "As tall as its content, then scroll."
    //
    // A viewport cannot size itself to its content the usual way: the content
    // is out of the flow, so it contributes nothing to the intrinsic pass and
    // an auto height collapses to zero. That is deliberate — an in-flow content
    // box would feed its height back into the container's and oscillate — so
    // the size comes from what the content measured *last* frame, which is a
    // fixed number this one. It settles in a frame and cannot ring.
    //
    // Only the scrolling axis: the other one is pinned or stretched below, and
    // taking that from the content too would be the circular version again.
    if (options.grow <= 0.0f) {
        const auto bounded = [](float value, float low, float high) {
            if (!isAuto(low)) value = std::max(value, low);
            if (!isAuto(high)) value = std::min(value, high);
            return value;
        };
        if (isAuto(options.height) && axis != ScrollAxis::Horizontal && contentFrame.height > 0.0f) {
            viewport.height = bounded(contentFrame.height, options.minHeight, options.maxHeight);
        }
        if (isAuto(options.width) && axis != ScrollAxis::Vertical && contentFrame.width > 0.0f) {
            viewport.width = bounded(contentFrame.width, options.minWidth, options.maxWidth);
        }
    }
    auto scope = ui.scope(viewport);
    ui.tag(id).focusable(options.focusable);
    // Only when it is a keyboard stop of its own. A viewport nothing can land
    // on is a clip, and a tree full of anonymous scroll regions is a tree a
    // reader has to walk through rather than one they can navigate — the same
    // rule `box` follows for `Group`.
    if (options.focusable) {
        ui.accessible({.role = Role::ScrollView, .name = options.name});
    }

    // ---- the bar ----------------------------------------------------------
    // Drawn before the content so the content's own scope can be returned to
    // the caller; the bar is out of the flow, so order does not place it.
    //
    // Everything below is in the viewport's own coordinates — `Position` is
    // measured from the parent's content box — so only the viewport's *size*
    // still comes from the last layout, and a resize costs a frame of width
    // rather than a frame of the whole pane sitting in the wrong place.
    if (scrolls && options.scrollbar && viewportFrame.width > 0.0f) {
        scrollbar(ui, input, id, state, Rect{0.0f, 0.0f, viewportFrame.width, viewportFrame.height},
                  axis, options.scrollbarWidth, options.autoHideScrollbar);
    }

    // ---- the content ------------------------------------------------------
    // Out of the flow and shifted by the offset: the content keeps its natural
    // size and the viewport's clip does the rest. Laying it out inside the flow
    // instead would let the container's height feed back into the content's,
    // which is how a scroll view starts oscillating.
    //
    // The offset is all there is to say, because the origin is the viewport
    // itself: a pane that has just moved — a list inside a popover that was
    // placed this frame — brings its content with it instead of arriving a
    // frame later.
    Style content;
    content.position = Position::Absolute;
    content.direction = options.direction;
    content.gap = options.gap;
    content.padding = options.padding;
    content.left = axis == ScrollAxis::Horizontal ? -state.offset : 0.0f;
    content.top = axis == ScrollAxis::Vertical ? -state.offset : 0.0f;
    // The axis that does not scroll is pinned to the viewport, so content that
    // would overflow it wraps or shrinks rather than running off the side with
    // no way to reach it. A view that scrolls neither way pins nothing: its
    // content keeps its natural size and the clip does the rest, which is the
    // whole point of asking for `None`.
    if (axis == ScrollAxis::Vertical && viewportFrame.width > 0.0f) {
        content.width =
            viewportFrame.width -
            (options.scrollbar && state.scrollable() ? options.scrollbarWidth : 0.0f);
    }
    if (axis == ScrollAxis::Horizontal && viewportFrame.height > 0.0f) {
        // Minus the bar, exactly as the vertical case is minus its own. Without
        // it the content covers the strip the bar is drawn in and takes every
        // press meant for it, so a horizontal bar could be seen and not used —
        // the asymmetry was the whole bug.
        content.height =
            viewportFrame.height -
            (options.scrollbar && state.scrollable() ? options.scrollbarWidth : 0.0f);
    }

    auto contentScope = ui.scope(content);
    ui.tag(contentId);
    // The content scope closes both boxes when the caller is done with it, and
    // the viewport scope gives up its own pop — otherwise it would fire the
    // moment this function returns and everything the caller adds would land
    // outside the scroll view.
    contentScope.adopt();
    scope.disown();
    return contentScope;
}

void revealRow(ScrollState& state, const RowMetrics& rows, std::size_t index) {
    const float top = rows.top + static_cast<float>(index) * rows.pitch();
    const float bottom = top + rows.height;

    // Scrolling the least distance rather than centring: a row one line below
    // the fold should move one line, not jump the list under the reader.
    //
    // The bottom edge is only known once the view has been laid out; before
    // that only "scroll up to it" can be answered, and answering the other half
    // with a viewport of zero would put every row at the top of the list.
    if (top < state.offset) {
        // Revealing the first row reveals the padding above it too. Stopping at
        // the padding instead leaves the list looking scrolled — the row sits
        // flush against the top edge and the bar never reaches the start — for
        // the sake of the few pixels that are supposed to be there.
        state.offset = top <= rows.top ? 0.0f : top;
    } else if (state.viewportSize > 0.0f && bottom > state.offset + state.viewportSize) {
        state.offset = bottom - state.viewportSize;
    }
    state.offset = state.contentSize > 0.0f
                       ? std::clamp(state.offset, 0.0f, state.maxOffset())
                       : std::max(0.0f, state.offset);
}

}  // namespace gbui
