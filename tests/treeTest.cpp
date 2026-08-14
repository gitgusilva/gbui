#include "gbui/scene/tree.hpp"

#include <string>
#include <vector>

#include "gbui/layout/layout.hpp"
#include "gbui/paint/paint.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/components.hpp"
#include "harness.hpp"

using namespace gbui;

TEST("children keep their insertion order and know their parent") {
    Arena arena;
    const NodeId root = arena.create();
    const NodeId first = arena.create();
    const NodeId second = arena.create();
    const NodeId third = arena.create();
    arena.addChild(root, first);
    arena.addChild(root, second);
    arena.addChild(root, third);

    std::vector<NodeId::Index> visited;
    arena.forEach(root, [&](NodeId id, const Node&, int) { visited.push_back(id.index()); });

    CHECK_EQ(visited.size(), std::size_t{4});
    CHECK_EQ(visited[1], first.index());
    CHECK_EQ(visited[2], second.index());
    CHECK_EQ(visited[3], third.index());
    CHECK(arena[third].parent == root);
    CHECK(arena[first].nextSibling == second);
}

TEST("interned text survives the arena growing past a block") {
    Arena arena;
    std::vector<std::string_view> views;
    std::vector<std::string> sources;

    // Well past the 4 KiB block size, so several blocks are in play and any
    // reallocation of the block list would dangle the earlier views.
    for (int i = 0; i < 2000; ++i) {
        sources.push_back("commit-" + std::to_string(i));
    }
    for (const auto& source : sources) {
        views.push_back(arena.intern(source));
    }

    for (std::size_t i = 0; i < views.size(); ++i) {
        CHECK_EQ(std::string(views[i]), sources[i]);
    }
}

TEST("interning a string larger than a block still returns one contiguous view") {
    Arena arena;
    const std::string huge(10000, 'x');
    const std::string_view view = arena.intern(huge);
    CHECK_EQ(view.size(), huge.size());
    CHECK_EQ(std::string(view), huge);
}

TEST("reset empties the tree while keeping the memory") {
    Arena arena;
    arena.reserve(256);
    for (int i = 0; i < 200; ++i) {
        const NodeId id = arena.create();
        arena[id].text = arena.intern("row");
    }
    const std::size_t before = arena.bytesUsed();

    arena.reset();

    CHECK(arena.empty());
    CHECK_EQ(arena.size(), std::size_t{0});
    // Capacity is retained, so the next frame writes over the same pages.
    CHECK(arena.bytesUsed() >= before / 2);
}

TEST("a scope closes its container even when a return skips the end") {
    Arena arena;
    Ui ui(arena);
    NodeId sibling;
    {
        auto column = ui.column();
        {
            auto row = ui.row();
            ui.add({});
            (void)row;
        }
        // Back at the column: this is a sibling of the row, not a child.
        sibling = ui.add({});
        (void)column;
    }
    CHECK(arena[sibling].parent == ui.root());
}

TEST("components build the tree they claim to") {
    Arena arena;
    Ui ui(arena);
    {
        auto panelScope = panel(ui);
        sectionHeading(ui, "UNSTAGED (1)");
        {
            auto row = listRow(ui, {.selected = true, .id = "row.theme"});
            text(ui, "theme.json");
            (void)row;
        }
        button(ui, "COMMIT", {.variant = ButtonVariant::Primary, .block = true});
        (void)panelScope;
    }

    const Theme theme = Theme::dark();
    LayoutContext context;
    context.theme = &theme;
    layout(arena, ui.root(), Rect{0, 0, 320, 200}, context);

    // The tagged row is findable, which is what hit testing and tests need.
    bool foundTag = false;
    arena.forEach(ui.root(), [&](NodeId, const Node& node, int) {
        if (node.id == "row.theme") foundTag = true;
    });
    CHECK(foundTag);

    DisplayList list;
    record(arena, ui.root(), theme, list);
    CHECK(!list.empty());

    // The primary button paints the accent somewhere in the list.
    bool paintedAccent = false;
    for (const auto& command : list.commands()) {
        if (const auto* fill = std::get_if<FillRect>(&command)) {
            if (fill->paint.color == theme.color(Token::Accent)) paintedAccent = true;
        }
    }
    CHECK(paintedAccent);
}

TEST("a transparent subtree costs no draw commands") {
    Arena arena;
    Ui ui(arena);
    {
        auto root = ui.column({.background = Fill{Token::Bg}});
        {
            auto hidden = ui.column({.background = Fill{Token::Accent}, .opacity = 0.0f});
            ui.label("invisible");
            (void)hidden;
        }
        (void)root;
    }

    const Theme theme = Theme::dark();
    LayoutContext context;
    context.theme = &theme;
    layout(arena, ui.root(), Rect{0, 0, 100, 100}, context);

    DisplayList list;
    record(arena, ui.root(), theme, list);

    // Only the root's background: the hidden branch is skipped entirely.
    CHECK_EQ(list.size(), std::size_t{1});
}

TEST("a naming scope prefixes what qualify returns") {
    Arena arena;
    Ui ui(arena);

    CHECK_EQ(ui.qualify("plain"), std::string_view("plain"));

    {
        auto outer = ui.ids("scada");
        CHECK_EQ(ui.qualify("hold"), std::string_view("scada.hold"));
        {
            auto inner = ui.ids("tank");
            CHECK_EQ(ui.qualify("clearwell"), std::string_view("scada.tank.clearwell"));
            CHECK_EQ(ui.idPrefix(), std::string_view("scada.tank"));
        }
        // The inner scope is gone; the outer one is not.
        CHECK_EQ(ui.qualify("auto"), std::string_view("scada.auto"));
        (void)outer;
    }
    CHECK_EQ(ui.qualify("plain"), std::string_view("plain"));
    CHECK(ui.idPrefix().empty());
}

TEST("a qualified name survives the call it was built in") {
    Arena arena;
    Ui ui(arena);
    std::string_view held;
    {
        auto scope = ui.ids("outer");
        held = ui.qualify("thing");
        (void)scope;
    }
    // Interned into the arena rather than pointing at the prefix buffer, which
    // the scope above has already shortened.
    CHECK_EQ(held, std::string_view("outer.thing"));
}

TEST("an empty scope name changes nothing") {
    Arena arena;
    Ui ui(arena);
    auto scope = ui.ids("");
    CHECK_EQ(ui.qualify("a"), std::string_view("a"));
    (void)scope;
}
