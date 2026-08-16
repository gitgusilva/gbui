#include "gbui/widgets/treeView.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "detail.hpp"
#include "gbui/widgets/icon.hpp"
#include "gbui/widgets/text.hpp"

namespace gbui {

// What this shares with its siblings, rather than a copy in each.
using namespace detail;

namespace {

constexpr std::size_t kNone = static_cast<std::size_t>(-1);

/**
 * Which rows are on screen, given what is closed.
 *
 * One pass with a watermark rather than a recursive walk: a row is hidden when
 * some ancestor is closed, and in pre-order that is exactly "we are still
 * deeper than the last closed node". The recursion the data shape saved us is
 * saved here too.
 */
std::vector<std::size_t> visibleRows(const std::vector<TreeItem>& items,
                                     const TreeState& state) {
    std::vector<std::size_t> out;
    out.reserve(items.size());
    std::size_t closedAt = kNone;
    for (std::size_t i = 0; i < items.size(); ++i) {
        const TreeItem& item = items[i];
        if (closedAt != kNone) {
            if (item.depth > closedAt) continue;
            closedAt = kNone;
        }
        out.push_back(i);
        if (item.hasChildren && !state.isExpanded(item.id)) closedAt = item.depth;
    }
    return out;
}

/**
 * Where each visible row sits among its **siblings**, and how many there are.
 *
 * Not its place in the list: "item 2 of 5" in a hierarchy means whose five, and
 * a reader told "row 340 of 900" has been told the size of the file tree rather
 * than the size of the directory they are in.
 *
 * Two linear passes with a counter per depth. The obvious version — scan
 * outwards from each row until the depth drops — is quadratic on a directory
 * with a thousand files in it, which is a directory people have.
 */
void siblingPositions(const std::vector<TreeItem>& items,
                      const std::vector<std::size_t>& visible,
                      std::vector<std::size_t>& position, std::vector<std::size_t>& total) {
    position.assign(visible.size(), 1);
    total.assign(visible.size(), 1);

    std::vector<std::size_t> counter;
    const auto count = [&](std::size_t depth) -> std::size_t& {
        if (counter.size() <= depth) counter.resize(depth + 1, 0);
        // A shallower row ends every run below it: the next thing at depth+1 is
        // the first child of a *different* parent.
        for (std::size_t deeper = depth + 1; deeper < counter.size(); ++deeper) {
            counter[deeper] = 0;
        }
        return counter[depth];
    };

    for (std::size_t k = 0; k < visible.size(); ++k) {
        position[k] = ++count(items[visible[k]].depth);
    }
    counter.clear();
    for (std::size_t k = visible.size(); k > 0; --k) {
        const std::size_t after = count(items[visible[k - 1]].depth)++;
        total[k - 1] = position[k - 1] + after;
    }
}

}  // namespace

TreeResult treeView(Ui& ui, const Interaction& input, std::string_view id,
                    const std::vector<TreeItem>& items, TreeState& state,
                    const TreeViewOptions& options) {
    TreeResult result;
    const std::string listId = std::string(id) + ".rows";

    const std::vector<std::size_t> visible = visibleRows(items, state);
    std::vector<std::size_t> position;
    std::vector<std::size_t> total;
    siblingPositions(items, visible, position, total);

    /** Where the keyboard is, as a position in what is on screen. */
    const auto focusedAt = [&]() -> std::size_t {
        for (std::size_t k = 0; k < visible.size(); ++k) {
            if (items[visible[k]].id == state.focused) return k;
        }
        return kNone;
    };

    const std::string wasFocused = state.focused;
    const std::string wasSelected = state.selected;
    std::size_t at = focusedAt();
    // A tree the reader has not been in yet starts at the top, so the first
    // arrow press moves rather than teleporting from nowhere.
    if (at == kNone && !visible.empty()) at = 0;

    const auto moveTo = [&](std::size_t k) {
        if (k < visible.size()) {
            at = k;
            state.focused = std::string(items[visible[k]].id);
        }
    };

    // ---- the keys ----------------------------------------------------------
    //
    // Right opens a closed node and steps *into* an open one; Left closes an
    // open one and steps *out* of a closed one. That pair is the whole of why a
    // tree feels like a tree — making Right always step turns it into an
    // indented list — and it is what every file browser does.
    if (input.isFocused(id) && !visible.empty()) {
        for (const KeyEvent& event : input.keys()) {
            const TreeItem& here = items[visible[at]];
            switch (event.key) {
                case Key::Down:
                    if (at + 1 < visible.size()) moveTo(at + 1);
                    break;
                case Key::Up:
                    if (at > 0) moveTo(at - 1);
                    break;
                case Key::Home:
                    moveTo(0);
                    break;
                case Key::End:
                    moveTo(visible.size() - 1);
                    break;
                case Key::Right:
                    if (here.hasChildren && !state.isExpanded(here.id)) {
                        state.toggle(here.id);
                        result.toggled = here.id;
                    } else if (at + 1 < visible.size() &&
                               items[visible[at + 1]].depth > here.depth) {
                        moveTo(at + 1);
                    }
                    break;
                case Key::Left:
                    if (here.hasChildren && state.isExpanded(here.id)) {
                        state.toggle(here.id);
                        result.toggled = here.id;
                    } else {
                        // Out to the parent, which in pre-order is the nearest
                        // row above at a shallower depth.
                        for (std::size_t k = at; k > 0; --k) {
                            if (items[visible[k - 1]].depth < here.depth) {
                                moveTo(k - 1);
                                break;
                            }
                        }
                    }
                    break;
                case Key::Return:
                case Key::Space:
                    if (!here.disabled) {
                        state.selected = std::string(here.id);
                        result.activated = here.id;
                    }
                    break;
                default:
                    break;
            }
        }
    }

    // Recomputed after the keys: opening or closing a node changes what is on
    // screen, and the rows below are laid out from this.
    const std::vector<std::size_t> rows =
        result.toggled ? visibleRows(items, state) : visible;
    if (result.toggled) siblingPositions(items, rows, position, total);
    const auto rowAt = [&](std::size_t k) -> const TreeItem& { return items[rows[k]]; };

    // ---- the frame ---------------------------------------------------------
    Style frame;
    frame.direction = Direction::Column;
    frame.width = options.width;
    frame.height = options.height;
    frame.grow = options.grow;
    if (options.grow > 0.0f) frame.basis = 0.0f;
    frame.minHeight = 0.0f;
    if (input.isFocusVisible(id)) frame.outline = Outline{2.0f, 2.0f, Fill{Token::Accent}};

    auto frameScope = ui.scope(frame);
    // One keyboard stop for the whole hierarchy, with `activeDescendant`
    // carrying which row the arrows are on — the roving pattern the tab strip
    // and the calendar already use, and the only one that keeps Tab a way *out*
    // of a nine-hundred-row tree.
    ui.tag(id).focusable().accessible({
        .role = Role::Tree,
        .name = options.name,
        .relations = {.activeDescendant =
                          state.focused.empty()
                              ? std::string_view{}
                              : std::string_view(std::string(id) + "." + state.focused)},
    });

    /** One row, whichever way it was reached. */
    const auto buildRow = [&](Ui& rowUi, std::size_t k) {
        const TreeItem& item = rowAt(k);
        const std::string rowId = std::string(id) + "." + std::string(item.id);
        const std::string twistyId = rowId + ".twisty";
        const bool open = item.hasChildren && state.isExpanded(item.id);
        const bool chosen = item.id == state.selected;
        const bool here = item.id == state.focused;

        Style line;
        line.direction = Direction::Row;
        line.align = Align::Center;
        line.height = options.rowHeight;
        line.gap = 6.0f;
        line.padding = Edges{0.0f, 8.0f, 0.0f, 4.0f};
        line.radius = 4.0f;
        line.opacity = opacityFor(item.disabled);
        if (chosen) line.background = Fill{Token::Accent, 0.18f};
        else if (input.isHovered(rowId)) line.background = Fill{Token::SurfaceHover};
        // The keyboard's row is outlined rather than filled, so it can be told
        // apart from the selection when they are not the same row — which is
        // the whole reason they are two things.
        if (here && input.isFocusVisible(id) && !chosen) {
            line.outline = Outline{2.0f, -1.0f, Fill{Token::Accent}};
        }
        line.cursorHint = item.disabled ? Cursor::NotAllowed : Cursor::Pointer;
        auto lineScope = rowUi.scope(line);
        rowUi.tag(rowId).cursor(line.cursorHint).accessible({
            .role = Role::TreeItem,
            .name = item.label,
            .description = item.detail,
            // `expanded` only where there is something to expand. A leaf that
            // announced "collapsed" would be a leaf a reader keeps pressing
            // Right on.
            .state = {.expanded = item.hasChildren ? flag(open) : Flag::Unset,
                      .selected = flag(chosen),
                      .disabled = flag(item.disabled)},
            .positionInSet = position[k],
            .setSize = total[k],
            .level = item.depth + 1,
        });

        // The indent, as one rigid box rather than as padding: the guides are
        // drawn inside it, and padding has nowhere to put them.
        if (item.depth > 0) {
            Style gutter;
            gutter.width = static_cast<float>(item.depth) * options.indent;
            gutter.height = Length::percent(100);
            gutter.shrink = 0.0f;
            gutter.direction = Direction::Row;
            auto gutterScope = rowUi.scope(gutter);
            if (options.guides) {
                // One hairline per level the row is under, at the left of each
                // step. Indentation alone stops being readable at about four
                // levels, and a file tree is routinely deeper.
                for (std::size_t level = 0; level < item.depth; ++level) {
                    Style lane;
                    lane.width = options.indent;
                    lane.height = Length::percent(100);
                    lane.shrink = 0.0f;
                    auto laneScope = rowUi.scope(lane);
                    Style rule;
                    rule.width = 1.0f;
                    rule.height = Length::percent(100);
                    rule.radius = 0.0f;
                    rule.background = Fill{Token::Border};
                    rowUi.add(rule);
                    (void)laneScope;
                }
            }
            (void)gutterScope;
        }

        // The twisty, and a hole the same size where a leaf would have one:
        // without it every leaf's label sits a chevron to the left of its
        // siblings' and the column stops being a column.
        Style twisty;
        twisty.width = 16.0f;
        twisty.height = 16.0f;
        twisty.shrink = 0.0f;
        twisty.justify = Justify::Center;
        twisty.align = Align::Center;
        twisty.radius = 3.0f;
        if (item.hasChildren && input.isHovered(twistyId)) {
            twisty.background = Fill{Token::SurfaceHover};
        }
        {
            auto twistyScope = rowUi.scope(twisty);
            if (item.hasChildren) {
                rowUi.tag(twistyId).cursor(Cursor::Pointer);
                icon(rowUi, open ? Icon::ChevronDown : Icon::ChevronRight,
                     {.color = Token::TextMuted, .size = 13.0f});
            }
            (void)twistyScope;
        }

        if (item.icon) {
            icon(rowUi, *item.icon,
                 {.color = chosen ? Token::Text : Token::TextMuted, .size = 14.0f});
        }
        text(rowUi, item.label,
             {.color = item.disabled ? Token::TextMuted
                       : chosen      ? Token::TextStrong
                                     : Token::Text,
              .size = 12.5f,
              .grow = 1.0f});
        if (!item.detail.empty()) {
            text(rowUi, item.detail,
                 {.color = Token::TextMuted, .role = FontRole::Mono, .size = 11.0f});
        }
        (void)lineScope;

        // The twisty is checked first: a click on it opens the node and does
        // *not* choose it, which is the difference between "show me what is in
        // here" and "I want this one".
        if (input.clicked(twistyId) && item.hasChildren) {
            state.toggle(item.id);
            result.toggled = item.id;
        } else if (input.clicked(rowId) && !item.disabled) {
            state.focused = std::string(item.id);
            state.selected = std::string(item.id);
            result.activated = item.id;
        }
    };

    if (options.virtualise) {
        VirtualListOptions list;
        list.count = rows.size();
        list.rowHeight = options.rowHeight;
        list.grow = options.grow > 0.0f ? 1.0f : 0.0f;
        list.height = options.height;
        list.name = options.name;
        // The tree owns the keyboard, so its scroller is not a second stop.
        list.focusable = false;
        // And the slot says nothing: a tree's rows are `TreeItem`s counted
        // among their siblings, not `ListItem`s counted among nine hundred.
        list.itemRole = Role::None;
        result.shown = virtualList(ui, input, listId, state.view, list, buildRow);
    } else {
        ScrollOptions scroll;
        scroll.direction = Direction::Column;
        scroll.grow = options.grow > 0.0f ? 1.0f : 0.0f;
        scroll.height = options.height;
        scroll.focusable = false;
        scroll.name = options.name;
        auto scroller = scrollArea(ui, input, listId, state.view, scroll);
        ui.accessible({.role = Role::Group, .name = options.name});
        for (std::size_t k = 0; k < rows.size(); ++k) buildRow(ui, k);
        result.shown = {0, rows.size(), rows.size(), options.rowHeight};
        (void)scroller;
    }
    (void)frameScope;

    // Kept on screen after the row was chosen rather than before it: the key
    // that moved the keyboard is the one this frame is drawing, and revealing
    // where it *was* would leave the reader a row behind their own arrows.
    if (state.focused != wasFocused) {
        for (std::size_t k = 0; k < rows.size(); ++k) {
            if (rowAt(k).id != state.focused) continue;
            revealRow(state.view, RowMetrics{options.rowHeight, 0.0f, 0.0f}, k);
            break;
        }
    }
    result.selectionChanged = state.selected != wasSelected;
    return result;
}

}  // namespace gbui
