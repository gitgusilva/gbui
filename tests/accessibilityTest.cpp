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
#include "gbui/a11y/tree.hpp"
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

// ---- the tree ---------------------------------------------------------------
//
// Stage 4: the records on the arena pruned into one node per thing a reader can
// perceive, the relations resolved onto the controls they belong to, and the
// whole thing diffed so a screen reader is told what changed rather than
// everything, sixty times a second.

namespace {

/** Builds the tree for one frame of `build`. */
struct Reader {
    Theme theme = Theme::dark();
    Arena arena;
    Interaction input;
    AccessibilityTree tree;

    template <typename Fn>
    void frame(Fn&& build) {
        arena.reset();
        Ui ui(arena);
        ui.setMeasure(&measureFixed, theme.typography());
        NodeId root;
        {
            auto column = ui.column({.gap = 8.0f, .width = kWindow.width});
            build(ui);
            root = column.id();
        }
        LayoutContext context;
        context.theme = &theme;
        context.measure = &measureFixed;
        layout(arena, root, kWindow, context);
        input.update(arena, root, InputFrame{});
        tree = buildAccessibilityTree(arena, root, input);
    }

    std::size_t countOf(Role role) const {
        std::size_t total = 0;
        for (const AccessibilityNode& node : tree.nodes) {
            if (node.role == role) ++total;
        }
        return total;
    }
};

}  // namespace

TEST("the tree collapses everything that exists only for layout") {
    Reader reader;
    reader.frame([](Ui& ui) {
        Interaction none;
        // Three layout boxes around one button, which is how a real screen is
        // built and what the pruning exists for.
        auto card = ui.column({.padding = Edges::all(8.0f)});
        auto row = ui.row({.gap = 6.0f});
        auto slot = ui.column({});
        button(ui, none, "Commit", {.id = "commit"});
        (void)slot;
        (void)row;
        (void)card;
    });

    // The window root, and the button. Nothing else survives.
    CHECK_EQ(reader.tree.nodes.size(), std::size_t{2});
    const AccessibilityNode* commit = reader.tree.find("commit");
    CHECK(commit != nullptr);
    if (commit) {
        CHECK(commit->role == Role::Button);
        CHECK(commit->name == "Commit");
        // Re-parented to the root, because everything between them was layout.
        CHECK_EQ(commit->parent, reader.tree.root);
        CHECK(commit->bounds.width > 0.0f);
    }
}

TEST("a hidden subtree is not in the tree at all") {
    Reader reader;
    MarqueeState state;
    reader.frame([&](Ui& ui) {
        Interaction none;
        (void)marquee(ui, none, "ticker", state, 0.0f,
                      [](Ui& inner) { inner.label("ALPHA 12.40"); },
                      {.name = "Market"});
    });

    // The strip is one `Group`, not two: the second pass is drawn to hide the
    // seam and a reader given both is read the same sentence twice.
    CHECK_EQ(reader.countOf(Role::Group), std::size_t{2});   // the window, and the strip
}

TEST("a caption's name lands on the control, and the caption keeps its own") {
    Reader reader;
    TextEditState text{};
    reader.frame([&](Ui& ui) {
        Interaction none;
        field(ui, none, "f",
              {.label = "Repository",
               .forId = "name",
               .help = "Lowercase, no spaces.",
               .required = true},
              [&](Ui& inner) { (void)textInput(inner, none, "name", text); });
    });

    const AccessibilityNode* control = reader.tree.find("name");
    const AccessibilityNode* caption = reader.tree.find("f.label");
    const AccessibilityNode* help = reader.tree.find("f.help");
    CHECK(control != nullptr);
    CHECK(caption != nullptr);
    CHECK(help != nullptr);
    if (!control || !caption || !help) return;

    // Resolved onto the control, which never knew it was named.
    CHECK_EQ(control->labelledBy, caption->id);
    CHECK(control->name == "Repository");
    // And `required` came across the same edge, because the asterisk is a
    // statement about the control drawn where there was room for it.
    CHECK(control->state.required == Flag::True);
    // The help is a description of the control, not a sibling paragraph.
    CHECK_EQ(control->describedBy, help->id);
    CHECK(control->description == "Lowercase, no spaces.");
}

TEST("a row with no name of its own is named by the text in it") {
    Reader reader;
    reader.frame([](Ui& ui) {
        auto row = listRow(ui, {.selected = true, .id = "row.3"});
        ui.label("themes/nord/theme.json");
        (void)row;
    });

    const AccessibilityNode* row = reader.tree.find("row.3");
    CHECK(row != nullptr);
    if (row) {
        CHECK(row->role == Role::ListItem);
        CHECK(row->name == "themes/nord/theme.json");
        CHECK(row->state.selected == Flag::True);
    }
}

TEST("a name is not gathered through a node that has one of its own") {
    // A table must not be announced as every cell it contains, which is what an
    // unbounded text walk would do.
    Reader reader;
    TableState state;
    const std::vector<Column> columns{{.title = "Author"}, {.title = "Commits"}};
    reader.frame([&](Ui& ui) {
        Interaction none;
        (void)table(ui, none, "stats", columns, 2, state,
                    [](Ui& cell, std::size_t row, std::size_t) {
                        cell.label(row == 0 ? "ana" : "bruno");
                    },
                    {.virtualise = false, .name = "Commits by author"});
    });

    const AccessibilityNode* whole = reader.tree.find("stats");
    CHECK(whole != nullptr);
    if (whole) CHECK(whole->name == "Commits by author");

    // The cells are their own nodes and carry their own text.
    bool sawCell = false;
    for (const AccessibilityNode& node : reader.tree.nodes) {
        if (node.role != Role::Cell) continue;
        sawCell = true;
        CHECK(node.name == "ana" || node.name == "bruno");
    }
    CHECK(sawCell);
}

TEST("an id is the same node next frame, so an unchanged frame diffs to nothing") {
    Reader reader;
    const auto build = [](Ui& ui) {
        Interaction none;
        (void)checkbox(ui, none, "tags", true, {.label = "Show tags"});
        button(ui, none, "Commit", {.id = "commit"});
    };

    reader.frame(build);
    const AccessibilityTree first = reader.tree;
    reader.frame(build);

    // Every node in the arena is new — that is the memory model — and every id
    // here is the same, because an id is a function of the tag.
    CHECK_EQ(first.nodes.size(), reader.tree.nodes.size());
    for (std::size_t i = 0; i < first.nodes.size(); ++i) {
        CHECK_EQ(first.nodes[i].id, reader.tree.nodes[i].id);
    }

    const AccessibilityUpdate update = diffAccessibility(first, reader.tree);
    CHECK(update.empty());
}

TEST("a diff reports the node that changed and nothing else") {
    Reader reader;
    bool checked = false;
    const auto build = [&](Ui& ui) {
        Interaction none;
        (void)checkbox(ui, none, "tags", checked, {.label = "Show tags"});
        button(ui, none, "Commit", {.id = "commit"});
    };

    reader.frame(build);
    const AccessibilityTree before = reader.tree;
    checked = true;
    reader.frame(build);

    const AccessibilityUpdate update = diffAccessibility(before, reader.tree);
    CHECK_EQ(update.changed.size(), std::size_t{1});
    if (update.changed.size() == 1) {
        CHECK(update.changed.front().tag == "tags");
        CHECK(update.changed.front().state.checked == Flag::True);
    }
    CHECK(update.removed.empty());
}

TEST("a row that goes away is reported as removed") {
    Reader reader;
    std::size_t rows = 3;
    const auto build = [&](Ui& ui) {
        for (std::size_t i = 0; i < rows; ++i) {
            auto row = listRow(ui, {.id = "row." + std::to_string(i)});
            ui.label("line " + std::to_string(i));
            (void)row;
        }
    };

    reader.frame(build);
    const AccessibilityTree before = reader.tree;
    rows = 2;
    reader.frame(build);

    const AccessibilityUpdate update = diffAccessibility(before, reader.tree);
    CHECK_EQ(update.removed.size(), std::size_t{1});
    const AccessibilityNode* gone = before.find("row.2");
    CHECK(gone != nullptr);
    if (gone && update.removed.size() == 1) CHECK_EQ(update.removed.front(), gone->id);
}

/** Focus moves between two nodes that are otherwise unchanged, so it is
 *  reported on its own. It is the one message a reader must never miss. */
TEST("focus is reported even when nothing else about either node changed") {
    Reader reader;
    const auto build = [](Ui& ui) {
        Interaction none;
        button(ui, none, "Commit", {.id = "commit"});
        button(ui, none, "Discard", {.id = "discard"});
    };

    reader.frame(build);
    const AccessibilityTree before = reader.tree;
    CHECK_EQ(diffAccessibility(before, before).focus, AccessibilityId{0});

    reader.input.focus("discard");
    reader.frame(build);

    const AccessibilityUpdate update = diffAccessibility(before, reader.tree);
    const AccessibilityNode* discard = reader.tree.find("discard");
    CHECK(discard != nullptr);
    if (discard) {
        CHECK(discard->focused);
        CHECK_EQ(update.focus, discard->id);
    }
    CHECK(update.focusMoved);
}

TEST("a virtualised row carries its real position into the tree") {
    Reader reader;
    ScrollState scroll;
    reader.frame([&](Ui& ui) {
        Interaction none;
        (void)virtualList(ui, none, "commits", scroll,
                          {.count = 50000, .rowHeight = 24.0f, .height = 120.0f,
                           .name = "Commits"},
                          [](Ui& row, std::size_t index) {
                              row.label("commit " + std::to_string(index));
                          });
    });

    const AccessibilityNode* list = reader.tree.find("commits");
    CHECK(list != nullptr);
    // `commits` is the scroll viewport; the list is its content, and both are in
    // the tree because both are things a reader meets.
    CHECK(reader.countOf(Role::List) == 1);

    std::size_t items = 0;
    for (const AccessibilityNode& node : reader.tree.nodes) {
        if (node.role != Role::ListItem) continue;
        ++items;
        CHECK_EQ(node.setSize, std::size_t{50000});
        CHECK(node.positionInSet >= 1);
        // Named from its own text, so a reader hears the commit and not "list
        // item".
        CHECK(node.name.rfind("commit ", 0) == 0);
    }
    CHECK(items > 0);
    CHECK(items < 100);
}

TEST("an open select's active descendant resolves to the highlighted row") {
    Reader reader;
    const std::vector<std::string> items{"main", "develop", "release"};
    SelectState state;
    state.open = true;
    state.highlighted = 2;
    reader.frame([&](Ui& ui) {
        Interaction none;
        SelectOptions options;
        options.name = "Branch";
        (void)select(ui, none, "branch", items, std::size_t{0}, state, options);
    });

    const AccessibilityNode* box = reader.tree.find("branch");
    const AccessibilityNode* row = reader.tree.find("branch.list.2");
    const AccessibilityNode* list = reader.tree.find("branch.list");
    CHECK(box != nullptr);
    CHECK(row != nullptr);
    CHECK(list != nullptr);
    if (!box || !row || !list) return;

    CHECK_EQ(box->activeDescendant, row->id);
    CHECK_EQ(box->controls, list->id);
    CHECK(list->role == Role::ListBox);
    CHECK(row->name == "release");
}

// ---- the keyboard, where it was missing -------------------------------------
//
// Stage 6 and stage 7, for the two the audit had already found. Both are cases
// where the role was right and the control was still unreachable, which is the
// failure a role alone cannot catch — and the reason the audit is a test rather
// than a read.

TEST("a colour picker can be driven from the keyboard") {
    // It could not be, at all: the square had no Tab stop and no keys, so the
    // only way to pick a colour was to point at it.
    Screen screen;
    ColorPickerState state;
    state.value = Hsv{200.0f, 0.5f, 0.5f, 1.0f};
    // The picker's own `Interaction`, not a throwaway: this is the one test in
    // the file that drives a control rather than reading the tree it built.
    const auto build = [&](Ui& ui) {
        (void)colorPicker(ui, screen.input, "pick", state, {.name = "Accent"});
    };

    screen.frame(build);

    // The real frame order, which matters here and did not in the cases above:
    // the keys are resolved against last frame's tree *before* the build, so a
    // component reads them while it is being built.
    const auto press = [&](Key key, bool shift = false) {
        InputFrame event;
        event.keys.push_back(KeyEvent{key, Modifiers{.shift = shift}});
        screen.input.update(screen.arena, NodeId{0}, event);
        screen.input.focus("pick.square", FocusSource::Keyboard);

        screen.arena.reset();
        Ui ui(screen.arena);
        ui.setMeasure(&measureFixed, screen.theme.typography());
        {
            auto column = ui.column({.gap = 8.0f, .width = kWindow.width});
            build(ui);
            (void)column;
        }
        LayoutContext context;
        context.theme = &screen.theme;
        context.measure = &measureFixed;
        layout(screen.arena, ui.root(), kWindow, context);
    };

    const float saturation = state.value.saturation;
    press(Key::Right);
    CHECK(state.value.saturation > saturation);
    press(Key::Left);
    CHECK_NEAR(state.value.saturation, saturation);

    // Up and Down are the second axis, which is the whole reason the square
    // cannot be a slider.
    const float brightness = state.value.value;
    press(Key::Up);
    CHECK(state.value.value > brightness);

    // Shift is the coarse gesture, ten times over.
    press(Key::Home);
    CHECK_NEAR(state.value.saturation, 0.0);
    press(Key::Right, true);
    CHECK_NEAR(state.value.saturation, 0.5);
    press(Key::End);
    CHECK_NEAR(state.value.saturation, 1.0);
}

TEST("the picker's three targets are Tab stops, and say what they are") {
    Screen screen;
    ColorPickerState state;
    screen.frame([&](Ui& ui) {
        Interaction none;
        (void)colorPicker(ui, none, "pick", state, {.name = "Accent"});
    });

    for (const char* tag : {"pick.square", "pick.hue", "pick.alpha"}) {
        bool focusable = false;
        for (std::size_t i = 0; i < screen.arena.size(); ++i) {
            const Node& node = screen.arena[NodeId{static_cast<std::uint32_t>(i)}];
            if (node.id == tag) focusable = node.focusable;
        }
        CHECK(focusable);
        const Accessibility* info = screen.of(tag);
        CHECK(info != nullptr);
        if (info) {
            CHECK(info->role != Role::None);
            CHECK(!info->name.empty());
        }
    }
}

/**
 * The bug this pins: Tab walked straight out of the back of a modal into the
 * page the backdrop says cannot be used, with nothing on screen saying where
 * the keyboard had gone.
 */
TEST("a modal confines Tab to itself, and gives the keyboard back when it closes") {
    Theme theme = Theme::dark();
    Arena arena;
    Interaction input;
    bool open = false;
    Vec2 at{200.0f, 100.0f};

    const auto frame = [&](const InputFrame& event = {}) {
        arena.reset();
        Ui ui(arena);
        ui.setMeasure(&measureFixed, theme.typography());
        NodeId root;
        {
            auto column = ui.column({.gap = 8.0f, .width = kWindow.width});
            button(ui, input, "Open", {.id = "open"});
            button(ui, input, "Elsewhere", {.id = "elsewhere"});
            if (open) {
                Modal dialog = modal(ui, input, "confirm", "Discard changes?", at);
                at = dialog.result.position;
                button(ui, input, "Cancel", {.id = "cancel"});
                button(ui, input, "Discard", {.id = "discard"});
            }
            root = column.id();
        }
        LayoutContext context;
        context.theme = &theme;
        context.measure = &measureFixed;
        layout(arena, root, kWindow, context);
        input.update(arena, root, event);
    };

    const auto tab = [&] {
        InputFrame event;
        event.keys.push_back(KeyEvent{Key::Tab});
        frame(event);
    };

    frame();
    input.focus("elsewhere", FocusSource::Keyboard);
    frame();
    CHECK(input.isFocused("elsewhere"));

    // Opening moves the keyboard inside, because a dialog nobody can type into
    // is a dialog that has to be dismissed with the pointer.
    open = true;
    frame();
    CHECK(!input.isFocused("elsewhere"));
    CHECK(input.isFocused("confirm.close"));

    // …and Tab stays inside, however long it is held.
    for (int i = 0; i < 8; ++i) {
        tab();
        const std::string_view where = input.focused();
        CHECK(where == "confirm.close" || where == "cancel" || where == "discard");
    }

    // Closing puts it back where it was, rather than at the top of the page.
    open = false;
    frame();
    CHECK(input.isFocused("elsewhere"));
}

/**
 * A toast is the one component here whose *only* job is to be announced.
 *
 * Which means the role is not decoration on top of the drawing: it is the
 * delivery mechanism, and a stack of messages a reader is never told about is a
 * stack of messages that were not sent.
 */
TEST("a toast is a live region, and which one depends on the news") {
    Screen screen;
    ToastState toasts;
    toasts.push({.id = "saved", .kind = ToastKind::Success, .title = "Saved",
                 .message = "3 commits", .duration = 0.0});
    toasts.push({.id = "failed", .kind = ToastKind::Error, .title = "Fetch failed",
                 .message = "Could not reach origin.", .duration = 0.0});
    screen.frame([&](Ui& ui) {
        Interaction none;
        (void)toast(ui, none, toasts, 0.0f, {.bounds = kWindow});
    });

    // Polite waits for a pause; an error interrupts, because the next thing the
    // reader was about to do will not work.
    const Accessibility* saved = screen.of("toast.saved");
    CHECK(saved != nullptr);
    if (saved) {
        CHECK(saved->role == Role::Status);
        CHECK(saved->name == "Saved");
        CHECK(saved->description == "3 commits");
    }
    const Accessibility* failed = screen.of("toast.failed");
    CHECK(failed != nullptr);
    if (failed) CHECK(failed->role == Role::Alert);

    // "3 of 4" is the only thing telling a reader where they are in a stack.
    if (saved && failed) {
        CHECK_EQ(saved->setSize, std::size_t{2});
        CHECK_EQ(failed->setSize, std::size_t{2});
        CHECK(saved->positionInSet != failed->positionInSet);
    }

    // Four buttons all called "Dismiss" are four buttons nobody can tell apart.
    const Accessibility* close = screen.of("toast.failed.close");
    CHECK(close != nullptr);
    if (close) {
        CHECK(close->role == Role::Button);
        CHECK(close->name == "Dismiss: Fetch failed");
    }
}

TEST("a toast never takes the keyboard, and only its controls are Tab stops") {
    // A message that stole focus would interrupt whatever the reader was
    // typing. The live region delivers it instead.
    Screen screen;
    ToastState toasts;
    toasts.push({.id = "t", .message = "Deleted three files", .duration = 0.0,
                 .action = "Undo"});
    screen.frame([&](Ui& ui) {
        Interaction none;
        (void)toast(ui, none, toasts, 0.0f, {.bounds = kWindow});
    });

    for (std::size_t i = 0; i < screen.arena.size(); ++i) {
        const Node& node = screen.arena[NodeId{static_cast<std::uint32_t>(i)}];
        if (!node.focusable) continue;
        // Only the × and the action; never the card, never the stack.
        CHECK(node.id == "toast.t.close" || node.id == "toast.t.action");
    }
}

TEST("a comparison is a slider, and says which side it is revealing") {
    // PrimeVue reaches the same answer through a hidden range input. Here the
    // role *is* the control, so there is nothing hidden to keep in step.
    Screen screen;
    screen.frame([](Ui& ui) {
        Interaction none;
        (void)compare(
            ui, none, "cmp", 0.6f, [](Ui& inner) { inner.label("before"); },
            [](Ui& inner) { inner.label("after"); },
            {.name = "Before and after retouching",
             .beforeLabel = "Original",
             .afterLabel = "Retouched",
             .height = 120.0f});
    });

    const Accessibility* info = screen.of("cmp");
    CHECK(info != nullptr);
    if (!info) return;
    CHECK(info->role == Role::Slider);
    CHECK(info->name == "Before and after retouching");
    CHECK(info->value.present);
    CHECK_NEAR(info->value.now, 0.6);
    // "60 percent" alone says sixty percent of what, revealing what.
    CHECK(info->value.text == "60% Retouched");

    // Both sides are named and **both stay in the tree** whatever the handle is
    // doing: a reader is not comparing them by eye, and hiding one would leave
    // them with half the comparison.
    bool sawBefore = false;
    bool sawAfter = false;
    for (std::size_t i = 0; i < screen.arena.size(); ++i) {
        const Accessibility* entry =
            screen.arena.accessibility(NodeId{static_cast<std::uint32_t>(i)});
        if (!entry) continue;
        if (entry->name == "Original") sawBefore = true;
        if (entry->name == "Retouched") sawAfter = true;
    }
    CHECK(sawBefore);
    CHECK(sawAfter);
}

TEST("a carousel hides the slides that are not on screen") {
    // What PrimeVue does, and it is right: a reader walking a carousel is
    // walking what is *on screen*. Eight slides all present at once turns a
    // control into a list they have to find their way out of.
    Screen screen;
    CarouselState state;
    state.first = 2;
    screen.frame([&](Ui& ui) {
        Interaction none;
        (void)carousel(ui, none, "strip", 6, state, 0.0f,
                       [](Ui& inner, std::size_t index) {
                           inner.label("slide " + std::to_string(index));
                       },
                       {.slidesPerPage = 2.0f, .name = "Screenshots", .height = 80.0f});
    });

    const Accessibility* region = screen.of("strip");
    CHECK(region != nullptr);
    if (region) {
        CHECK(region->role == Role::Group);
        CHECK(region->name == "Screenshots");
    }

    for (std::size_t i = 0; i < 6; ++i) {
        const Accessibility* info = screen.of("strip." + std::to_string(i));
        CHECK(info != nullptr);
        if (!info) continue;
        // Two showing from index 2.
        CHECK_EQ(info->hidden, !(i == 2 || i == 3));
        // "3 of 6" is the only thing that says where in the strip they are.
        CHECK_EQ(info->setSize, std::size_t{6});
        CHECK_EQ(info->positionInSet, i + 1);
    }
}

TEST("a carousel's dots are a tab list, and say which is in force") {
    // One of the two patterns ARIA blesses for a carousel, and the one that
    // fits: the dots are a set of choices with exactly one in force, and the
    // strip is the single keyboard stop they follow.
    Screen screen;
    CarouselState state;
    state.first = 1;
    screen.frame([&](Ui& ui) {
        Interaction none;
        (void)carousel(ui, none, "strip", 4, state, 0.0f,
                       [](Ui& inner, std::size_t) { inner.label("x"); },
                       {.name = "Screenshots", .height = 80.0f});
    });

    const Accessibility* dots = screen.of("strip.dots");
    CHECK(dots != nullptr);
    if (dots) {
        CHECK(dots->role == Role::TabList);
        CHECK(dots->relations.activeDescendant == "strip.dot.1");
    }

    const Accessibility* current = screen.of("strip.dot.1");
    const Accessibility* other = screen.of("strip.dot.2");
    if (current) {
        CHECK(current->role == Role::Tab);
        CHECK(current->name == "Slide 2");
        CHECK(current->state.selected == Flag::True);
    }
    if (other) CHECK(other->state.selected == Flag::False);
}

TEST("every picture in a gallery has a name, even when the caller gave none") {
    // An unnamed picture in a set of nine is "image, image, image". The
    // fallback is not a guess about *what* it is — nothing can guess that — it
    // is a guess about where it is, which is at least true.
    Screen screen;
    std::vector<std::uint8_t> pixels(4 * 4 * 4, 128);
    const Bitmap plate{pixels.data(), 4, 4, 0};
    const std::vector<GalleryItem> items{
        {.image = plate, .caption = "Ridge line", .alt = "A ridge at first light"},
        {.image = plate, .caption = "The approach"},
        {.image = plate},
    };
    GalleryState state;
    state.current = 1;
    screen.frame([&](Ui& ui) {
        Interaction none;
        (void)gallery(ui, none, "g", items, state,
                      {.thumbnailSize = 30.0f, .name = "Site survey", .height = 100.0f});
    });

    // The alt wins where there is one; the caption stands in where there is
    // not — a caption that describes the photograph *is* the alt text.
    const Accessibility* first = screen.of("g.thumb.0");
    if (first) CHECK(first->name == "A ridge at first light");
    const Accessibility* second = screen.of("g.thumb.1");
    if (second) CHECK(second->name == "The approach");
    const Accessibility* third = screen.of("g.thumb.2");
    if (third) CHECK(third->name == "Image 3 of 3");

    // The stage is the picture, not a group wrapping one: there is exactly one
    // thing here, and a wrapper would put a level between the reader and it.
    const Accessibility* stage = screen.of("g.stage");
    CHECK(stage != nullptr);
    if (stage) {
        CHECK(stage->role == Role::Image);
        CHECK(stage->name == "The approach");
        CHECK_EQ(stage->positionInSet, std::size_t{2});
        CHECK_EQ(stage->setSize, std::size_t{3});
    }
}
