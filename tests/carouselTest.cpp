// Stepping, wrapping, and the clock that has to be stoppable.
#include <string>

#include "gbui/input/interaction.hpp"
#include "gbui/layout/layout.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/style/theme.hpp"
#include "gbui/widgets/containers.hpp"
#include "harness.hpp"

using namespace gbui;

namespace {

const Rect kWindow{0, 0, 520, 300};

TextMetrics measureFixed(std::string_view text, const TextStyle&, const Typography&, float) {
    std::size_t characters = 0;
    for (char c : text) {
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++characters;
    }
    return {static_cast<float>(characters) * 8.0f, 14.0f, 11.0f};
}

struct Strip {
    Theme theme = Theme::dark();
    Arena arena;
    Interaction input;
    CarouselState state;
    CarouselOptions options{};
    CarouselResult result{};
    std::size_t count = 6;

    void frame(float delta = 0.0f, const InputFrame& event = {}) {
        input.update(arena, arena.empty() ? NodeId{} : NodeId(0), event);

        arena.reset();
        Ui ui(arena);
        ui.setMeasure(&measureFixed, theme.typography());
        {
            auto root = ui.column({.width = kWindow.width});
            result = carousel(ui, input, "c", count, state, delta,
                              [](Ui& inner, std::size_t) {
                                  Style fill;
                                  fill.width = Length::percent(100);
                                  fill.height = Length::percent(100);
                                  auto scope = inner.scope(fill);
                                  (void)scope;
                              },
                              options);
            (void)root;
        }
        LayoutContext context;
        context.theme = &theme;
        context.measure = &measureFixed;
        layout(arena, ui.root(), kWindow, context);
    }

    /** Three, for the reason the toast harness gives: the first has no
     *  viewport, the second is placed correctly, the third reports it. */
    void settle() {
        frame();
        frame();
        frame();
    }

    /** Presses a tagged control the way a pointer does. */
    void press(std::string_view tag) {
        const Rect box = input.frameOf(tag);
        const Vec2 at{box.x + box.width / 2.0f, box.y + box.height / 2.0f};
        InputFrame down;
        down.pointer = at;
        down.pointerDown = true;
        frame(0.0f, down);
        InputFrame up;
        up.pointer = at;
        up.pointerDown = false;
        frame(0.0f, up);
    }

    void key(Key which) {
        input.focus("c.view", FocusSource::Keyboard);
        InputFrame event;
        event.keys.push_back(KeyEvent{which});
        frame(0.0f, event);
    }
};

}  // namespace

TEST("next moves by one slide, not by a screenful") {
    // Both conventions exist. This is the one that keeps a four-across gallery
    // usable: "next" is the thing after the one you are looking at, not a jump
    // that takes away everything you were reading.
    Strip strip;
    strip.options.slidesPerPage = 2.5f;
    strip.settle();

    strip.press("c.next");
    CHECK_EQ(strip.state.first, std::size_t{1});
    strip.press("c.next");
    CHECK_EQ(strip.state.first, std::size_t{2});
    strip.press("c.previous");
    CHECK_EQ(strip.state.first, std::size_t{1});
}

TEST("the strip stops where the last slide reaches the trailing edge") {
    // Not at the last index: resting past the stop would scroll empty space
    // into view and show fewer slides than the strip is built for.
    Strip strip;
    strip.count = 6;
    strip.options.slidesPerPage = 2.0f;
    strip.settle();

    for (int i = 0; i < 10; ++i) strip.press("c.next");
    CHECK_EQ(strip.state.first, std::size_t{4});   // 6 slides, 2 showing

    for (int i = 0; i < 10; ++i) strip.press("c.previous");
    CHECK_EQ(strip.state.first, std::size_t{0});
}

TEST("loop wraps at both ends, and off it the arrows go dead") {
    Strip strip;
    strip.count = 4;
    strip.options.loop = true;
    strip.settle();

    strip.press("c.previous");
    CHECK_EQ(strip.state.first, std::size_t{3});
    strip.press("c.next");
    CHECK_EQ(strip.state.first, std::size_t{0});

    // Without it, the arrow at the end is disabled rather than inert: a control
    // that does nothing and does not say so is a control a reader keeps
    // pressing.
    Strip fixed;
    fixed.count = 4;
    fixed.settle();
    bool disabled = false;
    for (std::size_t i = 0; i < fixed.arena.size(); ++i) {
        const Node& node = fixed.arena[NodeId{static_cast<std::uint32_t>(i)}];
        if (node.id == "c.previous") disabled = !node.focusable;
    }
    CHECK(disabled);
}

TEST("Home and End reach both ends in one press") {
    Strip strip;
    strip.count = 8;
    strip.settle();

    strip.key(Key::End);
    CHECK_EQ(strip.state.first, std::size_t{7});
    strip.key(Key::Home);
    CHECK_EQ(strip.state.first, std::size_t{0});
    strip.key(Key::Right);
    CHECK_EQ(strip.state.first, std::size_t{1});
    strip.key(Key::Left);
    CHECK_EQ(strip.state.first, std::size_t{0});
}

TEST("a vertical strip answers the other pair of arrows") {
    Strip strip;
    strip.options.orientation = CarouselOrientation::Vertical;
    strip.settle();

    strip.key(Key::Right);
    CHECK_EQ(strip.state.first, std::size_t{0});   // not its axis
    strip.key(Key::Down);
    CHECK_EQ(strip.state.first, std::size_t{1});
    strip.key(Key::Up);
    CHECK_EQ(strip.state.first, std::size_t{0});
}

TEST("a dot jumps straight to its slide") {
    Strip strip;
    strip.count = 5;
    strip.settle();

    strip.press("c.dot.3");
    CHECK_EQ(strip.state.first, std::size_t{3});
}

TEST("autoplay advances, and wraps whatever loop says") {
    // A strip that plays itself to the end and stops has spent the reader's
    // attention and then quietly become a still picture.
    Strip strip;
    strip.count = 3;
    strip.options.autoplay = 1.0;
    strip.settle();

    strip.frame(0.6f);
    CHECK_EQ(strip.state.first, std::size_t{0});
    strip.frame(0.6f);
    CHECK_EQ(strip.state.first, std::size_t{1});
    strip.frame(1.1f);
    CHECK_EQ(strip.state.first, std::size_t{2});
    strip.frame(1.1f);
    CHECK_EQ(strip.state.first, std::size_t{0});
}

TEST("autoplay stops while somebody is looking at it") {
    Strip strip;
    strip.options.autoplay = 1.0;
    strip.settle();

    const Rect view = strip.input.frameOf("c.view");
    CHECK(view.width > 0.0f);

    InputFrame over;
    over.pointer = {view.x + view.width / 2.0f, view.y + view.height / 2.0f};
    for (int i = 0; i < 5; ++i) strip.frame(1.0f, over);
    CHECK_EQ(strip.state.first, std::size_t{0});

    // And the keyboard being inside it counts as looking.
    strip.input.focus("c.view", FocusSource::Keyboard);
    for (int i = 0; i < 5; ++i) strip.frame(1.0f);
    CHECK_EQ(strip.state.first, std::size_t{0});
}

/**
 * WCAG's "pause, stop, hide", which is a rule rather than a judgement: anything
 * that moves on its own for more than five seconds needs a way to stop it. So
 * the button exists whenever autoplay does, and there is no option to remove
 * it — the option would be a switch labelled "make this inaccessible".
 */
TEST("an autoplaying carousel always has a pause button, and it works") {
    Strip strip;
    strip.options.autoplay = 1.0;
    strip.settle();
    CHECK(!strip.input.frameOf("c.play").empty());

    strip.press("c.play");
    CHECK(!strip.state.playing);

    for (int i = 0; i < 5; ++i) strip.frame(1.0f);
    CHECK_EQ(strip.state.first, std::size_t{0});

    strip.press("c.play");
    CHECK(strip.state.playing);
    strip.frame(1.1f);
    CHECK_EQ(strip.state.first, std::size_t{1});

    // No autoplay, no button: there is nothing to pause.
    Strip still;
    still.settle();
    CHECK(still.input.frameOf("c.play").empty());
}
