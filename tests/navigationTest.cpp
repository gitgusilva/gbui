// Choosing, folding, and finding your way: segmented, accordion, breadcrumbs
// and pagination.
#include <string>
#include <vector>

#include "gbui/input/interaction.hpp"
#include "gbui/layout/layout.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/style/theme.hpp"
#include "gbui/widgets/containers.hpp"
#include "gbui/widgets/elements.hpp"
#include "harness.hpp"

using namespace gbui;

namespace {

const Rect kWindow{0, 0, 600, 400};

TextMetrics measureFixed(std::string_view text, const TextStyle& style, const Typography& type,
                         float) {
    std::size_t characters = 0;
    for (char c : text) {
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++characters;
    }
    const float size = style.size > 0.0f ? style.size : type.uiFontSize;
    return {static_cast<float>(characters) * size * 0.55f, size * 1.35f, size * 0.78f};
}

/** Builds whatever the case hands it, the way the frame loop does. */
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
            auto column = ui.column({.gap = 8.0f, .padding = Edges::all(10.0f)});
            build(ui, input);
            root = column.id();
        }
        LayoutContext context;
        context.theme = &theme;
        context.measure = &measureFixed;
        layout(arena, root, kWindow, context);
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

    template <typename Build>
    void clickAt(Build&& build, Vec2 at) {
        InputFrame down;
        down.pointer = at;
        down.pointerDown = true;
        frame(build, down);
        InputFrame up;
        up.pointer = at;
        frame(build, up);
    }

    template <typename Build>
    void press(Build&& build, Key key) {
        InputFrame event;
        event.keys.push_back(KeyEvent{key});
        frame(build, event);
    }

    Vec2 centreOf(std::string_view tag) const {
        const Rect box = input.frameOf(tag);
        return {box.x + box.width / 2.0f, box.y + box.height / 2.0f};
    }
};

const std::vector<Segment> kLayouts = {
    {.label = "Unified"}, {.label = "Split"}, {.label = "Ribbon", .disabled = true}};

}  // namespace

// ---- segmented -------------------------------------------------------------

TEST("a segmented control is one Tab stop, not one per segment") {
    // Five segments that each took the keyboard would be five Tab presses to
    // cross a single choice, which is why ARIA's radio group is one stop.
    Bench bench;
    std::size_t value = 0;
    const auto build = [&](Ui& ui, const Interaction& input) {
        if (const auto hit = segmented(ui, input, "seg", kLayouts, value, {.name = "Layout"})) {
            value = *hit;
        }
    };
    bench.frame(build);
    CHECK(bench.focusableAt("seg"));
    CHECK(!bench.focusableAt("seg.0"));
    CHECK(!bench.focusableAt("seg.1"));
}

TEST("it is a radio group, and each segment says whether it is the one") {
    Bench bench;
    std::size_t value = 1;
    bench.frame([&](Ui& ui, const Interaction& input) {
        (void)segmented(ui, input, "seg", kLayouts, value, {.name = "Layout"});
    });
    CHECK(bench.accessibilityOf("seg")->role == Role::RadioGroup);
    CHECK_EQ(std::string(bench.accessibilityOf("seg")->name), std::string("Layout"));
    CHECK(bench.accessibilityOf("seg.0")->role == Role::Radio);
    CHECK(bench.accessibilityOf("seg.0")->state.checked == Flag::False);
    CHECK(bench.accessibilityOf("seg.1")->state.checked == Flag::True);
    CHECK_EQ(bench.accessibilityOf("seg.1")->positionInSet, std::size_t{2});
    CHECK_EQ(bench.accessibilityOf("seg.1")->setSize, std::size_t{3});
}

TEST("the arrows move and choose in one press, and skip a disabled segment") {
    // A radio group whose arrows only moved a highlight would need a second
    // press to commit, which is not how any platform's radio group behaves.
    Bench bench;
    std::size_t value = 0;
    const auto build = [&](Ui& ui, const Interaction& input) {
        if (const auto hit = segmented(ui, input, "seg", kLayouts, value, {.name = "Layout"})) {
            value = *hit;
        }
    };
    bench.frame(build);
    bench.input.focus("seg", FocusSource::Keyboard);

    bench.press(build, Key::Right);
    CHECK_EQ(value, std::size_t{1});
    // Index 2 is disabled, so Right wraps past it to 0 rather than landing on
    // something that cannot be chosen.
    bench.press(build, Key::Right);
    CHECK_EQ(value, std::size_t{0});
    bench.press(build, Key::Left);
    CHECK_EQ(value, std::size_t{1});
}

TEST("a press on a disabled segment does nothing") {
    Bench bench;
    std::size_t value = 0;
    const auto build = [&](Ui& ui, const Interaction& input) {
        if (const auto hit = segmented(ui, input, "seg", kLayouts, value, {})) value = *hit;
    };
    bench.frame(build);
    bench.frame(build);
    bench.clickAt(build, bench.centreOf("seg.2"));
    CHECK_EQ(value, std::size_t{0});
}

// ---- accordion -------------------------------------------------------------

namespace {

std::vector<AccordionSection> sections(bool* built) {
    return {
        {.id = "general", .title = "General"},
        {.id = "git",
         .title = "Git",
         .body = [built](Ui& ui) {
             if (built) *built = true;
             text(ui, "inside");
         }},
        {.id = "advanced", .title = "Advanced", .disabled = true},
    };
}

}  // namespace

TEST("a closed section's body is never built") {
    // A closed section should cost nothing rather than be built and hidden,
    // which is the whole reason the body is a callback.
    bool built = false;
    AccordionState state;
    Bench bench;
    const auto list = sections(&built);
    bench.frame([&](Ui& ui, const Interaction& input) {
        (void)accordion(ui, input, "acc", list, state);
    });
    CHECK(!built);

    state.open.emplace("git");
    bench.frame([&](Ui& ui, const Interaction& input) {
        (void)accordion(ui, input, "acc", list, state);
    });
    CHECK(built);
}

TEST("every header says whether it is open before anybody presses it") {
    AccordionState state;
    state.open.emplace("git");
    Bench bench;
    const auto list = sections(nullptr);
    bench.frame([&](Ui& ui, const Interaction& input) {
        (void)accordion(ui, input, "acc", list, state);
    });
    CHECK(bench.accessibilityOf("acc.general")->role == Role::Button);
    CHECK(bench.accessibilityOf("acc.general")->state.expanded == Flag::False);
    CHECK(bench.accessibilityOf("acc.git")->state.expanded == Flag::True);
    CHECK(bench.accessibilityOf("acc.advanced")->state.disabled == Flag::True);
}

TEST("pressing a header opens it, and several may be open at once") {
    // The default, and the less obvious choice: an accordion whose sections
    // close each other cannot be used to compare two of them.
    AccordionState state;
    Bench bench;
    const auto list = sections(nullptr);
    const auto build = [&](Ui& ui, const Interaction& input) {
        (void)accordion(ui, input, "acc", list, state);
    };
    bench.frame(build);
    bench.frame(build);

    bench.clickAt(build, bench.centreOf("acc.general"));
    CHECK(state.isOpen("general"));
    bench.frame(build);
    bench.clickAt(build, bench.centreOf("acc.git"));
    CHECK(state.isOpen("general"));
    CHECK(state.isOpen("git"));
}

TEST("exclusive closes the others, and a second press closes the one") {
    AccordionState state;
    Bench bench;
    const auto list = sections(nullptr);
    const auto build = [&](Ui& ui, const Interaction& input) {
        (void)accordion(ui, input, "acc", list, state, {.exclusive = true});
    };
    bench.frame(build);
    bench.frame(build);

    bench.clickAt(build, bench.centreOf("acc.general"));
    CHECK(state.isOpen("general"));
    bench.frame(build);
    bench.clickAt(build, bench.centreOf("acc.git"));
    CHECK(!state.isOpen("general"));
    CHECK(state.isOpen("git"));
    // The same header again shuts it: an exclusive accordion where nothing can
    // be closed is one where the reader cannot get the room back.
    bench.frame(build);
    bench.clickAt(build, bench.centreOf("acc.git"));
    CHECK(!state.isOpen("git"));
}

TEST("the arrows walk the headers and stop at the ends") {
    AccordionState state;
    state.focused = "general";
    AccordionResult last;
    Bench bench;
    const auto list = sections(nullptr);
    const auto build = [&](Ui& ui, const Interaction& input) {
        last = accordion(ui, input, "acc", list, state);
    };
    bench.frame(build);
    bench.input.focus("acc.general", FocusSource::Keyboard);

    bench.press(build, Key::Down);
    CHECK_EQ(state.focused, std::string("git"));
    // `advanced` is disabled, so Down stops rather than landing on it — and a
    // stack of sections does not wrap, unlike a radio group: the reader can see
    // it has a bottom.
    bench.input.focus("acc.git", FocusSource::Keyboard);
    bench.press(build, Key::Down);
    CHECK_EQ(state.focused, std::string("git"));
}

// ---- breadcrumbs -----------------------------------------------------------

namespace {

const std::vector<Crumb> kTrail = {{.label = "gbui"},
                                   {.label = "src"},
                                   {.label = "widgets"},
                                   {.label = "layout"},
                                   {.label = "treeView.cpp"}};

}  // namespace

TEST("the last crumb is where you are, not a link") {
    // A link to the page you are on is a control that does nothing, and a reader
    // who tabs through five crumbs to find that out has been misled five times.
    Bench bench;
    bench.frame([](Ui& ui, const Interaction& input) {
        (void)breadcrumbs(ui, input, "crumbs", kTrail);
    });
    CHECK(bench.focusableAt("crumbs.0"));
    CHECK(bench.accessibilityOf("crumbs.0")->role == Role::Link);

    CHECK(!bench.focusableAt("crumbs.4"));
    CHECK(bench.accessibilityOf("crumbs.4")->state.current == Flag::True);
    // `current`, not `selected`: the reader is being told where they are, not
    // that they picked it out of a set they could pick differently from.
    CHECK(bench.accessibilityOf("crumbs.4")->state.selected == Flag::Unset);
}

TEST("the trail says it is a trail") {
    Bench bench;
    bench.frame([](Ui& ui, const Interaction& input) {
        (void)breadcrumbs(ui, input, "crumbs", kTrail);
    });
    CHECK(bench.accessibilityOf("crumbs")->role == Role::Group);
    CHECK_EQ(std::string(bench.accessibilityOf("crumbs")->name), std::string("Breadcrumb"));
}

TEST("the middle collapses and the ends stay") {
    // The ends are the two a reader needs: where they are, and the way home.
    Bench bench;
    bench.frame([](Ui& ui, const Interaction& input) {
        (void)breadcrumbs(ui, input, "crumbs", kTrail, {.maxVisible = 3});
    });
    bench.frame([](Ui& ui, const Interaction& input) {
        (void)breadcrumbs(ui, input, "crumbs", kTrail, {.maxVisible = 3});
    });
    CHECK(!bench.input.frameOf("crumbs.0").empty());          // the root
    CHECK(!bench.input.frameOf("crumbs.4").empty());          // where you are
    CHECK(bench.input.frameOf("crumbs.2").empty());           // and the middle went
    CHECK(!bench.input.frameOf("crumbs.more").empty());
    // The ellipsis says how many it stands for, because "…" on its own is a
    // button whose whole meaning is the number.
    const std::string name = std::string(bench.accessibilityOf("crumbs.more")->name);
    CHECK(name.find("3") != std::string::npos);
}

TEST("pressing a crumb reports it, and the last one cannot be pressed") {
    Bench bench;
    BreadcrumbsResult last;
    const auto build = [&](Ui& ui, const Interaction& input) {
        last = breadcrumbs(ui, input, "crumbs", kTrail);
    };
    bench.frame(build);
    bench.frame(build);

    bench.clickAt(build, bench.centreOf("crumbs.1"));
    CHECK(last.chosen.has_value());
    CHECK_EQ(last.chosen.value_or(99), std::size_t{1});

    bench.frame(build);
    bench.clickAt(build, bench.centreOf("crumbs.4"));
    CHECK(!last.chosen.has_value());
}

// ---- pagination ------------------------------------------------------------

TEST("the window keeps the first and last page whatever the current one is") {
    // "Jump to the end" is the second most common thing anybody does with a
    // paginator, and hiding it makes them press next forty times.
    Bench bench;
    bench.frame([](Ui& ui, const Interaction& input) {
        (void)pagination(ui, input, "pages", 10, 40);
    });
    bench.frame([](Ui& ui, const Interaction& input) {
        (void)pagination(ui, input, "pages", 10, 40);
    });
    CHECK(!bench.input.frameOf("pages.0").empty());
    CHECK(!bench.input.frameOf("pages.39").empty());
    CHECK(!bench.input.frameOf("pages.9").empty());
    CHECK(!bench.input.frameOf("pages.11").empty());
    CHECK(bench.input.frameOf("pages.20").empty());
}

TEST("the page you are on is current, and takes no press") {
    Bench bench;
    bench.frame([](Ui& ui, const Interaction& input) {
        (void)pagination(ui, input, "pages", 3, 20);
    });
    CHECK(bench.accessibilityOf("pages.3")->state.current == Flag::True);
    CHECK(!bench.focusableAt("pages.3"));
    CHECK(bench.accessibilityOf("pages.4")->role == Role::Button);
    CHECK(bench.focusableAt("pages.4"));
}

TEST("the arrows are disabled at the ends rather than missing") {
    Bench bench;
    bench.frame([](Ui& ui, const Interaction& input) {
        (void)pagination(ui, input, "pages", 0, 5);
    });
    CHECK(bench.accessibilityOf("pages.previous")->state.disabled == Flag::True);
    CHECK(bench.accessibilityOf("pages.next")->state.disabled == Flag::False);

    Bench end;
    end.frame([](Ui& ui, const Interaction& input) {
        (void)pagination(ui, input, "pages", 4, 5);
    });
    CHECK(end.accessibilityOf("pages.next")->state.disabled == Flag::True);
}

TEST("the keyboard steps pages from anywhere in the control") {
    Bench bench;
    std::size_t page = 3;
    const auto build = [&](Ui& ui, const Interaction& input) {
        if (const auto hit = pagination(ui, input, "pages", page, 20).chosen) page = *hit;
    };
    bench.frame(build);
    bench.frame(build);
    bench.input.focus("pages.next", FocusSource::Keyboard);

    bench.press(build, Key::Right);
    CHECK_EQ(page, std::size_t{4});
    bench.press(build, Key::Left);
    CHECK_EQ(page, std::size_t{3});
    bench.press(build, Key::End);
    CHECK_EQ(page, std::size_t{19});
    bench.press(build, Key::Home);
    CHECK_EQ(page, std::size_t{0});
}

TEST("a gap of one page is drawn as the page") {
    // An ellipsis standing in for a single button is strictly worse than the
    // button: the reader can see there is something there and cannot reach it.
    Bench bench;
    bench.frame([](Ui& ui, const Interaction& input) {
        (void)pagination(ui, input, "pages", 0, 4, {.around = 1});
    });
    bench.frame([](Ui& ui, const Interaction& input) {
        (void)pagination(ui, input, "pages", 0, 4, {.around = 1});
    });
    // 1 2 | 3 | 4 — page 3 is the only one outside the window, so it is drawn
    // rather than hidden behind an ellipsis for itself.
    CHECK(!bench.input.frameOf("pages.2").empty());
}
