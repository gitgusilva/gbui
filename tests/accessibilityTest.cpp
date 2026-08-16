// What every control says about itself, asserted rather than assumed.
//
// The gate behind rule 7 in CONTRIBUTING: a component that grows a role but no
// name, or a state that never updates, is a control that *claims* to be
// reachable. The last case in this file is the one that catches the whole
// class — it walks a form and fails on any Tab stop with nothing to announce —
// and the ones above it pin the individual decisions that are easy to get
// subtly wrong.
#include <string>
#include <string_view>
#include <vector>

#include "gbui/a11y/accessibility.hpp"
#include "gbui/input/interaction.hpp"
#include "gbui/layout/layout.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/style/theme.hpp"
#include "gbui/widgets/components.hpp"
#include "gbui/widgets/containers.hpp"
#include "gbui/widgets/elements.hpp"
#include "gbui/widgets/overlays.hpp"
#include "harness.hpp"

using namespace gbui;

namespace {

const Rect kWindow{0, 0, 600, 400};

/** Every character ten pixels wide, so nothing here depends on a font being
 *  installed. */
TextMetrics measureFixed(std::string_view text, const TextStyle&, const Typography&, float) {
    std::size_t characters = 0;
    for (char c : text) {
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++characters;
    }
    return {static_cast<float>(characters) * 10.0f, 14.0f, 11.0f};
}

/** Builds a tree, lays it out and resolves the input against it — the frame
 *  loop, with somewhere to ask questions afterwards. */
struct Screen {
    Theme theme = Theme::dark();
    Arena arena;
    Interaction input;

    template <typename Fn>
    void frame(Fn&& build) {
        arena.reset();
        Ui ui(arena);
        ui.setMeasure(&measureFixed, theme.typography());
        {
            auto root = ui.column({.gap = 8.0f, .width = kWindow.width});
            build(ui);
            (void)root;
        }
        LayoutContext context;
        context.theme = &theme;
        context.measure = &measureFixed;
        layout(arena, ui.root(), kWindow, context);
        input.update(arena, ui.root(), InputFrame{});
    }

    /** What the node with this tag says about itself, or null. */
    const Accessibility* of(std::string_view tag) const {
        for (std::size_t i = 0; i < arena.size(); ++i) {
            const NodeId id{static_cast<std::uint32_t>(i)};
            if (arena[id].id == tag) return arena.accessibility(id);
        }
        return nullptr;
    }
};

}  // namespace

TEST("a checkbox says what it is, what it is called and whether it is checked") {
    Screen screen;
    screen.frame([](Ui& ui) {
        Interaction none;
        (void)checkbox(ui, none, "tags", true, {.label = "Show tags"});
    });

    const Accessibility* info = screen.of("tags");
    CHECK(info != nullptr);
    if (!info) return;
    CHECK(info->role == Role::Checkbox);
    CHECK(info->name == "Show tags");
    CHECK(info->state.checked == Flag::True);
    // The states it does not have stay unsaid, which is the whole reason `Flag`
    // has four values instead of two.
    CHECK(info->state.expanded == Flag::Unset);
    CHECK(info->state.selected == Flag::Unset);
}

TEST("a switch is a switch and not a checkbox") {
    // The distinction the header argues for — "on now" against "will apply" —
    // is one a reader only gets from the role.
    Screen screen;
    screen.frame([](Ui& ui) {
        Interaction none;
        (void)toggle(ui, none, "fetch", false, {.label = "Auto fetch"});
    });

    const Accessibility* info = screen.of("fetch");
    CHECK(info != nullptr);
    if (!info) return;
    CHECK(info->role == Role::Switch);
    CHECK(info->state.checked == Flag::False);
}

/** The bug this pins: a button was never a Tab stop, and had not been since
 *  focus was built. Space and Return are handed to whatever has the keyboard,
 *  and nothing could ever give it to a button. */
TEST("a button is a Tab stop, and announces its label") {
    Screen screen;
    screen.frame([](Ui& ui) {
        Interaction none;
        button(ui, none, "Commit", {.id = "commit"});
        button(ui, none, "Discard", {.disabled = true, .id = "discard"});
    });

    bool commitFocusable = false;
    bool discardFocusable = false;
    for (std::size_t i = 0; i < screen.arena.size(); ++i) {
        const Node& node = screen.arena[NodeId{static_cast<std::uint32_t>(i)}];
        if (node.id == "commit") commitFocusable = node.focusable;
        if (node.id == "discard") discardFocusable = node.focusable;
    }
    CHECK(commitFocusable);
    // …and a disabled one is not, which is the other half of the same rule.
    CHECK(!discardFocusable);

    const Accessibility* info = screen.of("commit");
    CHECK(info != nullptr);
    if (!info) return;
    CHECK(info->role == Role::Button);
    CHECK(info->name == "Commit");
}

TEST("an icon-only button has no label to borrow and takes the caller's name") {
    Screen screen;
    screen.frame([](Ui& ui) {
        Interaction none;
        button(ui, none, "", {.leading = Icon::Archive, .id = "delete", .name = "Archive branch"});
        button(ui, none, "", {.leading = Icon::Plus, .id = "add"});
    });

    const Accessibility* named = screen.of("delete");
    CHECK(named != nullptr);
    if (named) CHECK(named->name == "Archive branch");

    // Nothing is invented from the glyph: a name guessed out of `Icon::Plus` is
    // a guess the reader cannot check. The gap stays visible instead.
    const Accessibility* unnamed = screen.of("add");
    CHECK(unnamed != nullptr);
    if (unnamed) CHECK(unnamed->name.empty());
}

TEST("a slider announces the words it was given, not only the number") {
    Screen screen;
    screen.frame([](Ui& ui) {
        Interaction none;
        (void)slider(ui, none, "volume", 0.7,
                     {.minimum = 0.0, .maximum = 1.0, .name = "Volume",
                      .valueText = "70 percent"});
    });

    const Accessibility* info = screen.of("volume");
    CHECK(info != nullptr);
    if (!info) return;
    CHECK(info->role == Role::Slider);
    CHECK(info->name == "Volume");
    CHECK(info->value.present);
    CHECK_NEAR(info->value.now, 0.7);
    CHECK_NEAR(info->value.maximum, 1.0);
    CHECK(info->value.text == "70 percent");
}

TEST("a number box is a spin button with bounds, and a text box has none") {
    Screen screen;
    TextEditState number{"42", 2, 2};
    TextEditState text{"gitbox", 6, 6};
    screen.frame([&](Ui& ui) {
        Interaction none;
        (void)textInput(ui, none, "minutes", number,
                        {.type = InputType::Number, .minimum = 0.0, .maximum = 60.0});
        (void)textInput(ui, none, "repo", text, {.placeholder = "Repository name"});
    });

    const Accessibility* spin = screen.of("minutes");
    CHECK(spin != nullptr);
    if (spin) {
        CHECK(spin->role == Role::SpinButton);
        CHECK(spin->value.present);
        CHECK_NEAR(spin->value.now, 42.0);
        CHECK_NEAR(spin->value.maximum, 60.0);
    }

    const Accessibility* box = screen.of("repo");
    CHECK(box != nullptr);
    if (box) {
        CHECK(box->role == Role::TextInput);
        CHECK(box->value.present);
        CHECK(box->value.text == "gitbox");
        // No range: `minimum == maximum` is how a text box says so.
        CHECK_NEAR(box->value.minimum, box->value.maximum);
        // The placeholder describes, never names — a box named by its
        // placeholder loses its name the moment somebody types in it.
        CHECK(box->name.empty());
        CHECK(box->description == "Repository name");
    }
}

TEST("a password box reports no value at all") {
    // The bullets exist so the string is not on offer. A tree that carried it
    // would hand back exactly what the screen refuses to show.
    Screen screen;
    TextEditState secret{"hunter2", 7, 7};
    screen.frame([&](Ui& ui) {
        Interaction none;
        (void)textInput(ui, none, "pw", secret, {.type = InputType::Password});
    });

    const Accessibility* info = screen.of("pw");
    CHECK(info != nullptr);
    if (!info) return;
    CHECK(info->role == Role::TextInput);
    CHECK(!info->value.present);
}

TEST("an invalid box says so, and a read-only one says something else") {
    Screen screen;
    TextEditState wrong{"my repo", 7, 7};
    TextEditState fixed{"origin/main", 0, 0};
    screen.frame([&](Ui& ui) {
        Interaction none;
        (void)textInput(ui, none, "bad", wrong, {.invalid = true});
        (void)textInput(ui, none, "ro", fixed, {.readOnly = true});
    });

    const Accessibility* bad = screen.of("bad");
    if (bad) {
        CHECK(bad->state.invalid == Flag::True);
        CHECK(bad->state.readOnly == Flag::False);
    }
    const Accessibility* readOnly = screen.of("ro");
    if (readOnly) CHECK(readOnly->state.readOnly == Flag::True);
}

TEST("a caption names its control and a message describes it, both from the end that knows") {
    Screen screen;
    TextEditState text{"my repo", 7, 7};
    screen.frame([&](Ui& ui) {
        Interaction none;
        field(ui, none, "f",
              {.label = "Repository",
               .forId = "name",
               .help = "Lowercase, no spaces.",
               .error = "A repository name cannot contain spaces.",
               .required = true},
              [&](Ui& inner) { (void)textInput(inner, none, "name", text, {.invalid = true}); });
    });

    // `<label for>` pointed this way round: the caption is built before the
    // control and says which one it names.
    const Accessibility* caption = screen.of("f.label");
    CHECK(caption != nullptr);
    if (caption) {
        CHECK(caption->role == Role::Label);
        CHECK(caption->name == "Repository");
        CHECK(caption->relations.labels == "name");
        CHECK(caption->state.required == Flag::True);
    }

    // The error replaces the help, so there is exactly one message and it is
    // the complaint.
    CHECK(screen.of("f.help") == nullptr);
    const Accessibility* error = screen.of("f.error");
    CHECK(error != nullptr);
    if (error) {
        CHECK(error->role == Role::Alert);
        CHECK(error->relations.describes == "name");
    }
}

TEST("a select reports which row the arrows are on while focus stays on the box") {
    Screen screen;
    const std::vector<std::string> items{"main", "develop", "release"};
    SelectState state;
    state.open = true;
    state.highlighted = 1;

    screen.frame([&](Ui& ui) {
        Interaction none;
        (void)select(ui, none, "branch", items, std::size_t{0}, state);
    });

    const Accessibility* box = screen.of("branch");
    CHECK(box != nullptr);
    if (box) {
        CHECK(box->role == Role::ComboBox);
        CHECK(box->state.expanded == Flag::True);
        CHECK(box->value.text == "main");
        CHECK(box->relations.controls == "branch.list");
        // The one thing that makes an open list usable: focus never leaves the
        // box, so this is the only way to say where the keys are.
        CHECK(box->relations.activeDescendant == "branch.list.1");
    }

    const Accessibility* row = screen.of("branch.list.1");
    CHECK(row != nullptr);
    if (row) {
        CHECK(row->role == Role::Option);
        CHECK(row->name == "develop");
    }
}

TEST("a virtualised row knows it is three of fifty thousand") {
    // The count belongs to the data, and the tree only ever holds the rows on
    // screen. Without `setSize` a reader is told the size of the window.
    Screen screen;
    ScrollState scroll;
    screen.frame([&](Ui& ui) {
        Interaction none;
        (void)virtualList(ui, none, "commits", scroll,
                          {.count = 50000, .rowHeight = 24.0f, .height = 200.0f,
                           .name = "Commits"},
                          [](Ui& row, std::size_t index) {
                              row.label(std::to_string(index));
                          });
    });

    std::size_t items = 0;
    std::size_t biggest = 0;
    for (std::size_t i = 0; i < screen.arena.size(); ++i) {
        const NodeId id{static_cast<std::uint32_t>(i)};
        const Accessibility* info = screen.arena.accessibility(id);
        if (!info || info->role != Role::ListItem) continue;
        ++items;
        CHECK_EQ(info->setSize, std::size_t{50000});
        biggest = std::max(biggest, info->positionInSet);
    }
    CHECK(items > 0);
    // Only the visible slice exists, which is the point of the widget and the
    // reason the count has to be carried rather than counted.
    CHECK(items < 100);
    CHECK(biggest <= 50000);
}

TEST("a marquee's second pass is hidden from the tree") {
    Screen screen;
    MarqueeState state;
    screen.frame([&](Ui& ui) {
        Interaction none;
        (void)marquee(ui, none, "ticker", state, 0.0f,
                      [](Ui& inner) { inner.label("GBP/USD 1.2712"); },
                      {.name = "Now playing"});
    });

    std::size_t hidden = 0;
    for (std::size_t i = 0; i < screen.arena.size(); ++i) {
        const Accessibility* info =
            screen.arena.accessibility(NodeId{static_cast<std::uint32_t>(i)});
        if (info && info->hidden) ++hidden;
    }
    // Exactly one: the copy drawn to hide the seam. A reader given both would
    // be read the same sentence twice with nothing to say why.
    CHECK_EQ(hidden, std::size_t{1});
}

TEST("a sortable column that is not the sorted one says so, and an unsortable one says nothing") {
    Screen screen;
    TableState state;
    state.sortColumn = 0;
    state.ascending = false;
    const std::vector<Column> columns{
        {.title = "Author", .sortable = true},
        {.title = "Commits", .sortable = true},
        {.title = "Last seen"},
    };
    screen.frame([&](Ui& ui) {
        Interaction none;
        (void)table(ui, none, "stats", columns, 3, state,
                    [](Ui& cell, std::size_t, std::size_t) { cell.label("x"); },
                    {.virtualise = false, .name = "Commits by author"});
    });

    const Accessibility* sorted = screen.of("stats.head.0");
    if (sorted) {
        CHECK(sorted->role == Role::ColumnHeader);
        CHECK(sorted->state.sorted == Sort::Descending);
    }
    const Accessibility* sortable = screen.of("stats.head.1");
    if (sortable) CHECK(sortable->state.sorted == Sort::None);
    const Accessibility* fixed = screen.of("stats.head.2");
    if (fixed) CHECK(fixed->state.sorted == Sort::Unset);

    const Accessibility* whole = screen.of("stats");
    if (whole) {
        CHECK(whole->role == Role::Table);
        CHECK(whole->name == "Commits by author");
    }
}

TEST("two calls merge rather than replace") {
    // What lets a component set its role and a wrapper add the relation that
    // names it. Assigning the whole record instead would make the second call
    // silently erase the first.
    Arena arena;
    Ui ui(arena);
    ui.add({});
    ui.tag("thing").accessible({.role = Role::Button, .name = "Commit"});
    ui.accessible({.state = {.disabled = Flag::True}});
    ui.accessible({.relations = {.describedBy = "hint"}});

    const Accessibility* info = arena.accessibility(ui.last());
    CHECK(info != nullptr);
    if (!info) return;
    CHECK(info->role == Role::Button);
    CHECK(info->name == "Commit");
    CHECK(info->state.disabled == Flag::True);
    CHECK(info->relations.describedBy == "hint");
}

TEST("a name is interned, so a temporary is safe to pass") {
    // Half the relations in the library are spelled `std::string(id) + ".error"`
    // at the call site. A view of one of those outlives nothing.
    Arena arena;
    Ui ui(arena);
    ui.add({});
    {
        const std::string temporary = "Archive branch";
        ui.accessible({.role = Role::Button, .name = temporary});
    }
    const Accessibility* info = arena.accessibility(ui.last());
    CHECK(info != nullptr);
    if (info) CHECK(info->name == "Archive branch");
}

TEST("nodes with nothing to say cost nothing") {
    // The reason the record is a side table and not a field on `Node`: a row
    // that spaces two things out is not a thing, and most nodes are that.
    Arena arena;
    Ui ui(arena);
    {
        auto row = ui.row({.gap = 4.0f});
        ui.label("one");
        ui.label("two");
        (void)row;
    }
    CHECK_EQ(arena.accessibleCount(), std::size_t{0});
    CHECK(arena.accessibility(ui.root()) == nullptr);
}

TEST("every role has a name of its own") {
    const Role roles[] = {
        Role::None,      Role::Label,      Role::Heading,     Role::Paragraph,
        Role::Image,     Role::Link,       Role::Figure,      Role::Button,
        Role::Checkbox,  Role::Radio,      Role::RadioGroup,  Role::Switch,
        Role::Slider,    Role::SpinButton, Role::TextInput,   Role::ComboBox,
        Role::ListBox,   Role::Option,     Role::ProgressBar, Role::Group,
        Role::Form,      Role::Toolbar,    Role::Separator,   Role::ScrollView,
        Role::List,      Role::ListItem,   Role::Table,       Role::Row,
        Role::Cell,      Role::ColumnHeader, Role::Tree,      Role::TreeItem,
        Role::TabList,   Role::Tab,        Role::TabPanel,    Role::Menu,
        Role::MenuBar,   Role::MenuItem,   Role::MenuItemCheckbox, Role::MenuItemRadio,
        Role::Dialog,    Role::AlertDialog, Role::Tooltip,    Role::Status,
        Role::Alert,
    };
    // A role added without a name falls through to "none" in `roleName`, which
    // is exactly the silent answer a generated bridge would ship.
    for (const Role role : roles) {
        if (role == Role::None) continue;
        CHECK(roleName(role) != "none");
    }
    for (const Role a : roles) {
        int matches = 0;
        for (const Role b : roles) {
            if (roleName(a) == roleName(b)) ++matches;
        }
        CHECK_EQ(matches, 1);
    }
}

/**
 * The gate the whole file exists for.
 *
 * Every node the keyboard can land on has to say what it is, and — unless
 * something else names it — what it is called. This is the check that catches
 * the next component to be added without one, which is a thing that happens by
 * omission rather than by decision.
 */
TEST("every Tab stop in a form has a role and a name") {
    Screen screen;
    TextEditState name{"gitbox", 6, 6};
    TextEditState note{};
    TextareaState message{};
    const std::vector<std::string> branches{"main", "develop"};
    SelectState branchList;
    ScrollState pane;

    screen.frame([&](Ui& ui) {
        Interaction none;
        field(ui, none, "f.name", {.label = "Repository", .forId = "name"},
              [&](Ui& inner) { (void)textInput(inner, none, "name", note); });
        (void)textInput(ui, none, "plain", name, {.name = "Branch", .placeholder = "anything"});
        (void)textarea(ui, none, "message", message,
                       {.name = "Message", .placeholder = "Describe the change"});
        (void)checkbox(ui, none, "tags", true, {.label = "Show tags"});
        (void)radio(ui, none, "merge", true, {.label = "Merge"});
        (void)toggle(ui, none, "fetch", false, {.label = "Auto fetch"});
        (void)slider(ui, none, "contrast", 0.5, {.name = "Contrast"});
        (void)select(ui, none, "branch", branches, std::size_t{0}, branchList,
                     {.name = "Base branch"});
        button(ui, none, "Commit", {.id = "commit"});
        (void)hyperlink(ui, none, "docs", "Open the guide", {.href = "https://example.test"});
        {
            auto view = scrollArea(ui, none, "pane", pane, {.focusable = true, .name = "Diff"});
            ui.label("a line");
            (void)view;
        }
    });

    // Which controls are named by something else rather than by themselves. A
    // `labels` relation pointed at them is a name; the tree resolves it.
    std::vector<std::string_view> namedElsewhere;
    for (std::size_t i = 0; i < screen.arena.size(); ++i) {
        const Accessibility* info =
            screen.arena.accessibility(NodeId{static_cast<std::uint32_t>(i)});
        if (info && !info->relations.labels.empty()) namedElsewhere.push_back(info->relations.labels);
    }

    std::size_t stops = 0;
    for (std::size_t i = 0; i < screen.arena.size(); ++i) {
        const NodeId id{static_cast<std::uint32_t>(i)};
        const Node& node = screen.arena[id];
        if (!node.focusable || node.id.empty()) continue;
        ++stops;

        const Accessibility* info = screen.arena.accessibility(id);
        if (!info) {
            ::gbui::test::reportFailure(__FILE__, __LINE__, "a Tab stop with no role",
                                        std::string(node.id));
            continue;
        }
        if (info->role == Role::None) {
            ::gbui::test::reportFailure(__FILE__, __LINE__, "a Tab stop whose role is None",
                                        std::string(node.id));
        }

        bool named = !info->name.empty() || !info->relations.labelledBy.empty();
        for (const std::string_view target : namedElsewhere) {
            named = named || target == node.id;
        }
        if (!named) {
            ::gbui::test::reportFailure(__FILE__, __LINE__, "a Tab stop with nothing to announce",
                                        std::string(node.id));
        }
    }
    // A form of eleven controls: if this collapses, the loop above stopped
    // checking anything and would pass in silence.
    CHECK(stops >= 11);
}
