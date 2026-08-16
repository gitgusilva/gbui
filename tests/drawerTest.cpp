// Which edge it comes from, what it blocks, and how it leaves.
#include "gbui/widgets/drawer.hpp"

#include <string>

#include "gbui/anim/animator.hpp"
#include "gbui/input/interaction.hpp"
#include "gbui/layout/layout.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/style/theme.hpp"
#include "gbui/widgets/elements.hpp"
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

/** A drawer with a button behind it, so there is somewhere else for Tab and
 *  the pointer to go. Driven the way the frame loop drives it. */
struct Sheet {
    Theme theme = Theme::dark();
    Arena arena;
    Interaction input;
    Animator animator;
    DrawerOptions options{};
    bool open = false;
    DrawerResult result{};
    /** Whether the body callback was reached at all. */
    bool bodyBuilt = false;

    void frame(const InputFrame& event = {}, float delta = 0.0f) {
        input.update(arena, arena.empty() ? NodeId{} : NodeId(0), event);
        animator.tick(delta);

        arena.reset();
        Ui ui(arena);
        ui.setMeasure(&measureFixed, theme.typography());
        ui.setAnimator(&animator);
        NodeId root;
        {
            auto page = ui.column({.width = kWindow.width, .height = kWindow.height});
            (void)button(ui, input, "BEHIND", {.id = "behind"});
            {
                auto sheet = drawer(ui, input, "sheet", "Filters", open, options);
                result = sheet.result;
                bodyBuilt = false;
                if (sheet.result.visible) {
                    bodyBuilt = true;
                    (void)button(ui, input, "INSIDE", {.id = "inside"});
                }
            }
            root = page.id();
        }
        LayoutContext context;
        context.theme = &theme;
        context.measure = &measureFixed;
        layout(arena, root, kWindow, context);
    }

    /** Frames until the slide has finished, whichever way it is going. */
    void settle() {
        for (int i = 0; i < 30; ++i) frame({}, 0.05f);
    }

    Rect panel() const { return input.frameOf("sheet"); }

    void clickAt(Vec2 at) {
        InputFrame down;
        down.pointer = at;
        down.pointerDown = true;
        frame(down);
        InputFrame up;
        up.pointer = at;
        frame(up);
    }

    void press(Key key) {
        InputFrame event;
        event.keys.push_back(KeyEvent{key});
        frame(event);
    }
};

}  // namespace

TEST("a closed drawer is not on screen and costs no panel") {
    Sheet sheet;
    sheet.settle();
    CHECK(!sheet.result.visible);
    CHECK(!sheet.bodyBuilt);
    CHECK(sheet.panel().empty());
}

TEST("it comes in from the side it was told to, and fills the other axis") {
    Sheet sheet;
    sheet.options.side = DrawerSide::Right;
    sheet.options.size = 260.0f;
    sheet.open = true;
    sheet.settle();

    const Rect panel = sheet.panel();
    CHECK_NEAR(panel.width, 260.0);
    CHECK_NEAR(panel.height, 600.0);
    CHECK_NEAR(panel.right(), 800.0);

    sheet.options.side = DrawerSide::Left;
    sheet.settle();
    CHECK_NEAR(sheet.panel().x, 0.0);

    sheet.options.side = DrawerSide::Bottom;
    sheet.settle();
    CHECK_NEAR(sheet.panel().width, 800.0);
    CHECK_NEAR(sheet.panel().height, 260.0);
    CHECK_NEAR(sheet.panel().bottom(), 600.0);
}

TEST("a drawer wider than the window is as wide as the window") {
    // Otherwise its close button is off the edge, which is a panel with no way
    // out on exactly the screens a drawer exists for.
    Sheet sheet;
    sheet.options.size = 2000.0f;
    sheet.open = true;
    sheet.settle();
    CHECK_NEAR(sheet.panel().width, 800.0);
}

TEST("it slides rather than appearing, and slides back out") {
    Sheet sheet;
    sheet.options.side = DrawerSide::Right;
    sheet.options.size = 260.0f;
    sheet.open = true;

    // One short frame in: on screen, but not all the way across yet.
    sheet.frame({}, 0.0f);
    sheet.frame({}, 0.05f);
    const Rect partway = sheet.panel();
    CHECK(partway.width > 0.0f);
    CHECK(partway.x > 800.0f - 260.0f);   // still hanging off the right

    sheet.settle();
    CHECK_NEAR(sheet.panel().x, 800.0 - 260.0);

    // And the half that is the whole reason the flag is a parameter: it is
    // still built while it leaves.
    sheet.open = false;
    // Three frames, and each one is a different lag: the first tells the
    // animator its new target — it ticks before the build, so the value it
    // hands back is still the old one — the second is the first that has
    // actually moved, and the third is what makes `frameOf` report it, because
    // the interaction layer is always one frame behind the tree.
    sheet.frame({}, 0.05f);
    sheet.frame({}, 0.05f);
    sheet.frame({}, 0.05f);
    CHECK(sheet.result.visible);
    CHECK(sheet.panel().x > 800.0f - 260.0f);

    sheet.settle();
    CHECK(!sheet.result.visible);
}

TEST("a modal drawer keeps the keyboard and a plain one does not") {
    Sheet sheet;
    sheet.options.modal = true;
    sheet.open = true;
    sheet.settle();

    sheet.input.focus("inside", FocusSource::Keyboard);
    sheet.frame();
    sheet.press(Key::Tab);
    sheet.frame();
    // Round the two things inside the panel — the close button and the button
    // — and never out to the one behind it.
    CHECK(sheet.input.focused() != "behind");

    Sheet pane;
    pane.options.modal = false;
    pane.open = true;
    pane.settle();
    // Nothing is trapped, so the page behind is still Tab's to reach.
    bool trapped = false;
    for (std::size_t i = 0; i < pane.arena.size(); ++i) {
        if (pane.arena[NodeId{static_cast<std::uint32_t>(i)}].trapsFocus) trapped = true;
    }
    CHECK(!trapped);
}

TEST("only a blocking drawer draws a backdrop") {
    Sheet sheet;
    sheet.open = true;
    sheet.settle();
    CHECK(!sheet.input.frameOf("sheet.backdrop").empty());

    Sheet pane;
    pane.options.modal = false;
    pane.open = true;
    pane.settle();
    // A pane beside the work dimming the work would be telling the reader they
    // cannot use something they can.
    CHECK(pane.input.frameOf("sheet.backdrop").empty());
}

TEST("the three ways out, and turning two of them off") {
    Sheet sheet;
    sheet.open = true;
    sheet.settle();

    sheet.press(Key::Escape);
    CHECK(sheet.result.dismissed);

    sheet.clickAt({20.0f, 300.0f});   // the backdrop, far from a right drawer
    CHECK(sheet.result.dismissed);

    const Rect close = sheet.input.frameOf("sheet.close");
    CHECK(close.width > 0.0f);
    sheet.clickAt({close.x + close.width / 2.0f, close.y + close.height / 2.0f});
    CHECK(sheet.result.dismissed);

    Sheet stubborn;
    stubborn.options.dismissOnEscape = false;
    stubborn.options.dismissOnBackdrop = false;
    stubborn.open = true;
    stubborn.settle();
    stubborn.press(Key::Escape);
    CHECK(!stubborn.result.dismissed);
    stubborn.clickAt({20.0f, 300.0f});
    CHECK(!stubborn.result.dismissed);
}

TEST("a drawer with no header is all body") {
    Sheet sheet;
    sheet.options.header = false;
    sheet.open = true;
    sheet.settle();
    CHECK(sheet.input.frameOf("sheet.header").empty());
    CHECK(sheet.input.frameOf("sheet.close").empty());
    CHECK(sheet.bodyBuilt);

    Sheet titled;
    titled.options.closeButton = false;
    titled.open = true;
    titled.settle();
    CHECK(!titled.input.frameOf("sheet.header").empty());
    CHECK(titled.input.frameOf("sheet.close").empty());
}
