#include "gbui/input/interaction.hpp"

#include "gbui/layout/layout.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/style/theme.hpp"
#include "gbui/widgets/components.hpp"
#include "gbui/widgets/controls.hpp"
#include "harness.hpp"

#include <functional>

using namespace gbui;

namespace {

/** Two focusable boxes side by side, laid out so hit testing has real
 *  rectangles to answer with: "first" spans x 0–100, "second" x 100–200. */
struct Scene {
    Arena arena;
    Ui ui{arena};
    Theme theme = Theme::dark();

    Scene() {
        {
            auto row = ui.beginRow({.gap = 0});
            ui.add({.width = 100.0f, .height = 40.0f});
            ui.tag("first").focusable();
            ui.add({.width = 100.0f, .height = 40.0f});
            ui.tag("second").focusable();
            (void)row;
        }
        LayoutContext context;
        context.theme = &theme;
        layout(arena, ui.root(), Rect{0, 0, 200, 40}, context);
    }
};

/** One frame of input against the scene, since a press and its release are two
 *  frames and every test needs both. */
void feed(Interaction& input, const Scene& scene, InputFrame frame) {
    input.update(scene.arena, scene.ui.root(), frame);
}

InputFrame tab(bool shift = false) {
    InputFrame frame;
    // The pointer sits away from both boxes so hover cannot confuse a case.
    frame.pointer = {180.0f, 300.0f};
    frame.keys.push_back({Key::Tab, {.shift = shift}});
    return frame;
}

InputFrame pointer(Vec2 at, bool down) {
    InputFrame frame;
    frame.pointer = at;
    frame.pointerDown = down;
    return frame;
}

}  // namespace

TEST("Tab focuses the first control and shows its ring") {
    Scene scene;
    Interaction input;

    feed(input, scene, tab());
    CHECK(input.focused() == "first");
    CHECK(input.isFocusVisible("first"));
    CHECK(!input.isFocusVisible("second"));

    feed(input, scene, tab());
    CHECK(input.focused() == "second");
    CHECK(input.isFocusVisible("second"));

    feed(input, scene, tab(/*shift=*/true));
    CHECK(input.focused() == "first");
}

TEST("a click focuses without showing the ring") {
    Scene scene;
    Interaction input;

    feed(input, scene, pointer({50.0f, 20.0f}, true));
    CHECK(input.focused() == "first");
    CHECK(input.isFocused("first"));
    // Focus is there — Space still activates it — but nothing is drawn.
    CHECK(!input.isFocusVisible("first"));
    CHECK(!input.focusVisible());

    feed(input, scene, pointer({50.0f, 20.0f}, false));
    CHECK(input.clicked("first"));
    CHECK(!input.isFocusVisible("first"));
}

TEST("Tab after a click brings the ring back, and a click takes it away") {
    Scene scene;
    Interaction input;

    feed(input, scene, pointer({50.0f, 20.0f}, true));
    feed(input, scene, pointer({50.0f, 20.0f}, false));
    CHECK(!input.focusVisible());

    feed(input, scene, tab());
    CHECK(input.focused() == "second");  // Tab moves on from the clicked control.
    CHECK(input.isFocusVisible("second"));

    feed(input, scene, pointer({150.0f, 20.0f}, true));
    CHECK(input.focused() == "second");
    CHECK(!input.isFocusVisible("second"));
}

TEST("typing into a clicked control does not light its ring") {
    Scene scene;
    Interaction input;

    feed(input, scene, pointer({50.0f, 20.0f}, true));
    feed(input, scene, pointer({50.0f, 20.0f}, false));

    InputFrame typed;
    typed.pointer = {50.0f, 20.0f};
    typed.text = "a";
    feed(input, scene, typed);

    // The ring says how focus *arrived*; typing does not move it.
    CHECK(input.isFocused("first"));
    CHECK(!input.isFocusVisible("first"));
}

TEST("programmatic focus follows the modality the user last used") {
    Scene scene;
    Interaction input;

    // After a click, a label focusing its field draws no ring…
    feed(input, scene, pointer({50.0f, 20.0f}, true));
    feed(input, scene, pointer({50.0f, 20.0f}, false));
    input.focus("second");
    CHECK(input.isFocused("second"));
    CHECK(!input.isFocusVisible("second"));

    // …and after the keyboard, the same call does.
    feed(input, scene, tab());
    input.focus("first");
    CHECK(input.isFocusVisible("first"));

    // An explicit source overrides the guess in both directions.
    input.focus("second", FocusSource::Pointer);
    CHECK(!input.isFocusVisible("second"));
    input.focus("second", FocusSource::Keyboard);
    CHECK(input.isFocusVisible("second"));
}

TEST("clicking nothing blurs, and blur clears the ring") {
    Scene scene;
    Interaction input;

    feed(input, scene, tab());
    CHECK(input.focusVisible());

    feed(input, scene, pointer({180.0f, 300.0f}, true));
    CHECK(input.focused().empty());
    CHECK(!input.focusVisible());

    input.focus("first", FocusSource::Keyboard);
    input.blur();
    CHECK(input.focused().empty());
    CHECK(!input.focusVisible());
}

// ---- cursors ---------------------------------------------------------------

namespace {

/** Lays out one widget alone and reports the cursor over its middle. This is
 *  the whole chain a real window uses: the component declares a cursor, the
 *  tree carries it, and `Interaction` resolves it from the node under the
 *  pointer. */
Cursor cursorOver(const std::function<void(Ui&, const Interaction&)>& build) {
    Arena arena;
    Ui ui(arena);
    Theme theme = Theme::dark();
    Interaction input;

    // Two frames: the first builds a tree with no interaction state, the
    // second resolves the pointer against the geometry the first produced.
    NodeId root;
    for (int frame = 0; frame < 2; ++frame) {
        arena.reset();
        Ui rebuilt(arena);
        {
            auto column = rebuilt.beginColumn({.width = 200.0f});
            build(rebuilt, input);
            (void)column;
        }
        root = rebuilt.root();
        LayoutContext context;
        context.theme = &theme;
        layout(arena, root, Rect{0, 0, 200, 200}, context);

        InputFrame events;
        events.pointer = {20.0f, 10.0f};
        input.update(arena, root, events);
    }
    (void)ui;
    return input.cursor();
}

}  // namespace

TEST("a button asks for the pointer, and says no when disabled") {
    CHECK(cursorOver([](Ui& ui, const Interaction&) {
              button(ui, "PUSH", {.id = "b"});
          }) == Cursor::Pointer);
    CHECK(cursorOver([](Ui& ui, const Interaction&) {
              button(ui, "PUSH", {.disabled = true, .id = "b"});
          }) == Cursor::NotAllowed);
}

TEST("a checkbox asks for the pointer across its whole row") {
    CHECK(cursorOver([](Ui& ui, const Interaction& input) {
              (void)checkbox(ui, input, "c", false, {.label = "Show tags"});
          }) == Cursor::Pointer);
}

TEST("a text field asks for the text cursor") {
    CHECK(cursorOver([](Ui& ui, const Interaction& input) {
              static TextEditState state{"origin/main", 0, 0};
              textField(ui, input, "t", state, {});
          }) == Cursor::Text);
}

TEST("a drag keeps its cursor after the pointer leaves the control") {
    Arena arena;
    Ui ui(arena);
    Theme theme = Theme::dark();
    {
        auto row = ui.beginRow({.gap = 0});
        ui.add({.width = 40.0f, .height = 40.0f});
        ui.tag("thumb").cursor(Cursor::Grabbing);
        ui.add({.width = 160.0f, .height = 40.0f});  // plain space beside it
        (void)row;
    }
    LayoutContext context;
    context.theme = &theme;
    layout(arena, ui.root(), Rect{0, 0, 200, 40}, context);

    Interaction input;
    // Press on the thumb…
    InputFrame press;
    press.pointer = {20.0f, 20.0f};
    press.pointerDown = true;
    input.update(arena, ui.root(), press);
    CHECK(input.cursor() == Cursor::Grabbing);

    // …then drag well off it. A scrollbar drag does this constantly, and a
    // cursor that snaps back to an arrow says the drag ended when it has not.
    InputFrame away;
    away.pointer = {150.0f, 20.0f};
    away.pointerDown = true;
    input.update(arena, ui.root(), away);
    CHECK(input.cursor() == Cursor::Grabbing);

    // Releasing hands it back to whatever is under the pointer.
    InputFrame release;
    release.pointer = {150.0f, 20.0f};
    release.pointerDown = false;
    input.update(arena, ui.root(), release);
    CHECK(input.cursor() == Cursor::Default);
}

TEST("focus falls to the nearest surviving ancestor, not off the tree") {
    Theme theme = Theme::dark();
    Interaction input;

    // Frame one: a list holding a row, and the row takes the keyboard — which
    // is what clicking a row does, since the row is what the pointer hit.
    const auto build = [&](bool withRow) {
        Arena arena;
        Ui ui(arena);
        {
            auto list = ui.beginColumn({.width = 100.0f});
            ui.tag("list").focusable();
            if (withRow) {
                ui.add({.width = 100.0f, .height = 20.0f});
                ui.tag("list.row");
            }
            (void)list;
        }
        LayoutContext context;
        context.theme = &theme;
        layout(arena, ui.root(), Rect{0, 0, 100, 100}, context);
        input.update(arena, ui.root(), InputFrame{});
    };

    build(true);
    input.focus("list.row");
    build(true);
    CHECK(input.isFocusedWithin("list"));

    // The row scrolls out of a virtualised slice and stops being built. Focus
    // must land on the list rather than on nothing: the container's own key
    // handling is guarded by `isFocusedWithin`, so dropping it here is what
    // makes a list stop responding to the arrows that scrolled it.
    build(false);
    CHECK(input.isFocusedWithin("list"));
    CHECK_EQ(std::string(input.focused()), std::string("list"));
}

TEST("a popup outside its parent's frame is still hittable") {
    Arena arena;
    Ui ui(arena);
    Theme theme = Theme::dark();
    {
        // A short row, with a popup hanging under it that is placed *below* the
        // row on purpose — which is what every dropdown in this toolkit is.
        auto row = ui.beginRow({.height = 30.0f});
        ui.add({.width = 200.0f, .height = 30.0f});
        ui.tag("box");

        Style popup;
        popup.position = Position::Fixed;
        popup.layer = Layer::Overlay;
        popup.left = 0.0f;
        popup.top = 40.0f;
        popup.width = 200.0f;
        popup.height = 100.0f;
        ui.add(popup);
        ui.tag("popup");
        (void)row;
    }
    LayoutContext context;
    context.theme = &theme;
    layout(arena, ui.root(), Rect{0, 0, 200, 200}, context);

    Interaction input;
    InputFrame frame;
    frame.pointer = {100.0f, 90.0f};  // inside the popup, far outside the row
    input.update(arena, ui.root(), frame);

    // The row misses the pointer, and stopping there is what made a select's
    // list unhoverable and unclickable.
    CHECK_EQ(std::string(input.hovered()), std::string("popup"));
}
