// Getting about a set of pictures, and what happens at the ends.
#include <cstdint>
#include <string>
#include <vector>

#include "gbui/input/interaction.hpp"
#include "gbui/layout/layout.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/style/theme.hpp"
#include "gbui/widgets/containers.hpp"
#include "harness.hpp"

using namespace gbui;

namespace {

const Rect kWindow{0, 0, 480, 420};

TextMetrics measureFixed(std::string_view text, const TextStyle&, const Typography&, float) {
    std::size_t characters = 0;
    for (char c : text) {
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++characters;
    }
    return {static_cast<float>(characters) * 8.0f, 14.0f, 11.0f};
}

/** Four tiny pictures, drawn rather than loaded: nothing here decodes. */
struct Plates {
    std::vector<std::vector<std::uint8_t>> pixels;

    explicit Plates(std::size_t count) {
        for (std::size_t i = 0; i < count; ++i) {
            pixels.emplace_back(4 * 4 * 4, static_cast<std::uint8_t>(40 * i + 20));
        }
    }
    Bitmap at(std::size_t i) const { return Bitmap{pixels[i].data(), 4, 4, 0}; }
};

struct Viewer {
    Theme theme = Theme::dark();
    Arena arena;
    Interaction input;
    GalleryState state;
    GalleryOptions options{};
    GalleryResult result{};
    Plates plates{4};
    std::vector<GalleryItem> items;

    Viewer() {
        static const char* captions[] = {"One", "Two", "Three", "Four"};
        for (std::size_t i = 0; i < 4; ++i) {
            items.push_back({.image = plates.at(i), .caption = captions[i], .alt = captions[i]});
        }
    }

    void frame(const InputFrame& event = {}) {
        input.update(arena, arena.empty() ? NodeId{} : NodeId(0), event);

        arena.reset();
        Ui ui(arena);
        ui.setMeasure(&measureFixed, theme.typography());
        {
            auto root = ui.column({.width = kWindow.width});
            result = gallery(ui, input, "g", items, state, options);
            (void)root;
        }
        LayoutContext context;
        context.theme = &theme;
        context.measure = &measureFixed;
        layout(arena, ui.root(), kWindow, context);
    }

    void settle() {
        frame();
        frame();
        frame();
    }

    void press(std::string_view tag) {
        const Rect box = input.frameOf(tag);
        const Vec2 at{box.x + box.width / 2.0f, box.y + box.height / 2.0f};
        InputFrame down;
        down.pointer = at;
        down.pointerDown = true;
        frame(down);
        InputFrame up;
        up.pointer = at;
        up.pointerDown = false;
        frame(up);
    }

    void key(Key which) {
        input.focus("g.stage", FocusSource::Keyboard);
        InputFrame event;
        event.keys.push_back(KeyEvent{which});
        frame(event);
    }
};

}  // namespace

TEST("an empty set draws nothing rather than an empty frame") {
    // An empty frame looks like a picture that failed to load, which is a
    // different thing to report and not this component's to report.
    Theme theme = Theme::dark();
    Arena arena;
    Interaction input;
    GalleryState state;
    const std::vector<GalleryItem> none;

    Ui ui(arena);
    NodeId root;
    {
        auto column = ui.column({.width = kWindow.width});
        (void)gallery(ui, input, "g", none, state);
        root = column.id();
    }
    LayoutContext context;
    context.theme = &theme;
    layout(arena, root, kWindow, context);

    // The column, and nothing inside it.
    CHECK_EQ(arena.size(), std::size_t{1});
}

TEST("the arrows and the arrow keys walk the set, and stop at the ends") {
    Viewer viewer;
    viewer.settle();

    viewer.press("g.next");
    CHECK_EQ(viewer.state.current, std::size_t{1});
    viewer.key(Key::Right);
    CHECK_EQ(viewer.state.current, std::size_t{2});
    viewer.key(Key::Left);
    CHECK_EQ(viewer.state.current, std::size_t{1});

    viewer.key(Key::End);
    CHECK_EQ(viewer.state.current, std::size_t{3});
    viewer.key(Key::Right);
    CHECK_EQ(viewer.state.current, std::size_t{3});   // the last is the last
    viewer.key(Key::Home);
    CHECK_EQ(viewer.state.current, std::size_t{0});
    viewer.key(Key::Left);
    CHECK_EQ(viewer.state.current, std::size_t{0});
}

TEST("loop makes both ends meet") {
    Viewer viewer;
    viewer.options.loop = true;
    viewer.settle();

    viewer.key(Key::Left);
    CHECK_EQ(viewer.state.current, std::size_t{3});
    viewer.key(Key::Right);
    CHECK_EQ(viewer.state.current, std::size_t{0});
}

TEST("a thumbnail goes straight to its picture") {
    Viewer viewer;
    viewer.settle();

    viewer.press("g.thumb.2");
    CHECK_EQ(viewer.state.current, std::size_t{2});
    CHECK(viewer.result.changed);

    // Pressing the one already showing is not a change.
    viewer.press("g.thumb.2");
    CHECK(!viewer.result.changed);
}

TEST("the strip keeps the current thumbnail in view") {
    // Forty pictures is a strip wider than any window, and a keyboard walk that
    // leaves the selection off screen is a keyboard walk nobody can follow.
    Plates many{40};
    Viewer viewer;
    viewer.items.clear();
    for (std::size_t i = 0; i < 40; ++i) {
        viewer.items.push_back({.image = many.at(i)});
    }
    viewer.options.thumbnailSize = 44.0f;
    viewer.settle();
    CHECK_NEAR(viewer.state.thumbnails.offset, 0.0);

    viewer.key(Key::End);
    viewer.settle();
    CHECK(viewer.state.thumbnails.offset > 100.0f);

    viewer.key(Key::Home);
    viewer.settle();
    CHECK_NEAR(viewer.state.thumbnails.offset, 0.0);
}

TEST("the gallery is one keyboard stop, not one per thumbnail") {
    // The roving pattern the tab strip and the calendar already use: Tab
    // reaches the gallery once, and the arrows move inside it. Forty
    // thumbnails as forty Tab stops is forty presses to get past a picture.
    Viewer viewer;
    viewer.settle();

    std::vector<std::string_view> stops;
    for (std::size_t i = 0; i < viewer.arena.size(); ++i) {
        const Node& node = viewer.arena[NodeId{static_cast<std::uint32_t>(i)}];
        if (node.focusable && !node.id.empty()) stops.push_back(node.id);
    }
    for (const std::string_view tag : stops) {
        CHECK(tag == "g.stage" || tag == "g.previous" || tag == "g.next");
    }
}
