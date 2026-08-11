#include "gbui/widgets/components.hpp"
#include "gbui/widgets/controls.hpp"

#include <string>

#include "gbui/input/interaction.hpp"
#include "gbui/layout/layout.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/style/theme.hpp"
#include <cmath>
#include "gbui/platform/font.hpp"
#include "harness.hpp"

using namespace gbui;

namespace {

constexpr float kCharacterWidth = 10.0f;
const Rect kWindow{0, 0, 400, 60};

/** A font where every character is ten pixels wide, so a click at x is
 *  expected at character x/10 and the assertions read as arithmetic. */
TextMetrics measureFixed(std::string_view text, const TextStyle&, const Typography&, float) {
    std::size_t characters = 0;
    for (char c : text) {
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++characters;
    }
    return {static_cast<float>(characters) * kCharacterWidth, 14.0f, 11.0f};
}

/** Drives a field the way the frame loop does: resolve the input against the
 *  previous tree, then build. Returns the tree it built, so the next frame can
 *  be resolved against it. */
struct Field {
    Theme theme = Theme::dark();
    Arena arena;
    Interaction input;
    TextEditState state;
    TextFieldOptions options{};

    explicit Field(std::string text) {
        state = TextEditState{std::move(text), 0, 0};
        // Spanning the row, so a click past the end of the text is still a
        // click inside the field.
        options.grow = 1.0f;
    }

    void frame(const InputFrame& event = {}) {
        input.update(arena, arena.empty() ? NodeId{} : NodeId(0), event);

        arena.reset();
        Ui ui(arena);
        ui.setMeasure(&measureFixed, theme.typography());
        {
            auto root = ui.beginRow({.width = kWindow.width, .height = kWindow.height});
            textField(ui, input, "field", state, options);
            (void)root;
        }
        LayoutContext context;
        context.theme = &theme;
        context.measure = &measureFixed;
        layout(arena, ui.root(), kWindow, context);
    }

    /** Where the text starts on screen, so a test can click "at character n". */
    float runLeft() const { return input.frameOf("field.run").x; }

    void pressAt(float x) {
        InputFrame event;
        event.pointer = {x, kWindow.height / 2.0f};
        event.pointerDown = true;
        frame(event);
    }

    void dragTo(float x) {
        InputFrame event;
        event.pointer = {x, kWindow.height / 2.0f};
        event.pointerDown = true;
        frame(event);
    }

    void release(float x) {
        InputFrame event;
        event.pointer = {x, kWindow.height / 2.0f};
        event.pointerDown = false;
        frame(event);
    }
};

}  // namespace

TEST("clicking inside a field puts the caret where the click was") {
    Field field("gitbox");
    field.frame();  // lay out once, so there is a run rectangle to click into
    field.frame();

    const float left = field.runLeft();
    CHECK(left > 0.0f);

    // Just past the middle of the third character lands after it.
    field.pressAt(left + 2.6f * kCharacterWidth);
    CHECK_EQ(field.state.caret, std::size_t{3});
    CHECK(!field.state.hasSelection());

    // The left half of a character lands before it.
    field.release(left + 2.6f * kCharacterWidth);
    field.pressAt(left + 1.2f * kCharacterWidth);
    CHECK_EQ(field.state.caret, std::size_t{1});
}

TEST("a click before the text or past its end clamps to the ends") {
    Field field("gitbox");
    field.frame();
    field.frame();
    const float left = field.runLeft();

    field.pressAt(left - 40.0f);
    CHECK_EQ(field.state.caret, std::size_t{0});

    field.release(left);
    // Past the text but still inside the field, which is where a click on the
    // empty half of a half-filled field lands.
    field.pressAt(left + 150.0f);
    CHECK_EQ(field.state.caret, std::size_t{6});
}

TEST("dragging from the press extends a selection") {
    Field field("gitbox");
    field.frame();
    field.frame();
    const float left = field.runLeft();

    field.pressAt(left + 1.1f * kCharacterWidth);
    CHECK(!field.state.hasSelection());

    field.dragTo(left + 4.2f * kCharacterWidth);
    CHECK_EQ(field.state.anchor, std::size_t{1});
    CHECK_EQ(field.state.caret, std::size_t{4});
    CHECK(field.state.selectedText() == "itb");

    // Dragging back past the anchor selects the other way round.
    field.dragTo(left + 0.1f * kCharacterWidth);
    CHECK_EQ(field.state.caret, std::size_t{0});
    CHECK(field.state.selectedText() == "g");
}

TEST("the run's box names geometry without stealing the click") {
    Field field("gitbox");
    field.frame();
    field.frame();

    // The pointer is over the text, which is a tagged node of its own — and the
    // field is still what was pressed, focused and clicked.
    field.pressAt(field.runLeft() + 2.0f);
    CHECK(field.input.isPressed("field"));
    CHECK(field.input.isFocused("field"));
    CHECK(field.input.frameOf("field.run").width > 0.0f);

    field.release(field.runLeft() + 2.0f);
    CHECK(field.input.clicked("field"));
}

TEST("a read-only field does not move its caret to the pointer") {
    Field field("gitbox");
    field.options.readOnly = true;
    field.frame();
    field.frame();

    field.pressAt(field.runLeft() + 4.0f * kCharacterWidth);
    CHECK_EQ(field.state.caret, std::size_t{0});
}

TEST("the caret and the selection are placed inside the field, not the window") {
    Arena arena;
    Ui ui(arena);
    Theme theme = Theme::dark();
    Interaction input;
    TextEditState state{"hello world", 5, 11};

    // Pushed far from the origin, which is the only place the bug showed: the
    // marks were positioned with the run's *window* coordinates on nodes that
    // `Position::Absolute` measures from their parent, so they landed a pane's
    // width away and the field's clip ate them.
    Rect run{};
    Rect caret{};
    Rect selection{};
    for (int pass = 0; pass < 3; ++pass) {
        arena.reset();
        Ui rebuilt(arena);
        NodeId root;
        {
            auto row = rebuilt.beginRow({.padding = Edges{20.0f, 20.0f, 20.0f, 400.0f}});
            textField(rebuilt, input, "f", state, {.grow = 1.0f});
            root = row.id();
        }
        LayoutContext context;
        context.theme = &theme;
        layout(arena, root, Rect{0, 0, 800, 100}, context);
        input.update(arena, root, InputFrame{});
        input.focus("f");

        run = input.frameOf("f.run");
        caret = {};
        selection = {};
        for (std::size_t i = 0; i < arena.size(); ++i) {
            const Node& node = arena[NodeId{static_cast<std::uint32_t>(i)}];
            if (node.style.position != Position::Absolute) continue;
            const float w = node.style.width.value;
            if (w > 0.9f && w < 3.1f && node.style.radius == 0.0f) caret = node.frame;
            else if (node.style.radius == 2.0f) selection = node.frame;
        }
    }
    (void)ui;

    CHECK(run.width > 0.0f);
    CHECK(caret.width > 0.0f);
    CHECK(caret.x >= run.x);
    CHECK(caret.right() <= run.right() + 1.0f);
    CHECK(selection.x >= run.x);
    CHECK(selection.right() <= run.right() + 1.0f);
}

TEST("a password field offers an eye, and reveals only when told to") {
    const auto build = [&](bool revealed, Interaction& input, Arena& arena) {
        Ui ui(arena);
        Theme theme = Theme::dark();
        TextEditState state{"hunter2", 7, 7};
        TextFieldResult result;
        {
            auto row = ui.beginRow({.align = Align::Center, .width = 300.0f});
            result = textField(ui, input, "pw", state,
                               {.password = true, .revealed = revealed, .grow = 1.0f});
            (void)row;
        }
        LayoutContext context;
        context.theme = &theme;
        layout(arena, ui.root(), Rect{0, 0, 300, 60}, context);
        input.update(arena, ui.root(), InputFrame{});
        return result;
    };

    // Hidden: the run is bullets, one per character, and the secret is not in
    // the tree at all.
    {
        Interaction input;
        Arena arena;
        build(false, input, arena);
        bool sawSecret = false;
        bool sawBullets = false;
        for (std::size_t i = 0; i < arena.size(); ++i) {
            const std::string_view text = arena[NodeId{static_cast<std::uint32_t>(i)}].text;
            if (text == "hunter2") sawSecret = true;
            if (text.find("\xe2\x80\xa2") != std::string_view::npos) sawBullets = true;
        }
        CHECK(!sawSecret);
        CHECK(sawBullets);
        CHECK(!input.frameOf("pw.reveal").empty());  // the eye is there by default
    }

    // Revealed: the same field shows the real string.
    {
        Interaction input;
        Arena arena;
        build(true, input, arena);
        bool sawSecret = false;
        for (std::size_t i = 0; i < arena.size(); ++i) {
            if (arena[NodeId{static_cast<std::uint32_t>(i)}].text == "hunter2") sawSecret = true;
        }
        CHECK(sawSecret);
    }
}

TEST("turning the eye off leaves no reveal target") {
    Arena arena;
    Ui ui(arena);
    Theme theme = Theme::dark();
    Interaction input;
    TextEditState state{"hunter2", 7, 7};
    {
        auto row = ui.beginRow({.align = Align::Center, .width = 300.0f});
        textField(ui, input, "pw", state,
                  {.password = true, .revealToggle = false, .grow = 1.0f});
        (void)row;
    }
    LayoutContext context;
    context.theme = &theme;
    layout(arena, ui.root(), Rect{0, 0, 300, 60}, context);
    input.update(arena, ui.root(), InputFrame{});

    CHECK(input.frameOf("pw.reveal").empty());
}

TEST("the caret lands on whole pixels, whatever the text before it measures") {
    // The bug this pins: a 1.5-pixel bar at a fractional x is antialiased over
    // two columns, and the split changes with every character typed — so the
    // caret appeared to alternate between two widths while typing. The field is
    // given a fractional origin here on purpose.
    Interaction input;
    Theme theme = Theme::dark();
    FontDatabase fonts;
    const char* typed = "gbui rules";

    for (int step = 0; step <= 10; ++step) {
        TextEditState state{std::string(typed, static_cast<std::size_t>(step)), 0, 0};
        state.caret = state.text.size();
        state.anchor = state.caret;

        Rect caret{};
        float width = 0.0f;
        for (int pass = 0; pass < 2; ++pass) {
            Arena arena;
            Ui ui(arena);
            ui.setMeasure(measureWith(fonts), theme.typography());
            NodeId root;
            {
                auto row = ui.beginRow({.padding = Edges{10.0f, 10.0f, 10.0f, 40.5f}});
                textField(ui, input, "snap", state, {.grow = 1.0f});
                root = row.id();
            }
            LayoutContext context;
            context.theme = &theme;
            context.measure = measureWith(fonts);
            layout(arena, root, Rect{0, 0, 400, 80}, context);
            input.update(arena, root, InputFrame{});
            input.focus("snap");
            for (std::size_t i = 0; i < arena.size(); ++i) {
                const Node& node = arena[NodeId{static_cast<std::uint32_t>(i)}];
                if (node.style.position != Position::Absolute) continue;
                if (node.style.radius != 0.0f) continue;
                const float w = node.style.width.value;
                if (w > 0.9f && w < 3.1f) {
                    caret = node.frame;
                    width = w;
                }
            }
        }
        if (caret.width <= 0.0f) continue;  // no font on this machine
        CHECK_NEAR(caret.x, std::round(caret.x));
        CHECK_NEAR(width, std::round(width));
    }
}

/**
 * The bug this pins: an empty focused field showed no caret at all.
 *
 * It was built, and it was even in the arena with a sensible width — which is
 * why an assertion that merely found it passed. An empty run measures zero
 * high, so the box holding it was zero high, so the caret was centred at minus
 * half its own height and the box's own clip removed it. The assertion that
 * catches this is that the caret lies *inside* the box it belongs to.
 */
TEST("an empty focused field still shows its caret") {
    Interaction input;
    Theme theme = Theme::dark();
    FontDatabase fonts;

    for (const char* initial : {"", "a"}) {
        TextEditState state{std::string(initial), 0, 0};
        state.caret = state.text.size();
        state.anchor = state.caret;

        Rect caret{};
        Rect run{};
        for (int pass = 0; pass < 4; ++pass) {
            Arena arena;
            Ui ui(arena);
            ui.setMeasure(measureWith(fonts), theme.typography());
            NodeId root;
            {
                auto row = ui.beginRow({.padding = Edges::all(10.0f)});
                textField(ui, input, "f", state, {.grow = 1.0f});
                root = row.id();
            }
            LayoutContext context;
            context.theme = &theme;
            context.measure = measureWith(fonts);
            layout(arena, root, Rect{0, 0, 300, 60}, context);
            input.update(arena, root, InputFrame{});
            input.focus("f");
            run = input.frameOf("f.run");
            caret = Rect{};
            for (std::size_t i = 0; i < arena.size(); ++i) {
                const Node& node = arena[NodeId{static_cast<std::uint32_t>(i)}];
                if (node.style.position != Position::Absolute) continue;
                const float w = node.style.width.value;
                if (w > 0.9f && w < 3.1f) caret = node.frame;
            }
        }
        if (run.width <= 0.0f) continue;  // no font on this machine

        CHECK(caret.width > 0.0f);
        // The box has a line's height whether or not anything is typed in it.
        CHECK(run.height > 0.0f);
        // And the caret sits inside it, rather than above and clipped away.
        CHECK(caret.y >= run.y - 0.5f);
        CHECK(caret.bottom() <= run.bottom() + 0.5f);
    }
}

/** An empty field is exactly as tall as one with text in it. A box that grows
 *  the moment somebody types would shift everything under it. */
TEST("a field does not change height when text arrives") {
    Interaction input;
    Theme theme = Theme::dark();
    FontDatabase fonts;

    const auto runHeight = [&](const char* initial) {
        TextEditState state{std::string(initial), 0, 0};
        Rect run{};
        for (int pass = 0; pass < 3; ++pass) {
            Arena arena;
            Ui ui(arena);
            ui.setMeasure(measureWith(fonts), theme.typography());
            NodeId root;
            {
                auto row = ui.beginRow({.padding = Edges::all(10.0f)});
                textField(ui, input, "f", state, {.grow = 1.0f});
                root = row.id();
            }
            LayoutContext context;
            context.theme = &theme;
            context.measure = measureWith(fonts);
            layout(arena, root, Rect{0, 0, 300, 60}, context);
            input.update(arena, root, InputFrame{});
            run = input.frameOf("f.run");
        }
        return run.height;
    };

    const float empty = runHeight("");
    const float typed = runHeight("hello");
    if (typed <= 0.0f) return;
    CHECK_NEAR(empty, typed);
}
