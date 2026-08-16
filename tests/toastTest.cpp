// The queue, the clock and the pause.
//
// All three are testable with no window because the queue is the application's
// and the clock is a `delta` the caller passes — which is the whole reason the
// component holds neither.
#include <string>
#include <vector>

#include "gbui/input/interaction.hpp"
#include "gbui/layout/layout.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/style/theme.hpp"
#include "gbui/widgets/overlays.hpp"
#include "harness.hpp"

using namespace gbui;

namespace {

const Rect kWindow{0, 0, 800, 600};

TextMetrics measureFixed(std::string_view text, const TextStyle&, const Typography&, float) {
    std::size_t characters = 0;
    for (char c : text) {
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++characters;
    }
    return {static_cast<float>(characters) * 8.0f, 14.0f, 11.0f};
}

/** One outlet, driven the way the frame loop drives it. */
struct Outlet {
    Theme theme = Theme::dark();
    Arena arena;
    Interaction input;
    ToastState state;
    ToastOptions options{};
    ToastResult result{};

    /** Resolve, then build — the real order, which matters because the toasts
     *  read the pointer and the keyboard while they are being built. */
    void frame(float delta = 0.0f, const InputFrame& event = {}) {
        input.update(arena, arena.empty() ? NodeId{} : NodeId(0), event);

        arena.reset();
        Ui ui(arena);
        ui.setMeasure(&measureFixed, theme.typography());
        {
            auto root = ui.column({.width = kWindow.width, .height = kWindow.height});
            result = toast(ui, input, state, delta, options);
            (void)root;
        }
        LayoutContext context;
        context.theme = &theme;
        context.measure = &measureFixed;
        layout(arena, ui.root(), kWindow, context);
    }

    /**
     * Three frames, and it takes three.
     *
     * The first has no viewport at all — `Interaction` learns it from the tree
     * it resolved against, and on frame one there is none — so the stack is
     * placed against the fallback. The second is placed correctly, and the
     * third is the first whose `frameOf` reports it. That is the library's
     * standard "estimate, then correct", not a quirk of this component, and a
     * test that measures anything positioned has to let it settle.
     */
    void settle() {
        frame();
        frame();
        frame();
    }

    Rect frameOf(std::string_view tag) const { return input.frameOf(tag); }
};

}  // namespace

TEST("the same message twice is one toast with a count on it") {
    // The failure every application's first queue has: a retry loop turning one
    // outage into forty stacked copies of the same sentence.
    ToastState state;
    for (int i = 0; i < 5; ++i) {
        state.push({.kind = ToastKind::Error, .message = "Could not reach origin."});
    }
    CHECK_EQ(state.items.size(), std::size_t{1});
    CHECK_EQ(state.items.front().count, std::size_t{5});

    // A different message is different news.
    state.push({.kind = ToastKind::Error, .message = "Could not reach upstream."});
    CHECK_EQ(state.items.size(), std::size_t{2});

    // And so is the same words at a different level: an error and a warning
    // saying the same thing are not the same event.
    state.push({.kind = ToastKind::Warning, .message = "Could not reach origin."});
    CHECK_EQ(state.items.size(), std::size_t{3});
}

TEST("an id given by the caller is the one that decides") {
    ToastState state;
    state.push({.id = "upload", .message = "Uploading a.png"});
    state.push({.id = "upload", .message = "Uploading b.png"});
    CHECK_EQ(state.items.size(), std::size_t{1});
    // The first one's text stays: `push` bumps what is there rather than
    // rewriting it, so a reader mid-sentence is not overwritten.
    CHECK(state.items.front().message == "Uploading a.png");
    CHECK_EQ(state.items.front().count, std::size_t{2});
}

TEST("saying it again puts the clock back") {
    Outlet outlet;
    outlet.state.push({.message = "Still offline", .duration = 4.0});
    outlet.frame();
    outlet.frame(3.0f);
    CHECK(outlet.state.items.front().elapsed > 2.0);

    outlet.state.push({.message = "Still offline", .duration = 4.0});
    CHECK_NEAR(outlet.state.items.front().elapsed, 0.0);
    // …and it is still one toast, saying it twice.
    CHECK_EQ(outlet.state.items.size(), std::size_t{1});
}

TEST("a toast goes when its time is up, and a sticky one never does") {
    Outlet outlet;
    outlet.state.push({.id = "timed", .message = "Saved", .duration = 2.0});
    outlet.state.push({.id = "sticky", .message = "Merge conflict", .duration = 0.0});

    outlet.frame();
    outlet.frame(1.0f);
    CHECK_EQ(outlet.state.items.size(), std::size_t{2});

    outlet.frame(1.5f);
    CHECK_EQ(outlet.state.items.size(), std::size_t{1});
    CHECK(outlet.state.items.front().id == "sticky");
    CHECK(outlet.result.dismissed.has_value());

    // Ten seconds later it is still there, which is the point of zero.
    for (int i = 0; i < 10; ++i) outlet.frame(1.0f);
    CHECK_EQ(outlet.state.items.size(), std::size_t{1});
}

/**
 * A message that vanishes while it is being read was not delivered either.
 * This is Toastify's behaviour and also WCAG's "enough time": a reader who
 * needs longer has a way to take it.
 */
TEST("the clock stops while the pointer is resting on a toast") {
    Outlet outlet;
    outlet.state.push({.id = "read-me", .message = "Take your time", .duration = 3.0});
    outlet.settle();

    const Rect card = outlet.frameOf("toast.read-me");
    CHECK(card.width > 0.0f);

    InputFrame over;
    over.pointer = {card.x + card.width / 2.0f, card.y + card.height / 2.0f};
    for (int i = 0; i < 10; ++i) outlet.frame(1.0f, over);

    // Ten seconds on a three-second toast, and it is still on screen.
    CHECK_EQ(outlet.state.items.size(), std::size_t{1});
    CHECK(outlet.state.items.front().elapsed < 0.001);

    // The pointer leaves and it resumes from where it stopped.
    outlet.frame(1.0f);
    CHECK(outlet.state.items.front().elapsed > 0.5);
}

TEST("the pointer somewhere else in the column does not stop it") {
    // A bottom-anchored stack is a full-height column with nothing in most of
    // it. Pausing because the pointer is in that empty strip would stop the
    // clock across half the window, which is why the pause measures the toasts.
    Outlet outlet;
    outlet.options.placement = ToastPlacement::BottomRight;
    outlet.state.push({.id = "t", .message = "Saved", .duration = 3.0});
    outlet.settle();

    const Rect card = outlet.frameOf("toast.t");
    CHECK(card.height > 0.0f);

    InputFrame above;
    above.pointer = {card.x + card.width / 2.0f, card.y - 200.0f};
    outlet.frame(1.0f, above);
    CHECK(outlet.state.items.front().elapsed > 0.5);
}

TEST("a stack grows away from the edge it is anchored to") {
    const auto placeAt = [](ToastPlacement placement) {
        Outlet outlet;
        outlet.options.placement = placement;
        outlet.state.push({.id = "first", .message = "One", .duration = 0.0});
        outlet.state.push({.id = "second", .message = "Two", .duration = 0.0});
        outlet.settle();
        return std::pair{outlet.frameOf("toast.first"), outlet.frameOf("toast.second")};
    };

    // Downwards from the top, newest nearest the edge: the second is above.
    {
        const auto [first, second] = placeAt(ToastPlacement::TopRight);
        CHECK(second.y < first.y);
        CHECK(second.y >= kWindow.y);
        CHECK(first.bottom() <= kWindow.bottom());
    }
    // Upwards from the bottom, newest nearest the edge: the second is below —
    // and the whole stack is inside the window, which is what the full-height
    // column is for.
    {
        const auto [first, second] = placeAt(ToastPlacement::BottomRight);
        CHECK(second.y > first.y);
        CHECK(second.bottom() <= kWindow.bottom());
        CHECK(first.y >= kWindow.y);
    }
}

TEST("the corners are the corners, and bounds moves all six of them") {
    Outlet outlet;
    outlet.state.push({.id = "t", .message = "One", .duration = 0.0});

    outlet.options.placement = ToastPlacement::TopLeft;
    outlet.settle();
    const Rect left = outlet.frameOf("toast.t");
    CHECK_NEAR(left.x, kWindow.x + outlet.options.margin);

    outlet.options.placement = ToastPlacement::TopCenter;
    outlet.settle();
    const Rect centre = outlet.frameOf("toast.t");
    CHECK_NEAR(centre.x + centre.width / 2.0f, kWindow.width / 2.0f);

    // A stack inside a panel rather than over the window — one rectangle, and
    // every placement follows it.
    outlet.options.placement = ToastPlacement::TopRight;
    outlet.options.bounds = Rect{100.0f, 50.0f, 300.0f, 200.0f};
    outlet.settle();
    const Rect inside = outlet.frameOf("toast.t");
    CHECK_NEAR(inside.right(), 400.0f - outlet.options.margin);
    CHECK_NEAR(inside.y, 50.0f + outlet.options.margin);
}

TEST("a group is an outlet, and the entries that are not in it are not shown") {
    Outlet outlet;
    outlet.options.group = "dialog";
    outlet.state.push({.id = "a", .message = "In the dialog", .duration = 0.0, .group = "dialog"});
    outlet.state.push({.id = "b", .message = "In the corner", .duration = 0.0});
    outlet.settle();

    CHECK(!outlet.frameOf("toast.dialog.a").empty());
    CHECK(outlet.frameOf("toast.dialog.b").empty());
    // And the one it did not show is still in the queue, waiting for the outlet
    // that will.
    CHECK_EQ(outlet.state.items.size(), std::size_t{2});
}

TEST("only what is on screen ages") {
    // A toast that has not been shown has not been read, and starting its clock
    // in the queue would let it expire before anybody saw it.
    Outlet outlet;
    outlet.options.maxVisible = 2;
    for (int i = 0; i < 4; ++i) {
        outlet.state.push({.id = "t" + std::to_string(i), .message = "Message", .duration = 2.0});
    }
    outlet.frame();
    outlet.frame(1.0f);

    // The newest two are the ones on screen, and the only ones running.
    CHECK_NEAR(outlet.state.items[0].elapsed, 0.0);
    CHECK_NEAR(outlet.state.items[1].elapsed, 0.0);
    CHECK(outlet.state.items[2].elapsed > 0.5);
    CHECK(outlet.state.items[3].elapsed > 0.5);
}

TEST("the × takes one out, and reports which") {
    Outlet outlet;
    outlet.state.push({.id = "t", .title = "Pushed", .message = "3 commits", .duration = 0.0});
    outlet.settle();

    const Rect close = outlet.frameOf("toast.t.close");
    CHECK(close.width > 0.0f);
    const Vec2 at{close.x + close.width / 2.0f, close.y + close.height / 2.0f};

    InputFrame down;
    down.pointer = at;
    down.pointerDown = true;
    outlet.frame(0.0f, down);
    CHECK_EQ(outlet.state.items.size(), std::size_t{1});   // a press is not a click

    InputFrame up;
    up.pointer = at;
    up.pointerDown = false;
    outlet.frame(0.0f, up);
    CHECK(outlet.state.items.empty());
    CHECK(outlet.result.dismissed.has_value());
}

TEST("an action reports and does not close, because only the caller knows") {
    Outlet outlet;
    outlet.state.push(
        {.id = "t", .message = "Deleted three files", .duration = 0.0, .action = "Undo"});
    outlet.settle();

    const Rect action = outlet.frameOf("toast.t.action");
    CHECK(action.width > 0.0f);
    const Vec2 at{action.x + action.width / 2.0f, action.y + action.height / 2.0f};

    InputFrame down;
    down.pointer = at;
    down.pointerDown = true;
    outlet.frame(0.0f, down);
    InputFrame up;
    up.pointer = at;
    up.pointerDown = false;
    outlet.frame(0.0f, up);

    CHECK(outlet.result.activated.has_value());
    if (outlet.result.activated) CHECK(*outlet.result.activated == "t");
    // Still there: whether "Undo" should close it is the application's answer.
    CHECK_EQ(outlet.state.items.size(), std::size_t{1});
}

TEST("a bar is drawn only where there is time to show") {
    const auto barCount = [](double duration, bool progress) {
        Outlet outlet;
        outlet.options.progress = progress;
        outlet.state.push({.id = "t", .message = "Saved", .duration = duration});
        outlet.settle();
        // The fill is the only node in the toast that is a percentage wide.
        std::size_t bars = 0;
        for (std::size_t i = 0; i < outlet.arena.size(); ++i) {
            const Node& node = outlet.arena[NodeId{static_cast<std::uint32_t>(i)}];
            if (node.style.height.value == 3.0f && node.style.width.relative) ++bars;
        }
        return bars;
    };

    CHECK_EQ(barCount(5.0, true), std::size_t{1});
    // Nothing to count down, so nothing that says there is.
    CHECK_EQ(barCount(0.0, true), std::size_t{0});
    CHECK_EQ(barCount(5.0, false), std::size_t{0});
}
