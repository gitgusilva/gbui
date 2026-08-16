// The two pieces a form is built from: the multi-line box, and the wrapper that
// gives a control its caption and its message.
#include <optional>
#include <string>

#include "gbui/input/interaction.hpp"
#include "gbui/layout/layout.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/platform/font.hpp"
#include "gbui/style/theme.hpp"
#include "gbui/widgets/elements.hpp"
#include "harness.hpp"

using namespace gbui;

namespace {

constexpr float kCharacterWidth = 10.0f;
constexpr float kLineHeight = 14.0f;
const Rect kWindow{0, 0, 400, 400};

/** The same fixed font `textInputTest` uses: every character ten pixels wide
 *  and every line fourteen tall, so a click at x is expected at character x/10
 *  and a click at y on line y/14, and the assertions read as arithmetic. */
TextMetrics measureFixed(std::string_view text, const TextStyle&, const Typography&, float) {
    std::size_t characters = 0;
    for (char c : text) {
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++characters;
    }
    return {static_cast<float>(characters) * kCharacterWidth, kLineHeight, 11.0f};
}

/** A textarea and a checkbox after it, so Tab has somewhere else to go. Driven
 *  the way the frame loop drives it: build, lay out, then resolve. */
struct Composer {
    Theme theme = Theme::dark();
    Arena arena;
    Interaction input;
    TextareaState state;
    TextareaOptions options{};
    TextEditResult last;

    void frame(const InputFrame& event = {}) {
        // Resolved against the *previous* frame's tree, then built — the order
        // a real loop uses, and the one that lets a keystroke reach the build
        // that happens in the same call.
        input.update(arena, arena.empty() ? NodeId{} : NodeId(0), event);

        arena.reset();
        Ui ui(arena);
        ui.setMeasure(&measureFixed, theme.typography());
        {
            auto root = ui.column({.gap = 6.0f, .padding = Edges::all(10.0f)});
            last = textarea(ui, input, "note", state, options);
            (void)checkbox(ui, input, "after", false, {.label = "something else"});
            (void)root;
        }
        LayoutContext context;
        context.theme = &theme;
        context.measure = &measureFixed;
        layout(arena, ui.root(), kWindow, context);
    }

    void press(Key key, bool shift = false, bool ctrl = false) {
        InputFrame event;
        KeyEvent stroke;
        stroke.key = key;
        stroke.modifiers.shift = shift;
        stroke.modifiers.ctrl = ctrl;
        event.keys.push_back(stroke);
        frame(event);
    }

    void type(std::string_view what) {
        InputFrame event;
        event.text = std::string(what);
        frame(event);
    }

    /**
     * Frames until the geometry stops moving.
     *
     * A scroll view sizes itself from the previous frame's viewport — "the
     * first frame estimates and the second corrects", which is how every
     * floating and scrolling box in this library works — so a box whose content
     * just changed is one frame behind. Three is enough for any of them and the
     * count is not the point of any test here.
     */
    void settle() {
        frame();
        frame();
        frame();
    }

    /** Puts the keyboard on the box, the way Tab would. */
    void focus() {
        press(Key::Tab);
        frame();
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
};

}  // namespace

TEST("a textarea takes the newline that a text field submits on") {
    Composer composer;
    composer.frame();
    composer.focus();
    CHECK(composer.input.isFocused("note"));

    composer.type("first");
    composer.press(Key::Return);
    composer.type("second");

    CHECK_EQ(composer.state.edit.text, std::string("first\nsecond"));
    CHECK(!composer.last.submitted);
}

TEST("the modified Return is what submits, and it leaves the text alone") {
    Composer composer;
    composer.frame();
    composer.focus();
    composer.type("a note");

    const std::string before = composer.state.edit.text;
#if defined(__APPLE__)
    InputFrame event;
    KeyEvent stroke;
    stroke.key = Key::Return;
    stroke.modifiers.meta = true;
    event.keys.push_back(stroke);
    composer.frame(event);
#else
    composer.press(Key::Return, /*shift=*/false, /*ctrl=*/true);
#endif

    CHECK(composer.last.submitted);
    CHECK(!composer.last.changed);
    CHECK_EQ(composer.state.edit.text, before);
}

TEST("clicking the second line puts the caret on the second line") {
    // The hit test has two axes here and only one in a text field: the row
    // comes from y, and the offset within it from x. Getting the first wrong
    // puts every click on line one, which is the bug this is against.
    Composer composer;
    composer.state.edit.text = "alpha\nbravo\ncharlie";
    composer.settle();

    const Rect content = composer.input.frameOf("note.content");
    CHECK(!content.empty());
    // Halfway down the second of three rows, and at its very start.
    composer.clickAt({content.x + 1.0f, content.y + kLineHeight * 1.5f});

    CHECK_EQ(composer.state.edit.caret, std::size_t{6});
    CHECK(!composer.state.edit.hasSelection());
    // And the click put the keyboard on the field, not on the scroll view the
    // press actually landed on.
    CHECK(composer.input.isFocused("note"));
}

TEST("a growing box stops growing at maxRows and scrolls instead") {
    Composer composer;
    composer.options.rows = 2;
    composer.options.maxRows = 4;
    composer.frame();

    const auto heightNow = [&] { return composer.input.frameOf("note").height; };

    composer.settle();
    const float atTwo = heightNow();
    CHECK(atTwo > 0.0f);

    // Four lines: still growing.
    composer.state.edit.text = "one\ntwo\nthree\nfour";
    composer.settle();
    const float atFour = heightNow();
    CHECK(atFour > atTwo);

    // Eight: the box is the same as it was at four, and the rest is behind the
    // scroll rather than making the box taller than the form.
    composer.state.edit.text = "one\ntwo\nthree\nfour\nfive\nsix\nseven\neight";
    composer.settle();
    CHECK_NEAR(heightNow(), atFour);
    CHECK(composer.state.view.scrollable());
}

TEST("typing at the bottom of a full box scrolls it into view") {
    Composer composer;
    composer.options.rows = 2;
    composer.frame();
    composer.focus();

    composer.state.edit.text = "one\ntwo\nthree\nfour";
    composer.state.edit.moveToEnd();
    composer.frame();
    CHECK_NEAR(composer.state.view.offset, 0.0f);

    // One more line, typed at the end: the box has two rows and the caret is on
    // the fifth, so the view has to move or the caret is somewhere nobody can
    // see it.
    composer.press(Key::Return);
    composer.frame();
    CHECK(composer.state.view.offset > 0.0f);
    CHECK(composer.state.view.offset <= composer.state.view.maxOffset() + 0.01f);
}

// ---- the wrapper -----------------------------------------------------------

namespace {

/** A field around a checkbox, so there is a real control to point `forId` at. */
struct Form {
    Theme theme = Theme::dark();
    Arena arena;
    Interaction input;
    FieldOptions options{.label = "Repository", .forId = "repo", .help = "Lowercase, no spaces."};
    FieldResult last;

    void frame(const InputFrame& event = {}) {
        input.update(arena, arena.empty() ? NodeId{} : NodeId(0), event);

        arena.reset();
        Ui ui(arena);
        ui.setMeasure(&measureFixed, theme.typography());
        {
            auto root = ui.column({.padding = Edges::all(10.0f)});
            last = field(ui, input, "f.repo", options,
                         [&](Ui& inner) { (void)checkbox(inner, input, "repo", false); });
            (void)root;
        }
        LayoutContext context;
        context.theme = &theme;
        context.measure = &measureFixed;
        layout(arena, ui.root(), kWindow, context);
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
};

}  // namespace

TEST("clicking a field's caption reports the control it names") {
    Form form;
    form.frame();
    form.frame();

    const Rect caption = form.input.frameOf("f.repo.label");
    CHECK(!caption.empty());
    form.clickAt({caption.x + 4.0f, caption.y + caption.height / 2.0f});

    // Reported rather than acted on: the toolkit does not move focus behind the
    // caller's back, which is the same contract `label` has on its own.
    CHECK(form.last.focus.has_value());
    CHECK_EQ(std::string(form.last.focus.value_or("")), std::string("repo"));
}

TEST("an error replaces the help rather than joining it") {
    Form form;
    form.frame();
    form.frame();
    CHECK(!form.input.frameOf("f.repo.help").empty());
    CHECK(form.input.frameOf("f.repo.error").empty());

    form.options.error = "That name is taken.";
    form.frame();
    form.frame();

    // One message, not two: advice and a complaint in the same place at the
    // same time is two things asking to be read first.
    CHECK(!form.input.frameOf("f.repo.error").empty());
    CHECK(form.input.frameOf("f.repo.help").empty());
}
