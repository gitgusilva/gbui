// What is on screen, and the two keys that make a tree a tree.
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

const Rect kWindow{0, 0, 360, 400};

TextMetrics measureFixed(std::string_view text, const TextStyle&, const Typography&, float) {
    std::size_t characters = 0;
    for (char c : text) {
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++characters;
    }
    return {static_cast<float>(characters) * 8.0f, 14.0f, 11.0f};
}

/**
 * A small hierarchy, flat and in pre-order:
 *
 *     src            0
 *       widgets      1
 *         a.cpp      2
 *         b.cpp      2
 *       ui.cpp       1
 *     docs           0
 *       guide.md     1
 *     README.md      0
 */
std::vector<TreeItem> sample() {
    return {
        {.id = "src", .label = "src", .depth = 0, .hasChildren = true},
        {.id = "src/widgets", .label = "widgets", .depth = 1, .hasChildren = true},
        {.id = "src/widgets/a.cpp", .label = "a.cpp", .depth = 2},
        {.id = "src/widgets/b.cpp", .label = "b.cpp", .depth = 2},
        {.id = "src/ui.cpp", .label = "ui.cpp", .depth = 1},
        {.id = "docs", .label = "docs", .depth = 0, .hasChildren = true},
        {.id = "docs/guide.md", .label = "guide.md", .depth = 1},
        {.id = "README.md", .label = "README.md", .depth = 0},
    };
}

struct Tree {
    Theme theme = Theme::dark();
    Arena arena;
    Interaction input;
    TreeState state;
    TreeViewOptions options{};
    TreeResult result{};
    std::vector<TreeItem> items = sample();

    void frame(const InputFrame& event = {}) {
        input.update(arena, arena.empty() ? NodeId{} : NodeId(0), event);

        arena.reset();
        Ui ui(arena);
        ui.setMeasure(&measureFixed, theme.typography());
        {
            auto root = ui.column({.width = kWindow.width, .height = kWindow.height});
            result = treeView(ui, input, "t", items, state, options);
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

    void key(Key which) {
        input.focus("t", FocusSource::Keyboard);
        InputFrame event;
        event.keys.push_back(KeyEvent{which});
        frame(event);
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

    /** Which rows the tree actually built, in order. */
    std::vector<std::string_view> onScreen() const {
        std::vector<std::string_view> out;
        for (const TreeItem& item : items) {
            const std::string tag = "t." + std::string(item.id);
            if (!input.frameOf(tag).empty()) out.push_back(item.id);
        }
        return out;
    }
};

}  // namespace

TEST("everything under a closed node is off screen, however deep") {
    Tree tree;
    tree.settle();
    // Nothing is open: three roots and nothing else.
    CHECK_EQ(tree.onScreen().size(), std::size_t{3});

    tree.state.expanded.emplace("src");
    tree.settle();
    // `src` opens, but `widgets` is still closed — so its children stay away.
    CHECK_EQ(tree.onScreen().size(), std::size_t{5});
    CHECK(tree.input.frameOf("t.src/widgets").width > 0.0f);
    CHECK(tree.input.frameOf("t.src/widgets/a.cpp").empty());

    tree.state.expanded.emplace("src/widgets");
    tree.settle();
    CHECK_EQ(tree.onScreen().size(), std::size_t{7});
}

/**
 * The pair that makes a tree a tree. Right opens a closed node and steps *into*
 * an open one; Left closes an open one and steps *out* of a closed one. Making
 * Right always step turns the control into an indented list.
 */
TEST("Right opens then steps in, and Left closes then steps out") {
    Tree tree;
    tree.settle();
    tree.state.focused = "src";
    tree.settle();

    tree.key(Key::Right);
    CHECK(tree.state.isExpanded("src"));
    CHECK(tree.state.focused == "src");   // opened, did not move

    tree.key(Key::Right);
    CHECK(tree.state.focused == "src/widgets");   // and now it steps in

    tree.key(Key::Right);
    CHECK(tree.state.isExpanded("src/widgets"));
    CHECK(tree.state.focused == "src/widgets");

    tree.key(Key::Left);
    CHECK(tree.state.focused == "src/widgets");   // it was open, so it closed
    CHECK(!tree.state.isExpanded("src/widgets"));

    tree.key(Key::Left);
    CHECK(tree.state.focused == "src");   // closed already, so out to the parent

    tree.key(Key::Left);
    CHECK(!tree.state.isExpanded("src"));
}

TEST("Left from a leaf goes to its parent, however deep it is") {
    Tree tree;
    tree.state.expanded.emplace("src");
    tree.state.expanded.emplace("src/widgets");
    tree.settle();
    tree.state.focused = "src/widgets/b.cpp";
    tree.settle();

    tree.key(Key::Left);
    CHECK(tree.state.focused == "src/widgets");
    tree.key(Key::Left);   // closes it
    tree.key(Key::Left);   // and now out
    CHECK(tree.state.focused == "src");
}

TEST("Up and Down walk what is on screen, not what is underneath it") {
    Tree tree;
    tree.state.expanded.emplace("src");
    tree.settle();
    tree.state.focused = "src";
    tree.settle();

    tree.key(Key::Down);
    CHECK(tree.state.focused == "src/widgets");
    tree.key(Key::Down);
    // Straight past a.cpp and b.cpp, which `widgets` is hiding.
    CHECK(tree.state.focused == "src/ui.cpp");
    tree.key(Key::Down);
    CHECK(tree.state.focused == "docs");

    tree.key(Key::Up);
    CHECK(tree.state.focused == "src/ui.cpp");
}

TEST("Home and End reach the ends of what is showing") {
    Tree tree;
    tree.settle();
    tree.state.focused = "docs";
    tree.settle();

    tree.key(Key::End);
    CHECK(tree.state.focused == "README.md");
    tree.key(Key::Down);
    CHECK(tree.state.focused == "README.md");   // the last is the last
    tree.key(Key::Home);
    CHECK(tree.state.focused == "src");
    tree.key(Key::Up);
    CHECK(tree.state.focused == "src");
}

TEST("the twisty opens without choosing, and the row chooses") {
    // "Show me what is in here" and "I want this one" are two gestures, and a
    // file browser that conflated them would select a directory every time
    // somebody looked inside it.
    Tree tree;
    tree.settle();

    tree.press("t.src.twisty");
    CHECK(tree.state.isExpanded("src"));
    CHECK(tree.state.selected.empty());
    CHECK(tree.result.toggled.has_value());
    CHECK(!tree.result.activated.has_value());

    tree.settle();
    tree.press("t.src/ui.cpp");
    CHECK(tree.state.selected == "src/ui.cpp");
    CHECK(tree.result.activated.has_value());
    CHECK(tree.result.selectionChanged);
}

TEST("Return chooses the row the keyboard is on, and a disabled one is not chosen") {
    Tree tree;
    tree.items[7].disabled = true;   // README.md
    tree.settle();
    tree.state.focused = "docs";
    tree.settle();

    tree.key(Key::Return);
    CHECK(tree.state.selected == "docs");

    tree.key(Key::End);
    CHECK(tree.state.focused == "README.md");
    tree.key(Key::Return);
    CHECK(tree.state.selected == "docs");   // unchanged
}

TEST("the selection and the keyboard are two things") {
    // Walking a tree is not choosing from it, the same separation `select`
    // makes between its highlight and its value.
    Tree tree;
    tree.settle();
    tree.press("t.docs");
    CHECK(tree.state.selected == "docs");
    CHECK(tree.state.focused == "docs");

    tree.key(Key::Up);
    CHECK(tree.state.focused == "src");
    CHECK(tree.state.selected == "docs");   // still
}

TEST("a big tree builds only the rows on screen, and keeps the focused one in it") {
    Tree tree;
    tree.items.clear();
    tree.items.push_back({.id = "root", .label = "root", .depth = 0, .hasChildren = true});
    static std::vector<std::string> names;
    names.clear();
    for (std::size_t i = 0; i < 400; ++i) names.push_back("file" + std::to_string(i));
    for (std::size_t i = 0; i < 400; ++i) {
        tree.items.push_back({.id = names[i], .label = names[i], .depth = 1});
    }
    tree.state.expanded.emplace("root");
    tree.options.height = 200.0f;
    tree.settle();

    CHECK_EQ(tree.result.shown.total, std::size_t{401});
    CHECK(tree.result.shown.count < 60);   // a screenful and an overscan

    tree.state.focused = "file0";
    tree.settle();
    CHECK_NEAR(tree.state.view.offset, 0.0);

    tree.key(Key::End);
    tree.settle();
    // The last row is a long way down, and the view followed it there.
    CHECK(tree.state.view.offset > 8000.0f);
}
