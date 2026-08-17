// The small elements: what they draw, what they announce, and what they refuse.
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

const Rect kWindow{0, 0, 400, 300};

TextMetrics measureFixed(std::string_view text, const TextStyle& style, const Typography& type,
                         float) {
    std::size_t characters = 0;
    for (char c : text) {
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++characters;
    }
    const float size = style.size > 0.0f ? style.size : type.uiFontSize;
    return {static_cast<float>(characters) * size * 0.55f, size * 1.35f, size * 0.78f};
}

/** Builds whatever the case hands it, twice, so the second pass has last
 *  frame's geometry the way a real loop does. */
struct Bench {
    Theme theme = Theme::dark();
    Arena arena;
    Interaction input;

    template <typename Build>
    void frame(Build&& build, const InputFrame& event = {}) {
        input.update(arena, arena.empty() ? NodeId{} : NodeId(0), event);
        arena.reset();
        Ui ui(arena);
        ui.setMeasure(&measureFixed, theme.typography());
        NodeId root;
        {
            auto column = ui.column({.gap = 6.0f, .padding = Edges::all(8.0f)});
            build(ui, input);
            root = column.id();
        }
        LayoutContext context;
        context.theme = &theme;
        context.measure = &measureFixed;
        layout(arena, root, kWindow, context);
    }

    /** The accessibility record of the node carrying `tag`, if it has one. */
    const Accessibility* accessibilityOf(std::string_view tag) const {
        for (std::size_t i = 0; i < arena.size(); ++i) {
            const NodeId id{static_cast<std::uint32_t>(i)};
            if (arena[id].id == tag) return arena.accessibility(id);
        }
        return nullptr;
    }

    /** Every accessibility record in the tree, in build order. */
    std::vector<const Accessibility*> records() const {
        std::vector<const Accessibility*> out;
        for (std::size_t i = 0; i < arena.size(); ++i) {
            const NodeId id{static_cast<std::uint32_t>(i)};
            if (const Accessibility* entry = arena.accessibility(id)) out.push_back(entry);
        }
        return out;
    }
};

}  // namespace

// ---- avatar ----------------------------------------------------------------

TEST("initials are the first letter of the first word and of the last") {
    CHECK_EQ(initialsFor("Ada Lovelace"), std::string("AL"));
    CHECK_EQ(initialsFor("grace hopper"), std::string("GH"));
    // One word gives one letter rather than two from the same word, which is
    // what "AD" for "Ada" would be.
    CHECK_EQ(initialsFor("Ada"), std::string("A"));
    // Three words: the first and the *last*, because the middle one is usually
    // the one nobody uses.
    CHECK_EQ(initialsFor("Ada King Lovelace"), std::string("AL"));
    CHECK_EQ(initialsFor("   spaced   out   "), std::string("SO"));
    CHECK_EQ(initialsFor(""), std::string(""));
    CHECK_EQ(initialsFor("   "), std::string(""));
}

TEST("an avatar's colour is a function of the name, not of the order it was drawn") {
    // The property that matters: the same person is the same colour on every
    // screen, in every session, on every machine — with nobody storing one.
    // Two names that differ get different washes; the same name twice does not,
    // wherever in the frame it was drawn.
    // The ids the calls hand back, not a tag: `ui.tag` names the *last node
    // built*, which after an avatar is the text inside it rather than the
    // circle the wash is on.
    NodeId first, again, other;
    Bench third;
    third.frame([&](Ui& ui, const Interaction&) {
        first = avatar(ui, "Ada Lovelace");
        again = avatar(ui, "Ada Lovelace");
        other = avatar(ui, "Grace Hopper");
    });
    const auto fillOf = [&](NodeId id) -> Color {
        return third.arena[id].style.background.color.value_or(Color{});
    };
    const Color a = fillOf(first);
    const Color b = fillOf(again);
    const Color c = fillOf(other);
    CHECK(a.r == b.r && a.g == b.g && a.b == b.b);
    CHECK(!(a.r == c.r && a.g == c.g && a.b == c.b));
}

TEST("an avatar beside its own name says nothing") {
    // The returned ids again: an avatar is a circle with a picture or two
    // letters inside it, so a tag written after the call names the letters.
    NodeId quiet, loud;
    Bench bench;
    bench.frame([&](Ui& ui, const Interaction&) {
        quiet = avatar(ui, "Ada Lovelace", {.decorative = true});
        loud = avatar(ui, "Grace Hopper");
    });
    CHECK(bench.arena.accessibility(quiet)->hidden);
    CHECK(bench.arena.accessibility(loud)->role == Role::Image);
    CHECK_EQ(std::string(bench.arena.accessibility(loud)->name), std::string("Grace Hopper"));
}

// ---- spinner ---------------------------------------------------------------

TEST("a spinner with nothing to say is hidden rather than announced empty") {
    Bench bench;
    bench.frame([](Ui& ui, const Interaction&) {
        spinner(ui);
        ui.tag("quiet");
        spinner(ui, {.name = "Cloning"});
        ui.tag("named");
    });
    CHECK(bench.accessibilityOf("quiet")->hidden);

    const Accessibility* named = bench.accessibilityOf("named");
    // ARIA's indeterminate progress is a progressbar with no value, which is
    // exactly what this is: busy, and unable to say how far along.
    CHECK(named->role == Role::ProgressBar);
    CHECK(named->state.busy == Flag::True);
    CHECK(!named->value.present);
}

TEST("the phase is read modulo one turn, so a clock that never resets is fine") {
    const auto ringAt = [](float phase) {
        Bench bench;
        bench.frame([phase](Ui& ui, const Interaction&) { spinner(ui, {.phase = phase}); });
        // The moving arc is the second of the node's two shapes.
        const Node& node = bench.arena[NodeId{1}];
        return bench.arena.shape(node.firstShape + node.shapeCount - 1).path.bounds();
    };
    const Rect quarter = ringAt(0.25f);
    const Rect same = ringAt(4.25f);
    CHECK_NEAR(quarter.x, same.x);
    CHECK_NEAR(quarter.y, same.y);
}

// ---- chip ------------------------------------------------------------------

TEST("a chip is a toggle button, and says which it is") {
    Bench bench;
    bench.frame([](Ui& ui, const Interaction& input) {
        (void)chip(ui, input, "on", "Merged", {.selected = true});
        (void)chip(ui, input, "off", "Stale");
    });
    CHECK(bench.accessibilityOf("on")->role == Role::Button);
    CHECK(bench.accessibilityOf("on")->state.pressed == Flag::True);
    CHECK(bench.accessibilityOf("off")->state.pressed == Flag::False);
}

TEST("a chip's remove button is named after the chip") {
    // Six chips whose remove buttons are all called "Remove" are six buttons a
    // reader cannot tell apart.
    Bench bench;
    bench.frame([](Ui& ui, const Interaction& input) {
        (void)chip(ui, input, "c", "feat/nord", {.removable = true});
    });
    CHECK_EQ(std::string(bench.accessibilityOf("c.remove")->name), std::string("Remove feat/nord"));
}

TEST("Delete takes a removable chip off, and does not press it on the way") {
    Bench bench;
    ChipResult result;
    const auto build = [&result](Ui& ui, const Interaction& input) {
        result = chip(ui, input, "c", "feat/nord", {.removable = true});
    };
    bench.frame(build);
    bench.input.focus("c", FocusSource::Keyboard);

    InputFrame event;
    event.keys.push_back(KeyEvent{Key::Delete});
    bench.frame(build, event);
    CHECK(result.removed);
    CHECK(!result.pressed);
}

TEST("a disabled chip answers nothing") {
    Bench bench;
    ChipResult result;
    const auto build = [&result](Ui& ui, const Interaction& input) {
        result = chip(ui, input, "c", "Merged", {.removable = true, .disabled = true});
    };
    bench.frame(build);
    bench.input.focus("c", FocusSource::Keyboard);
    InputFrame event;
    event.keys.push_back(KeyEvent{Key::Delete});
    bench.frame(build, event);
    CHECK(!result.removed);
    CHECK(!result.pressed);
}

// ---- kbd -------------------------------------------------------------------

TEST("a shortcut is split into caps, and read out as one thing") {
    Bench bench;
    bench.frame([](Ui& ui, const Interaction&) {
        kbd(ui, "Ctrl+Shift+P");
        ui.tag("shortcut");
    });
    const Accessibility* group = bench.accessibilityOf("shortcut");
    CHECK(group == nullptr);  // the tag lands on the last cap, not the group

    // The group is the first record, and every cap under it is hidden — a
    // reader hears the shortcut once rather than three loose letters.
    const std::vector<const Accessibility*> all = bench.records();
    CHECK(all.front()->role == Role::Group);
    CHECK_EQ(std::string(all.front()->name), std::string("Ctrl+Shift+P"));
    std::size_t hidden = 0;
    for (const Accessibility* entry : all) {
        if (entry->hidden) ++hidden;
    }
    CHECK_EQ(hidden, std::size_t{3});  // three caps
}

TEST("spaces around a key are trimmed, and no separator means one cap") {
    Bench spaced;
    spaced.frame([](Ui& ui, const Interaction&) { kbd(ui, "Ctrl + A"); });
    std::size_t caps = 0;
    for (const Accessibility* entry : spaced.records()) {
        if (entry->hidden) ++caps;
    }
    CHECK_EQ(caps, std::size_t{2});

    Bench whole;
    whole.frame([](Ui& ui, const Interaction&) { kbd(ui, "Ctrl+A", {.separator = ""}); });
    std::size_t one = 0;
    for (const Accessibility* entry : whole.records()) {
        if (entry->hidden) ++one;
    }
    CHECK_EQ(one, std::size_t{1});
}

// ---- skeleton --------------------------------------------------------------

TEST("only the skeleton carrying the message is in the tree") {
    // Six placeholders that each announce themselves are six announcements of
    // nothing.
    Bench bench;
    bench.frame([](Ui& ui, const Interaction&) {
        skeleton(ui, {.name = "Loading commits"});
        ui.tag("first");
        skeleton(ui);
        ui.tag("second");
        skeleton(ui);
        ui.tag("third");
    });
    CHECK(bench.accessibilityOf("first")->role == Role::Status);
    CHECK(bench.accessibilityOf("first")->state.busy == Flag::True);
    CHECK(bench.accessibilityOf("second")->hidden);
    CHECK(bench.accessibilityOf("third")->hidden);
}

TEST("a circle skeleton is as tall as it is wide, whatever the height says") {
    Bench bench;
    bench.frame([](Ui& ui, const Interaction&) {
        skeleton(ui, {.shape = SkeletonShape::Circle, .width = 30.0f, .height = 999.0f});
        ui.tag("dot");
    });
    const Rect box = bench.input.frameOf("dot");
    (void)box;
    // Read off the laid-out frame rather than the style: `Length` is not a
    // float, and what it came out as is the question anyway.
    const Rect drawn = bench.arena[NodeId{1}].frame;
    CHECK_NEAR(drawn.width, 30.0);
    CHECK_NEAR(drawn.height, 30.0);
}

// ---- banner ----------------------------------------------------------------

TEST("a warning interrupts and a note waits") {
    // The whole reason the kind is not merely a colour: `alert` cuts across a
    // screen reader mid-sentence and `status` waits for a pause, and a merge
    // conflict and a confirmation are not the same kind of news.
    Bench bench;
    bench.frame([](Ui& ui, const Interaction& input) {
        (void)banner(ui, input, "warn", "Merge in progress", {.kind = BannerKind::Warning});
        (void)banner(ui, input, "note", "Signed commits are on");
        (void)banner(ui, input, "bad", "Push rejected", {.kind = BannerKind::Danger});
        (void)banner(ui, input, "good", "Pushed", {.kind = BannerKind::Success});
    });
    CHECK(bench.accessibilityOf("warn")->role == Role::Alert);
    CHECK(bench.accessibilityOf("bad")->role == Role::Alert);
    CHECK(bench.accessibilityOf("note")->role == Role::Status);
    CHECK(bench.accessibilityOf("good")->role == Role::Status);
}

TEST("a banner's × is named after it, and reports being pressed") {
    Bench bench;
    BannerResult result;
    const auto build = [&result](Ui& ui, const Interaction& input) {
        result = banner(ui, input, "b", "Signed commits are on", {.closable = true});
    };
    bench.frame(build);
    bench.frame(build);
    CHECK_EQ(std::string(bench.accessibilityOf("b.close")->name),
             std::string("Dismiss Signed commits are on"));

    const Rect cross = bench.input.frameOf("b.close");
    CHECK(cross.width > 0.0f);
    InputFrame down;
    down.pointer = {cross.x + cross.width / 2.0f, cross.y + cross.height / 2.0f};
    down.pointerDown = true;
    bench.frame(build, down);
    InputFrame up;
    up.pointer = down.pointer;
    bench.frame(build, up);
    CHECK(result.dismissed);
}

TEST("a banner with no × and no action is still a message") {
    Bench bench;
    bench.frame(
        [](Ui& ui, const Interaction& input) { (void)banner(ui, input, "b", "Read only"); });
    CHECK(bench.input.frameOf("b.close").empty());
    CHECK(bench.accessibilityOf("b") != nullptr);
}
