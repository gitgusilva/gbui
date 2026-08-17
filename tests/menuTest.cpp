// A menu, and the same menu at a point: the keyboard, the roles, the dismissal.
#include "gbui/widgets/contextMenu.hpp"

#include <string>
#include <vector>

#include "gbui/input/interaction.hpp"
#include "gbui/layout/layout.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/style/theme.hpp"
#include "gbui/widgets/elements.hpp"
#include "harness.hpp"

using namespace gbui;

namespace {

const Rect kWindow{0, 0, 640, 480};

TextMetrics measureFixed(std::string_view text, const TextStyle& style, const Typography& type,
                         float) {
    std::size_t characters = 0;
    for (char c : text) {
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++characters;
    }
    const float size = style.size > 0.0f ? style.size : type.uiFontSize;
    return {static_cast<float>(characters) * size * 0.55f, size * 1.35f, size * 0.78f};
}

const std::vector<MenuEntry> kEntries = {
    {.id = "stage", .label = "Stage file", .shortcut = "Ctrl+S"},
    {.id = "tags", .label = "Show tags", .checked = true},
    {},   // a separator
    {.id = "later", .label = "Not yet", .disabled = true},
    {.id = "discard", .label = "Discard changes", .danger = true},
};

/** A button with a menu hanging off it, driven the way the frame loop does. */
struct Bench {
    Theme theme = Theme::dark();
    Arena arena;
    Interaction input;
    MenuState state;
    MenuResult result{};
    bool atPointer = false;
    Vec2 at{200.0f, 140.0f};

    void frame(const InputFrame& event = {}) {
        input.update(arena, arena.empty() ? NodeId{} : NodeId(0), event);
        arena.reset();
        Ui ui(arena);
        ui.setMeasure(&measureFixed, theme.typography());
        NodeId root;
        {
            auto column = ui.column({.gap = 8.0f, .padding = Edges::all(12.0f)});
            (void)button(ui, input, "BRANCH", {.id = "anchor"});
            MenuOptions options;
            options.name = "Branch";
            result = atPointer ? contextMenu(ui, input, "m", at, kEntries, state, options)
                               : menu(ui, input, "m", "anchor", kEntries, state, options);
            if (result.focus) input.focus(*result.focus, FocusSource::Keyboard);
            root = column.id();
        }
        LayoutContext context;
        context.theme = &theme;
        context.measure = &measureFixed;
        layout(arena, root, kWindow, context);
    }

    void settle() {
        frame();
        frame();
        frame();
    }

    void press(Key key) {
        InputFrame event;
        event.keys.push_back(KeyEvent{key});
        frame(event);
    }

    const Accessibility* accessibilityOf(std::string_view tag) const {
        for (std::size_t i = 0; i < arena.size(); ++i) {
            const NodeId id{static_cast<std::uint32_t>(i)};
            if (arena[id].id == tag) return arena.accessibility(id);
        }
        return nullptr;
    }

    bool focusableAt(std::string_view tag) const {
        for (std::size_t i = 0; i < arena.size(); ++i) {
            const NodeId id{static_cast<std::uint32_t>(i)};
            if (arena[id].id == tag) return arena[id].focusable;
        }
        return false;
    }

    /** Just the press, which is the frame a dismissal happens on: the release
     *  is a frame later and `pressedOutside` is deliberately an edge. */
    void pressAt(Vec2 point) {
        InputFrame down;
        down.pointer = point;
        down.pointerDown = true;
        frame(down);
    }

    void clickAt(Vec2 point) {
        InputFrame down;
        down.pointer = point;
        down.pointerDown = true;
        frame(down);
        InputFrame up;
        up.pointer = point;
        frame(up);
    }

    Vec2 centreOf(std::string_view tag) const {
        const Rect box = input.frameOf(tag);
        return {box.x + box.width / 2.0f, box.y + box.height / 2.0f};
    }
};

}  // namespace

TEST("a menu is one Tab stop and the rows are not stops at all") {
    // Nine commands that each took the keyboard would be nine Tab presses to
    // cross one menu.
    Bench bench;
    bench.settle();
    CHECK(bench.focusableAt("m"));
    CHECK(!bench.focusableAt("m.stage"));
    CHECK(!bench.focusableAt("m.discard"));
}

TEST("it asks for the keyboard on the frame it opens") {
    // Otherwise the arrows do nothing until the reader clicks a row, which is
    // the one gesture a keyboard user cannot make.
    Bench bench;
    bench.frame();
    CHECK(bench.result.focus.has_value());
    bench.settle();
    CHECK(bench.input.isFocusedWithin("m"));
}

TEST("the arrows walk the rows, skipping the separator and the disabled one") {
    // A highlight that stopped on either would look stuck, and Enter on it does
    // nothing — which reads as a broken menu rather than as a skipped row.
    Bench bench;
    bench.settle();

    bench.press(Key::Down);
    CHECK_EQ(bench.state.highlighted, std::string("stage"));
    bench.press(Key::Down);
    CHECK_EQ(bench.state.highlighted, std::string("tags"));
    bench.press(Key::Down);
    CHECK_EQ(bench.state.highlighted, std::string("discard"));
    // And round, because a desktop menu walked to its end comes back rather
    // than stopping dead.
    bench.press(Key::Down);
    CHECK_EQ(bench.state.highlighted, std::string("stage"));

    bench.press(Key::End);
    CHECK_EQ(bench.state.highlighted, std::string("discard"));
    bench.press(Key::Home);
    CHECK_EQ(bench.state.highlighted, std::string("stage"));
}

TEST("Return activates what is highlighted, and Escape asks to close") {
    Bench bench;
    bench.settle();
    bench.press(Key::Down);
    bench.press(Key::Down);
    CHECK_EQ(bench.state.highlighted, std::string("tags"));

    bench.press(Key::Return);
    CHECK(bench.result.chosen.has_value());
    CHECK_EQ(std::string(bench.result.chosen.value_or("")), std::string("tags"));

    bench.press(Key::Escape);
    CHECK(bench.result.dismissed);
}

TEST("a row that is pressed is the row that is reported") {
    Bench bench;
    bench.settle();
    bench.clickAt(bench.centreOf("m.discard"));
    CHECK_EQ(std::string(bench.result.chosen.value_or("")), std::string("discard"));
}

TEST("a press outside asks to close, and one inside does not") {
    Bench bench;
    bench.settle();

    const Rect box = bench.input.frameOf("m");
    CHECK(box.width > 0.0f);
    // Inside the menu but not on a row — its padding. "The press hit nothing
    // tagged" would call this outside.
    bench.pressAt({box.x + 2.0f, box.y + 2.0f});
    CHECK(!bench.result.dismissed);

    bench.frame();
    bench.pressAt({box.right() + 60.0f, box.bottom() + 60.0f});
    CHECK(bench.result.dismissed);
}

TEST("the menu and its rows say what they are") {
    Bench bench;
    bench.settle();
    CHECK(bench.accessibilityOf("m")->role == Role::Menu);
    CHECK_EQ(std::string(bench.accessibilityOf("m")->name), std::string("Branch"));
    CHECK(bench.accessibilityOf("m.tags")->state.checked == Flag::True);
    CHECK(bench.accessibilityOf("m.later")->state.disabled == Flag::True);
}

TEST("a context menu opens at the point it was given, not under an anchor") {
    Bench bench;
    bench.atPointer = true;
    bench.at = {260.0f, 150.0f};
    bench.settle();

    const Rect box = bench.input.frameOf("m");
    CHECK(box.width > 0.0f);
    // Its corner is at the pointer: no gap, because six pixels of air reads as
    // a menu that missed.
    CHECK_NEAR(box.x, 260.0);
    CHECK_NEAR(box.y, 150.0);
}

TEST("a context menu near an edge comes back inside the window") {
    // The same placement engine a dropdown uses, which is the whole reason a
    // point is expressed as a zero-sized anchor rather than as a second path.
    Bench bench;
    bench.atPointer = true;
    bench.at = {630.0f, 470.0f};
    bench.settle();

    const Rect box = bench.input.frameOf("m");
    CHECK(box.width > 0.0f);
    CHECK(box.right() <= kWindow.width + 0.01f);
    CHECK(box.bottom() <= kWindow.height + 0.01f);
}

TEST("the secondary button is its own press, and moves no focus") {
    // A right-click on a slider must not take the keyboard off whatever had it
    // and must not drag the thumb, which is what sharing the primary path would
    // do.
    Theme theme = Theme::dark();
    Arena arena;
    Interaction input;

    const auto build = [&](const InputFrame& event) {
        input.update(arena, arena.empty() ? NodeId{} : NodeId(0), event);
        arena.reset();
        Ui ui(arena);
        ui.setMeasure(&measureFixed, theme.typography());
        {
            auto column = ui.column({.gap = 6.0f, .padding = Edges::all(10.0f)});
            (void)button(ui, input, "ROW", {.id = "row"});
            (void)button(ui, input, "OTHER", {.id = "other"});
            (void)column;
        }
        LayoutContext context;
        context.theme = &theme;
        context.measure = &measureFixed;
        layout(arena, ui.root(), kWindow, context);
    };
    build({});
    build({});
    input.focus("other", FocusSource::Keyboard);

    const Rect row = input.frameOf("row");
    const Vec2 at{row.x + row.width / 2.0f, row.y + row.height / 2.0f};

    InputFrame down;
    down.pointer = at;
    down.secondaryDown = true;
    build(down);
    CHECK(input.dragging().empty());        // no drag started
    CHECK(input.focused() == "other");      // and the keyboard did not move

    InputFrame up;
    up.pointer = at;
    build(up);
    CHECK(input.secondaryClicked("row"));
    CHECK(!input.clicked("row"));           // and it is not a primary click
}
