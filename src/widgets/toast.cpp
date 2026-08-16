#include "gbui/widgets/toast.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "detail.hpp"
#include "gbui/widgets/badge.hpp"
#include "gbui/widgets/button.hpp"
#include "gbui/widgets/icon.hpp"
#include "gbui/widgets/text.hpp"

namespace gbui {

// What this shares with its siblings, rather than a copy in each.
using namespace detail;

namespace {

/** The token a kind is drawn in. The same four an application already reads as
 *  neutral, good, careful and wrong. */
Token toneOf(ToastKind kind) {
    switch (kind) {
        case ToastKind::Success: return Token::Added;
        case ToastKind::Warning: return Token::Modified;
        case ToastKind::Error: return Token::Removed;
        case ToastKind::Info: break;
    }
    return Token::Accent;
}

Icon glyphOf(ToastKind kind) {
    switch (kind) {
        case ToastKind::Success: return Icon::Check;
        case ToastKind::Warning:
        case ToastKind::Error: return Icon::CircleAlert;
        case ToastKind::Info: break;
    }
    return Icon::CircleAlert;
}

/**
 * Whether a reader is interrupted, or told at the next pause.
 *
 * Not a style choice. `Error` and `Warning` mean the next thing the reader was
 * about to do will not work, and a message that waits for a gap in their
 * reading arrives after they have already done it.
 */
Role regionOf(ToastKind kind) {
    return kind == ToastKind::Error || kind == ToastKind::Warning ? Role::Alert : Role::Status;
}

bool atTop(ToastPlacement placement) {
    return placement == ToastPlacement::TopLeft || placement == ToastPlacement::TopCenter ||
           placement == ToastPlacement::TopRight;
}

/** The identity two identical messages share, when the caller supplied none. */
std::string derivedId(const ToastEntry& entry) {
    return std::to_string(static_cast<int>(entry.kind)) + "|" + entry.title + "|" + entry.message;
}

}  // namespace

std::string ToastState::push(ToastEntry entry) {
    if (entry.id.empty()) entry.id = derivedId(entry);

    for (ToastEntry& existing : items) {
        if (existing.id != entry.id) continue;
        // Saying the same thing again is not new news, it is the same news
        // still being true — so the count goes up and the clock goes back,
        // which keeps it on screen for as long as it keeps happening.
        ++existing.count;
        existing.elapsed = 0.0;
        return existing.id;
    }

    entry.elapsed = 0.0;
    entry.count = 1;
    items.push_back(std::move(entry));
    return items.back().id;
}

void ToastState::dismiss(std::string_view id) {
    items.erase(std::remove_if(items.begin(), items.end(),
                               [&](const ToastEntry& entry) { return entry.id == id; }),
                items.end());
}

ToastResult toast(Ui& ui, const Interaction& input, ToastState& state, float delta,
                  const ToastOptions& options) {
    ToastResult result;

    // One outlet per group, and its tag has to name the group: two stacks in
    // one frame both called "toast" would share a rectangle, a hover test and
    // a focus check.
    const std::string outletId =
        options.group.empty() ? std::string("toast") : "toast." + std::string(options.group);

    std::vector<std::size_t> mine;
    for (std::size_t i = 0; i < state.items.size(); ++i) {
        if (state.items[i].group == options.group) mine.push_back(i);
    }
    if (mine.empty()) return result;

    // The newest `maxVisible`. The rest are still in the queue and still the
    // caller's; they simply have not been shown yet, and — because only what is
    // shown ages — their clocks have not started either.
    const std::size_t shown = std::min(mine.size(), std::max<std::size_t>(1, options.maxVisible));
    std::vector<std::size_t> visible(mine.end() - static_cast<std::ptrdiff_t>(shown), mine.end());

    // ---- the pause ---------------------------------------------------------
    //
    // Tested against each toast's own rectangle rather than by hit testing, and
    // deliberately: a toast is a place a reader *rests* the pointer to read it,
    // and that gesture pauses the whole stack rather than the one node under
    // the tip. Not the container's rectangle either — a bottom-anchored stack
    // is a full-height column with nothing in most of it, and pausing because
    // the pointer is somewhere in that empty strip would stop the clock across
    // half the window.
    bool paused = input.isFocusedWithin(outletId);
    for (const std::size_t index : visible) {
        const std::string tag = outletId + "." + state.items[index].id;
        if (input.frameOf(tag).contains(input.pointer())) paused = true;
    }

    // ---- the clock ---------------------------------------------------------
    std::vector<std::string> expired;
    for (const std::size_t index : visible) {
        ToastEntry& entry = state.items[index];
        if (entry.duration <= 0.0) continue;
        if (!paused) entry.elapsed += static_cast<double>(delta);
        if (entry.elapsed >= entry.duration) expired.push_back(entry.id);
    }

    // ---- where the stack goes ----------------------------------------------
    const Rect bounds = options.bounds.empty()
                            ? (input.viewport().empty() ? Rect{0, 0, 1280, 720} : input.viewport())
                            : options.bounds;

    Vec2 at;
    bool growsDown = true;
    // ---- how a bottom stack finds its own bottom ---------------------------
    //
    // Not by measuring itself. Its height is its content's and that is not
    // known until this frame has been laid out, so placing it at
    // `bottom - height` would use last frame's and be wrong on the first and
    // after every change. The container is the **whole column** instead, from
    // one margin to the other, and `justify` puts the toasts at the end of it.
    // Flexbox already solves this; the arithmetic was the mistake.
    //
    // It costs an invisible full-height node, which is why the tag ignores the
    // pointer and why the pause above measures the toasts rather than this.
    const bool fullColumn = options.placement != ToastPlacement::Anchored;

    if (options.placement == ToastPlacement::Anchored) {
        // The one case that cannot: an anchored stack is placed *against
        // something*, so it has to be given a size to be placed at. Last
        // frame's, and the same one-frame estimate every floating box here
        // makes — see `popover`.
        const Rect anchor = input.frameOf(options.anchorId);
        const float known = input.frameOf(outletId).height;
        const PlacementResult placed =
            place(anchor, {options.width, known > 0.0f ? known : 48.0f}, bounds,
                  {.preferred = Placement::Bottom, .gap = 8.0f, .margin = options.margin});
        at = {placed.rect.x, placed.rect.y};
        growsDown = placed.placement != Placement::Top;
    } else {
        growsDown = atTop(options.placement);
        switch (options.placement) {
            case ToastPlacement::TopLeft:
            case ToastPlacement::BottomLeft:
                at.x = bounds.x + options.margin;
                break;
            case ToastPlacement::TopCenter:
            case ToastPlacement::BottomCenter:
                at.x = bounds.x + (bounds.width - options.width) / 2.0f;
                break;
            default:
                at.x = bounds.right() - options.width - options.margin;
                break;
        }
        at.y = bounds.y + options.margin;
    }

    // ---- the stack ---------------------------------------------------------
    Style stack;
    stack.position = Position::Fixed;
    stack.layer = Layer::Overlay;
    stack.left = at.x;
    stack.top = at.y;
    stack.width = options.width;
    stack.direction = Direction::Column;
    stack.gap = options.gap;
    if (fullColumn) {
        stack.height = std::max(0.0f, bounds.height - 2.0f * options.margin);
        stack.justify = growsDown ? Justify::Start : Justify::End;
    }

    auto stackScope = ui.scope(stack);
    // A `Group`, not a live region: the live regions are the toasts themselves,
    // which is where ARIA puts them and what lets one message interrupt while
    // the one beside it waits its turn.
    ui.tag(outletId).ignoresPointer().accessible({.role = Role::Group, .name = options.name});

    // Newest nearest the edge it is anchored to, which means the order is
    // reversed for a stack that grows downwards: the newest is at the top.
    std::vector<std::size_t> order = visible;
    if (growsDown) std::reverse(order.begin(), order.end());

    for (std::size_t slot = 0; slot < order.size(); ++slot) {
        const ToastEntry& entry = state.items[order[slot]];
        const std::string toastId = outletId + "." + entry.id;
        const std::string closeId = toastId + ".close";
        const std::string actionId = toastId + ".action";
        const Token tone = toneOf(entry.kind);

        // A column of two: the message, and the bar under it.
        //
        // The bar is *in the flow* rather than positioned over the card, and
        // that is the second attempt. Absolute is measured from the parent's
        // content box, which is inset by the padding — so pinning it to the
        // foot needs the card's height, which is not known while the card is
        // being built, and the first attempt drew it across the title.
        Style card;
        card.direction = Direction::Column;
        card.background = Fill{Token::BgElevated};
        card.border = Border{1.0f, Fill{Token::BorderStrong}};
        card.radius = ui.design().controlRadius;
        card.overflow = Overflow::Hidden;
        auto cardScope = ui.scope(card);
        // One per toast, and the role is what decides whether the reader is
        // interrupted. `positionInSet` because a stack of four is four things a
        // reader walks, and "3 of 4" is the only thing telling them where they
        // are in it.
        ui.tag(toastId).accessible({
            .role = regionOf(entry.kind),
            .name = entry.title.empty() ? entry.message : entry.title,
            .description = entry.title.empty() ? std::string_view{}
                                               : std::string_view(entry.message),
            .positionInSet = slot + 1,
            .setSize = order.size(),
        });

        Style body;
        body.direction = Direction::Row;
        body.gap = 10.0f;
        body.padding = Edges{10.0f, 10.0f, 10.0f, 0.0f};
        auto bodyRow = ui.scope(body);

        // The stripe down the leading edge, which is the whole of how a kind
        // reads at a glance — and it is *not* the whole of how it reads at all,
        // because the role above says the same thing in words.
        Style stripe;
        stripe.width = 4.0f;
        stripe.shrink = 0.0f;
        stripe.alignSelf = Align::Stretch;
        stripe.radius = 0.0f;
        stripe.background = Fill{tone};
        ui.add(stripe);

        icon(ui, entry.icon.value_or(glyphOf(entry.kind)), {.color = tone, .size = 16.0f});

        {
            Style column;
            column.direction = Direction::Column;
            column.gap = 2.0f;
            column.grow = 1.0f;
            column.basis = 0.0f;
            column.minWidth = 0.0f;
            auto columnScope = ui.scope(column);

            if (!entry.title.empty()) {
                Style line;
                line.direction = Direction::Row;
                line.align = Align::Center;
                line.gap = 6.0f;
                auto lineScope = ui.scope(line);
                text(ui, entry.title,
                     {.color = Token::TextStrong, .weight = FontWeight::SemiBold, .grow = 1.0f});
                // The count, and only when there is one to show. A badge
                // reading "1" is a badge that says nothing.
                if (entry.count > 1) {
                    badge(ui, std::to_string(entry.count) + "×",
                          {.background = tone, .foreground = Token::AccentFg});
                }
                (void)lineScope;
            }
            if (!entry.message.empty()) {
                text(ui, entry.message,
                     {.color = Token::Text, .size = 12.0f, .overflow = TextOverflow::Wrap});
            }
            if (!entry.action.empty()) {
                // In a row of its own, so it sits against the leading edge. The
                // body is a column and a column stretches its children, which
                // made a ghost button as wide as the message and centred its
                // label under it — an action that reads as a caption.
                Style line;
                line.direction = Direction::Row;
                auto lineScope = ui.scope(line);
                button(ui, input, entry.action,
                       {.variant = ButtonVariant::Ghost, .height = 24.0f, .id = actionId});
                (void)lineScope;
                if (input.clicked(actionId)) result.activated = entry.id;
            }
            (void)columnScope;
        }

        if (entry.closable) {
            Style close;
            close.width = 22.0f;
            close.height = 22.0f;
            close.shrink = 0.0f;
            close.justify = Justify::Center;
            close.align = Align::Center;
            close.radius = 4.0f;
            if (input.isHovered(closeId)) close.background = Fill{Token::SurfaceHover};
            if (input.isFocusVisible(closeId)) {
                close.outline = Outline{2.0f, 1.0f, Fill{Token::Accent}};
            }
            close.cursorHint = Cursor::Pointer;
            auto closeScope = ui.scope(close);
            ui.tag(closeId).focusable().cursor(Cursor::Pointer);
            // An × with the message in its name, because a stack of four
            // dismiss buttons all called "Dismiss" is four buttons a reader
            // cannot tell apart.
            ui.accessible({
                .role = Role::Button,
                .name = "Dismiss: " + (entry.title.empty() ? entry.message : entry.title),
            });
            icon(ui, Icon::X, {.color = Token::TextMuted, .size = 13.0f});
            (void)closeScope;
            if (activated(input, closeId, false)) expired.push_back(entry.id);
        }
        bodyRow.close();

        // ---- the bar -------------------------------------------------------
        //
        // Drawn only where there is time to show: a toast with no duration has
        // nothing to count down, and a full bar under it would say the opposite
        // of what is true.
        //
        // It carries **no accessibility record at all**, which is the decision
        // worth writing down: it is the timer, the timer is already paused
        // whenever a reader is anywhere near it, and a progress bar draining
        // under every message is a second announcement of something nobody
        // asked about. It dims while paused, which is the only way the pause is
        // visible at all.
        if (options.progress && entry.duration > 0.0) {
            const auto left = static_cast<float>(
                std::clamp(1.0 - entry.elapsed / entry.duration, 0.0, 1.0));
            Style track;
            track.direction = Direction::Row;
            track.height = 3.0f;
            track.radius = 0.0f;
            track.shrink = 0.0f;
            auto trackScope = ui.scope(track);
            Style fill;
            fill.width = Length::percent(left * 100.0f);
            fill.height = 3.0f;
            fill.radius = 0.0f;
            fill.background = Fill{tone, paused ? 0.4f : 1.0f};
            ui.add(fill);
            (void)trackScope;
        }
        (void)cardScope;
    }

    // Escape, but only while the keyboard is inside the stack — a global
    // Escape belongs to whatever the reader is actually working in, and a
    // notification that swallowed it would close the dialog's cancel key.
    if (input.isFocusedWithin(outletId)) {
        for (const KeyEvent& event : input.keys()) {
            if (event.key != Key::Escape) continue;
            if (!order.empty()) expired.push_back(state.items[order.front()].id);
        }
    }
    (void)stackScope;

    for (const std::string& id : expired) {
        state.dismiss(id);
        result.dismissed = id;
    }
    return result;
}

}  // namespace gbui
