#include "gbui/widgets/select.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "detail.hpp"
#include "gbui/widgets/icon.hpp"
#include "gbui/widgets/menu.hpp"
#include "gbui/widgets/popover.hpp"
#include "gbui/widgets/textInput.hpp"
#include "gbui/widgets/text.hpp"

namespace gbui {

// What this shares with its siblings, rather than a copy in each.
using namespace detail;

namespace {

/** The space between two rows of the open list, and the padding around them. */
constexpr float kRowGap = 2.0f;
constexpr float kListPadding = 6.0f;

RowMetrics rowsOf() { return {kMenuItemHeight, kRowGap, 0.0f}; }

/** Wraps, because a list walked with the arrow keys should not stop dead at
 *  either end — the same rule the closed box already follows. */
std::size_t step(std::size_t from, std::size_t count, bool forward) {
    return forward ? (from + 1) % count : (from + count - 1) % count;
}

/**
 * Whether an item survives the filter.
 *
 * A case-insensitive **substring**, and not a fuzzy match. Fuzzy matching is a
 * ranking problem wearing a filter's clothes: it reorders the list under the
 * reader, it matches things they cannot see why it matched, and the only way to
 * find out why is to read the scoring function. A branch picker wants "the ones
 * with `nord` in them", which is a question with one answer.
 *
 * ASCII folding only. Anything more is a locale-aware casing table, which is a
 * larger dependency than this whole component — and the same line the calendar
 * draws for the same reason.
 */
bool matchesQuery(std::string_view item, std::string_view query) {
    if (query.empty()) return true;
    if (query.size() > item.size()) return false;
    const auto lower = [](char c) {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    };
    for (std::size_t at = 0; at + query.size() <= item.size(); ++at) {
        bool same = true;
        for (std::size_t i = 0; i < query.size() && same; ++i) {
            same = lower(item[at + i]) == lower(query[i]);
        }
        if (same) return true;
    }
    return false;
}

/**
 * The node a tag was put on, searched from the end.
 *
 * `ui.accessible` writes to the *last* node built, and a component builds a
 * subtree — so after `textInput` returns, the last node is its caret or its
 * icon and not the box the keyboard is on. A relation put there would be a
 * relation on a node no reader ever reaches.
 *
 * From the end because the one wanted is always the most recent, and a linear
 * walk of one frame's nodes once per open list is not a cost worth a map.
 */
NodeId nodeWithTag(const Arena& arena, std::string_view tag) {
    for (std::size_t i = arena.size(); i > 0; --i) {
        const NodeId id{static_cast<NodeId::Index>(i - 1)};
        if (arena[id].id == tag) return id;
    }
    return NodeId{};
}

/**
 * What the closed box says when it is holding several rows.
 *
 * Below `summariseFrom` the labels themselves, because "main, develop" is more
 * use than "2 selected"; at or above it the count, because a box listing nine
 * branch names is a box whose own label has gone.
 */
std::string summarise(const std::vector<std::string>& items,
                      const std::vector<std::size_t>& selected, std::size_t summariseFrom) {
    if (selected.empty()) return {};
    if (summariseFrom > 0 && selected.size() >= summariseFrom) {
        return std::to_string(selected.size()) + " selected";
    }
    std::string out;
    for (const std::size_t index : selected) {
        if (index >= items.size()) continue;
        if (!out.empty()) out += ", ";
        out += items[index];
    }
    return out;
}

SelectResult selectImpl(Ui& ui, const Interaction& input, std::string_view id,
                        const std::vector<std::string>& items,
                        const std::vector<std::size_t>& selected, SelectState& state,
                        const SelectOptions& options) {
    SelectResult result;

    /** Whether a row is in the caller's set. Linear, and deliberately: a select
     *  is dozens of options, and a set would cost more to build each frame than
     *  the walk costs to do. */
    const auto isSelected = [&](std::size_t index) {
        for (const std::size_t entry : selected) {
            if (entry == index) return true;
        }
        return false;
    };
    /** The first value, which is what the single-value behaviours read: where
     *  the highlight opens, and what the arrows step from when closed. */
    const std::optional<std::size_t> firstSelected =
        selected.empty() ? std::nullopt : std::optional<std::size_t>(selected.front());

    const bool hovered = input.isHovered(id);
    const bool focused = input.isFocused(id);
    const bool wasOpen = state.open;
    const std::size_t count = items.size();

    // ---- the closed box ---------------------------------------------------
    Style box;
    box.direction = Direction::Row;
    box.align = Align::Center;
    const float controlHeight =
        options.height > 0.0f ? options.height : ui.design().controlHeight;
    box.minHeight = controlHeight;
    box.width = options.width;
    box.grow = options.grow;
    box.gap = 8.0f;
    box.padding = Edges::symmetric(0.0f, 10.0f);
    const FieldPalette off = disabledPalette();
    box.background = options.disabled ? off.background : Fill{Token::Bg};
    box.border = Border{1.0f, Fill{options.disabled ? off.border
                                   : hovered        ? Token::BorderStrong
                                                    : Token::Border}};
    if (input.isFocusVisible(id)) box.outline = Outline{2.0f, 2.0f, Fill{Token::Accent}};
    box.opacity = opacityFor(options.disabled);
    box.cursorHint = options.disabled ? Cursor::NotAllowed : Cursor::Pointer;

    const std::string listId = std::string(id) + ".list";
    const std::string filterId = listId + ".filter";
    // Which node has to announce the highlight, kept because the box is closed
    // and several nodes behind by the time the keyboard has been read.
    NodeId boxNode;
    /** The filter box, when there is one — see `nodeWithTag`. */
    NodeId queryNode;

    // ---- what the filter leaves ---------------------------------------------
    //
    // Indices into the caller's list, never positions in the filtered view.
    // Everything downstream — the highlight, the chosen index, the row ids,
    // `activeDescendant` — is in the caller's numbering, because the moment two
    // numberings exist one of them ends up in a result.
    std::vector<std::size_t> matches;
    const auto refilter = [&] {
        matches.clear();
        matches.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            if (!options.filter || matchesQuery(items[i], state.query.text)) matches.push_back(i);
        }
    };
    refilter();
    /** Where an item sits in what is on screen, or nothing when it is filtered
     *  out. */
    const auto shownAt = [&](std::size_t item) -> std::optional<std::size_t> {
        for (std::size_t at = 0; at < matches.size(); ++at) {
            if (matches[at] == item) return at;
        }
        return std::nullopt;
    };

    {
        auto scope = ui.scope(box);
        boxNode = scope.id();
        ui.tag(id).focusable(!options.disabled).cursor(box.cursorHint);

        // One label whichever form this is: a single value reads as itself, and
        // several read as themselves until there are too many to be a label.
        const std::string shown = summarise(items, selected, options.multiple
                                                                 ? options.summariseFrom
                                                                 : 0);
        const bool hasValue = !shown.empty();
        ui.accessible({
            .role = Role::ComboBox,
            .name = options.name,
            .description = options.placeholder,
            .state = {.expanded = flag(wasOpen), .disabled = flag(options.disabled)},
            .value = {.present = true, .text = shown},
            .relations = {.controls = listId},
        });
        text(ui, hasValue ? std::string_view(shown) : options.placeholder,
             {.color = options.disabled ? Token::TextMuted
                       : hasValue        ? Token::Text
                                         : Token::TextMuted,
              .grow = 1.0f,
              .overflow = TextOverflow::Ellipsis});
        // Down when closed, up when open — the convention every platform's
        // combobox uses. A right-pointing chevron is a *disclosure* triangle
        // and belongs on a tree node, where the thing it reveals appears
        // beside it rather than below.
        icon(ui, wasOpen ? Icon::ChevronUp : Icon::ChevronDown,
             {.color = Token::TextMuted, .size = 14.0f});
        // The focus ring only means something on a control the keyboard can
        // reach, and a disabled one cannot.
        (void)scope;
    }

    if (options.disabled) return result;

    // ---- opening, closing, and walking ------------------------------------
    // The highlight starts on the value, so opening a list and pressing Return
    // changes nothing — which is what a keyboard user expects from a control
    // they opened to look at.
    const auto open = [&] {
        state.open = true;
        state.highlighted = firstSelected;
        // A filter left over from last time is a list with rows missing and
        // nothing on screen saying why.
        state.query = TextEditState{};
        if (options.filter) result.focus = ui.qualify(filterId);
    };
    const auto close = [&] {
        state.open = false;
        state.highlighted.reset();
        // The keyboard has to come back, or it is left on a filter box that no
        // longer exists — and `Interaction`'s recovery would then clear it
        // rather than return it to the control the reader was using.
        if (options.filter) result.focus = ui.qualify(id);
    };

    if (input.clicked(id)) {
        if (wasOpen) close();
        else open();
    }

    // While the list is open the keyboard is in the filter box, not on the
    // control — so the list's own keys have to answer to either.
    const bool driving = focused || (options.filter && input.isFocused(filterId));

    if (driving) {
        for (const KeyEvent& event : input.keys()) {
            if (event.key == Key::Escape && options.dismissOnEscape) {
                // Escape clears the filter before it closes the list. Two
                // meanings for one key, in the order a reader wants them: the
                // first press undoes the typing, the second gives up.
                if (options.filter && state.open && !state.query.text.empty()) {
                    state.query = TextEditState{};
                    continue;
                }
                close();
                continue;
            }
            // Space commits a plain select and **types a space** in a filter
            // box. A combobox that could not have a space in its query would
            // be a combobox that cannot find "feat/nord tuning".
            const bool commits =
                event.key == Key::Return ||
                (event.key == Key::Space && !(options.filter && state.open));
            if (commits) {
                if (!state.open) {
                    open();
                } else {
                    // Committing what is highlighted. A list opened and left
                    // alone highlights the current value, so this is a no-op
                    // rather than a surprise.
                    if (state.highlighted && *state.highlighted < count) {
                        result.chosen = state.highlighted;
                    }
                    // Holding several, the list stays up: a reader taking three
                    // branches should not have to re-open it twice, and the
                    // press that took the third is not a press that says "done".
                    if (!options.multiple) close();
                }
                continue;
            }
            if (count == 0) continue;

            const bool down = event.key == Key::Down;
            const bool up = event.key == Key::Up;
            if (state.open) {
                // The arrows walk what is *on screen*. Stepping the underlying
                // index instead would walk into rows the filter has taken away,
                // and the highlight would vanish for several presses at a time.
                if ((down || up) && !matches.empty()) {
                    const std::optional<std::size_t> here =
                        state.highlighted ? shownAt(*state.highlighted) : std::nullopt;
                    const std::size_t at =
                        here ? step(*here, matches.size(), down)
                             : (down ? 0 : matches.size() - 1);
                    state.highlighted = matches[at];
                } else if (event.key == Key::Home && !matches.empty()) {
                    state.highlighted = matches.front();
                } else if (event.key == Key::End && !matches.empty()) {
                    state.highlighted = matches.back();
                }
            } else if (down || up) {
                // Closed, the arrows step the value itself, which is how a
                // select behaves everywhere.
                // Closed, the arrows step the value itself, which is how a
                // select behaves everywhere — but only when there *is* one
                // value. Stepping a set has no meaning, so a multiple one
                // opens instead, which is what the reader wanted anyway.
                if (options.multiple) open();
                else result.chosen = step(firstSelected.value_or(0), count, down);
            }
        }
    }

    if (!state.open) {
        state.list.offset = 0.0f;
        return result;
    }

    // ---- the list ---------------------------------------------------------
    PopoverOptions popoverOptions;
    popoverOptions.placement = options.placement;
    popoverOptions.gap = options.gap;
    popoverOptions.margin = options.margin;
    popoverOptions.flip = options.flip;
    popoverOptions.shift = options.shift;
    popoverOptions.bounds = options.bounds;
    popoverOptions.matchAnchorWidth = true;
    popoverOptions.padding = Edges::all(kListPadding);
    popoverOptions.gapBetweenItems = kRowGap;
    popoverOptions.scroll = options.listScroll;
    // A list of values, which is what makes its rows options rather than
    // commands and what a reader is told before the first of them.
    popoverOptions.role = Role::ListBox;

    auto list = popover(ui, input, listId, id, popoverOptions);
    // How many of them the reader may take, on the list itself. Without it a
    // multi-select list is announced exactly like a single-select one, and the
    // first choice appears to have thrown the previous one away. Written to the
    // popover's own node rather than through `PopoverOptions`, which carries a
    // role and a name and no business knowing about this.
    if (options.multiple) {
        ui.accessible(list.id(), {.state = {.multiSelectable = Flag::True}});
    }
    {
        // As many rows as `maxVisible` allows, and the rest behind a scroll.
        // Every row is built either way — a select is dozens of options, not
        // the fifty thousand `virtualList` exists for.
        // ---- the filter box --------------------------------------------
        //
        // Inside the popover and above the rows, which is where every picker
        // that does this puts it — and the only place it can be without the
        // closed control becoming an editable box when it is not one.
        if (options.filter) {
            TextInputOptions query;
            query.name = options.name;
            query.placeholder = options.filterPlaceholder;
            query.leading = Icon::Search;
            query.grow = 1.0f;
            (void)textInput(ui, input, filterId, state.query, query);
            // Kept, because the relations below have to go on the box the
            // keyboard is on and the highlight is not known until the rows
            // have been filtered against what was just typed.
            queryNode = nodeWithTag(ui.arena(), ui.qualify(filterId));

            // ---- and only now is the query this frame's ------------------
            //
            // `textInput` applies the keystroke while it is being *built*, so
            // everything above ran against the query as it was last frame. The
            // list is rebuilt against the new one here rather than a frame
            // later — a list that lags the typing is a list where Return can
            // commit the row the reader had before their last letter.
            refilter();

            // How much is left, in words. A filter that silently drops
            // thirty-eight of forty rows has told a sighted reader everything
            // and a screen reader nothing; `Status` is the polite live region
            // that says so at the next pause.
            if (!state.query.text.empty()) {
                text(ui, std::to_string(matches.size()) + " of " + std::to_string(count),
                     {.color = Token::TextMuted, .size = 11.0f});
                ui.accessible({.role = Role::Status,
                               .name = std::to_string(matches.size()) + " of " +
                                       std::to_string(count) + " shown"});
            }
        }

        // A highlight the filter has just taken away is a highlight on
        // nothing, and Return would commit a row the reader cannot see. It
        // falls to the first match, which is also what makes "type three
        // letters and press Return" work — the gesture the feature is for.
        if (!state.highlighted || !shownAt(*state.highlighted)) {
            if (matches.empty()) state.highlighted.reset();
            else state.highlighted = matches.front();
        }
        // Which row the keys are on. On the closed box always, and on the
        // filter box as well when there is one — because that is where the
        // keyboard actually is, and a reader typing has to be told their
        // letters are moving a highlight somewhere they are not.
        // A `string`, not a `string_view`. The view form binds to the temporary
        // the concatenation makes, and that temporary dies at the end of the
        // *declaration* rather than at the end of the call it is used in — so
        // both relations below read freed memory and came out empty. It worked
        // while the expression sat inside the call and stopped the moment it
        // was hoisted to be used twice.
        const std::string active =
            state.highlighted ? listId + "." + std::to_string(*state.highlighted)
                              : std::string{};
        ui.accessible(boxNode, {.state = {.expanded = Flag::True},
                                .relations = {.activeDescendant = active}});
        if (queryNode.valid()) {
            ui.accessible(queryNode,
                          {.relations = {.controls = listId, .activeDescendant = active}});
        }
        // Kept on screen before the view is built, so the offset the rows are
        // laid out against is the one this frame decided — and counted in
        // *visible* rows, because that is what the scroll view holds.
        if (state.highlighted) {
            if (const std::optional<std::size_t> at = shownAt(*state.highlighted)) {
                revealRow(state.list, rowsOf(), *at);
            }
        }

        const std::size_t visible =
            std::min(std::max<std::size_t>(matches.size(), 1),
                     std::max<std::size_t>(1, options.maxVisible));
        ScrollOptions scroll;
        scroll.axis = options.listScroll;
        scroll.gap = kRowGap;
        scroll.grow = 0.0f;
        // The box keeps the keyboard while its list is open, so the list is not
        // a place Tab can land.
        scroll.focusable = false;
        // A row count and a pixel ceiling answer different questions — "how many
        // do I want to see" and "how much room is there" — so the smaller wins.
        const float byRows = static_cast<float>(visible) * rowsOf().pitch() - kRowGap;
        scroll.height = isAuto(options.maxListHeight)
                            ? byRows
                            : std::min(byRows, options.maxListHeight);
        auto view = scrollArea(ui, input, listId + ".scroll", state.list, scroll);

        // Nothing rather than an empty box: a list that has quietly become
        // blank reads as one that failed to load.
        if (matches.empty()) {
            text(ui, options.emptyMessage, {.color = Token::TextMuted, .size = 12.0f});
        }

        for (std::size_t at = 0; at < matches.size(); ++at) {
            const std::size_t i = matches[at];
            const std::string itemId = listId + "." + std::to_string(i);
            MenuItemOptions row;
            row.selected = isSelected(i);
            // A list of values, not of commands: the tick goes on the right.
            row.checkSide = CheckSide::Trailing;
            row.highlighted = state.highlighted == i;
            row.focusable = false;
            // Values, not commands — see `MenuItemOptions::role`.
            row.role = Role::Option;
            // Its place in what is *shown*, not in the whole list: "3 of 40"
            // in a list filtered to four is three lies in five words.
            row.positionInSet = at + 1;
            row.setSize = matches.size();
            const bool chosen = menuItem(ui, input, itemId, items[i], row);
            if (chosen) {
                result.chosen = i;
                // A row is a toggle when the control holds several, and the
                // list stays where it is: the reader is probably not finished.
                if (!options.multiple) close();
                else state.highlighted = i;
            }
        }
        (void)view;
    }
    (void)list;

    // A press anywhere else puts the list away — outside the list *and*
    // outside the closed box, because a press on the box is the toggle above
    // and closing here as well would shut it on the way down and re-open it on
    // the way up.
    if (options.dismissOnOutsideClick && pressedOutside(input, {ui.qualify(listId),
                                                                ui.qualify(id)})) {
        close();
    }

    return result;
}

}  // namespace

SelectResult select(Ui& ui, const Interaction& input, std::string_view id,
                    const std::vector<std::string>& items, std::optional<std::size_t> selected,
                    SelectState& state, const SelectOptions& options) {
    // One value expressed as a set of at most one, so there is one
    // implementation rather than two that drift apart.
    std::vector<std::size_t> one;
    if (selected && *selected < items.size()) one.push_back(*selected);
    return selectImpl(ui, input, id, items, one, state, options);
}

SelectResult select(Ui& ui, const Interaction& input, std::string_view id,
                    const std::vector<std::string>& items,
                    const std::vector<std::size_t>& selected, SelectState& state,
                    const SelectOptions& options) {
    return selectImpl(ui, input, id, items, selected, state, options);
}

}  // namespace gbui
