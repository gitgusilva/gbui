// The seam, the handle and the keys.
//
// The one thing worth testing hardest is the claim the header makes: that the
// seam needs no measured geometry, and is therefore right on the very first
// frame. Everything else here is a slider.
#include <string>

#include "gbui/input/interaction.hpp"
#include "gbui/layout/layout.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/style/theme.hpp"
#include "gbui/widgets/containers.hpp"
#include "gbui/widgets/elements.hpp"
#include "harness.hpp"

using namespace gbui;

namespace {

const Rect kWindow{0, 0, 400, 300};

TextMetrics measureFixed(std::string_view text, const TextStyle&, const Typography&, float) {
    std::size_t characters = 0;
    for (char c : text) {
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++characters;
    }
    return {static_cast<float>(characters) * 8.0f, 14.0f, 11.0f};
}

struct Pair {
    Theme theme = Theme::dark();
    Arena arena;
    Interaction input;
    float position = 0.5f;
    CompareOptions options{};
    CompareResult result{};
    /** Tagged inside each side, so a test can find the two layers. */
    Rect beforeRect{};
    Rect afterRect{};

    void frame(const InputFrame& event = {}) {
        input.update(arena, arena.empty() ? NodeId{} : NodeId(0), event);

        arena.reset();
        Ui ui(arena);
        ui.setMeasure(&measureFixed, theme.typography());
        {
            auto root = ui.column({.width = kWindow.width, .height = kWindow.height});
            result = compare(
                ui, input, "cmp", position,
                [](Ui& inner) {
                    Style fill;
                    fill.width = Length::percent(100);
                    fill.height = Length::percent(100);
                    auto scope = inner.scope(fill);
                    inner.tag("cmp.before.fill");
                    (void)scope;
                },
                [](Ui& inner) {
                    Style fill;
                    fill.width = Length::percent(100);
                    fill.height = Length::percent(100);
                    auto scope = inner.scope(fill);
                    inner.tag("cmp.after.fill");
                    (void)scope;
                },
                options);
            (void)root;
        }
        LayoutContext context;
        context.theme = &theme;
        context.measure = &measureFixed;
        layout(arena, ui.root(), kWindow, context);

        // Read straight out of the arena rather than from `frameOf`, which is
        // one frame behind — the point of several of these cases is what the
        // *first* frame looks like.
        beforeRect = afterRect = Rect{};
        for (std::size_t i = 0; i < arena.size(); ++i) {
            const Node& node = arena[NodeId{static_cast<std::uint32_t>(i)}];
            if (node.id == "cmp.before.fill") beforeRect = node.frame;
            if (node.id == "cmp.after.fill") afterRect = node.frame;
        }
        if (result.changed) position = result.position;
    }
};

}  // namespace

/**
 * The claim the header makes, and the reason the clip is a percentage: a seam
 * placed from a measured width would be a frame late, and would jump on every
 * resize. This is the *first* frame.
 */
TEST("the seam is in the right place on the very first frame") {
    Pair pair;
    pair.options.width = 200.0f;
    pair.options.height = 100.0f;
    pair.position = 0.25f;
    pair.frame();

    // Both sides are laid out at the full width of the box; only the clip
    // around the revealed one is a quarter of it.
    CHECK_NEAR(pair.beforeRect.width, 200.0);
    CHECK_NEAR(pair.afterRect.width, 200.0);
    // The revealed side starts at the leading edge, and the clip that carries
    // it is a quarter wide.
    CHECK_NEAR(pair.afterRect.x, pair.beforeRect.x);

    const Node* clip = nullptr;
    for (std::size_t i = 0; i < pair.arena.size(); ++i) {
        const Node& node = pair.arena[NodeId{static_cast<std::uint32_t>(i)}];
        if (node.style.overflow == Overflow::Hidden && node.style.width.relative) clip = &node;
    }
    CHECK(clip != nullptr);
    if (clip) CHECK_NEAR(clip->frame.width, 50.0);
}

TEST("the whole box is drawn at every position, and the clip is what moves") {
    Pair pair;
    pair.options.width = 200.0f;
    pair.options.height = 100.0f;

    for (const float at : {0.1f, 0.5f, 0.9f, 1.0f}) {
        pair.position = at;
        pair.frame();
        CHECK_NEAR(pair.afterRect.width, 200.0);
        CHECK_NEAR(pair.beforeRect.width, 200.0);
    }

    // Nothing of it showing is nothing of it built: the guard that keeps
    // `100 / position` from dividing by zero is the same one that tells the
    // truth about what is on screen.
    pair.position = 0.0f;
    pair.frame();
    CHECK(pair.afterRect.empty());
    CHECK(!pair.beforeRect.empty());
}

TEST("dragging puts the seam under the pointer") {
    Pair pair;
    pair.options.width = 200.0f;
    pair.options.height = 100.0f;
    pair.frame();
    pair.frame();

    const Rect box = pair.input.frameOf("cmp");
    CHECK(box.width > 0.0f);

    InputFrame down;
    down.pointer = {box.x + box.width * 0.75f, box.y + box.height / 2.0f};
    down.pointerDown = true;
    pair.frame(down);
    CHECK_NEAR(pair.position, 0.75);

    // It keeps following after the pointer leaves the box, which is what makes
    // either end reachable.
    InputFrame past;
    past.pointer = {box.right() + 200.0f, box.y + box.height / 2.0f};
    past.pointerDown = true;
    pair.frame(past);
    CHECK_NEAR(pair.position, 1.0);
}

TEST("hovering moves nothing unless it was asked to") {
    Pair pair;
    pair.options.width = 200.0f;
    pair.options.height = 100.0f;
    pair.frame();
    pair.frame();
    const Rect box = pair.input.frameOf("cmp");

    InputFrame over;
    over.pointer = {box.x + box.width * 0.8f, box.y + box.height / 2.0f};
    pair.frame(over);
    // A comparison is something a reader sets and then looks at.
    CHECK_NEAR(pair.position, 0.5);

    pair.options.slideOnHover = true;
    pair.frame(over);
    CHECK_NEAR(pair.position, 0.8);
}

TEST("the keys aim at a seam, which is finer than a colour") {
    Pair pair;
    pair.frame();
    pair.input.focus("cmp", FocusSource::Keyboard);

    const auto press = [&](Key key) {
        InputFrame event;
        event.keys.push_back(KeyEvent{key});
        pair.frame(event);
        pair.input.focus("cmp", FocusSource::Keyboard);
    };

    press(Key::Right);
    CHECK_NEAR(pair.position, 0.52);
    press(Key::Left);
    CHECK_NEAR(pair.position, 0.50);
    // Page is the sweep, five times the arrow.
    press(Key::PageUp);
    CHECK_NEAR(pair.position, 0.60);
    press(Key::PageDown);
    CHECK_NEAR(pair.position, 0.50);
    press(Key::Home);
    CHECK_NEAR(pair.position, 0.0);
    press(Key::End);
    CHECK_NEAR(pair.position, 1.0);
    // And it does not run past either end.
    press(Key::Right);
    CHECK_NEAR(pair.position, 1.0);
}

TEST("a vertical comparison moves on the other axis") {
    Pair pair;
    pair.options.orientation = CompareOrientation::Vertical;
    pair.options.width = 200.0f;
    pair.options.height = 100.0f;
    pair.frame();
    pair.frame();
    const Rect box = pair.input.frameOf("cmp");

    InputFrame down;
    down.pointer = {box.x + box.width / 2.0f, box.y + box.height * 0.25f};
    down.pointerDown = true;
    pair.frame(down);
    CHECK_NEAR(pair.position, 0.25);
}
