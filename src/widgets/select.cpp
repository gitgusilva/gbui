#include "gbui/widgets/select.hpp"

#include <algorithm>
#include <string>

#include "detail.hpp"
#include "gbui/widgets/icon.hpp"
#include "gbui/widgets/menu.hpp"
#include "gbui/widgets/popover.hpp"
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

}  // namespace

SelectResult select(Ui& ui, const Interaction& input, std::string_view id,
                    const std::vector<std::string>& items, std::optional<std::size_t> selected,
                    SelectState& state, const SelectOptions& options) {
    SelectResult result;

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
    // Which node has to announce the highlight, kept because the box is closed
    // and several nodes behind by the time the keyboard has been read.
    NodeId boxNode;

    {
        auto scope = ui.scope(box);
        boxNode = scope.id();
        ui.tag(id).focusable(!options.disabled).cursor(box.cursorHint);

        const bool hasValue = selected && *selected < count;
        ui.accessible({
            .role = Role::ComboBox,
            .name = options.name,
            .description = options.placeholder,
            .state = {.expanded = flag(wasOpen), .disabled = flag(options.disabled)},
            .value = {.present = true,
                      .text = hasValue ? std::string_view(items[*selected])
                                       : std::string_view{}},
            .relations = {.controls = listId},
        });
        text(ui, hasValue ? std::string_view(items[*selected]) : options.placeholder,
             {.color = options.disabled ? Token::TextMuted
                       : hasValue        ? Token::Text
                                         : Token::TextMuted,
              .grow = 1.0f});
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
        state.highlighted = selected;
    };
    const auto close = [&] {
        state.open = false;
        state.highlighted.reset();
    };

    if (input.clicked(id)) {
        if (wasOpen) close();
        else open();
    }

    if (focused) {
        for (const KeyEvent& event : input.keys()) {
            if (event.key == Key::Escape) {
                close();
                continue;
            }
            if (event.key == Key::Return || event.key == Key::Space) {
                if (!state.open) {
                    open();
                } else {
                    // Committing what is highlighted. A list opened and left
                    // alone highlights the current value, so this is a no-op
                    // rather than a surprise.
                    if (state.highlighted && *state.highlighted < count) {
                        result.chosen = state.highlighted;
                    }
                    close();
                }
                continue;
            }
            if (count == 0) continue;

            const bool down = event.key == Key::Down;
            const bool up = event.key == Key::Up;
            if (state.open) {
                if (down || up) {
                    // The first press lands on the value rather than one past
                    // it: nothing is highlighted yet, so there is nothing to
                    // step from.
                    state.highlighted = state.highlighted
                                            ? step(*state.highlighted, count, down)
                                            : selected.value_or(0);
                } else if (event.key == Key::Home) {
                    state.highlighted = 0;
                } else if (event.key == Key::End) {
                    state.highlighted = count - 1;
                }
            } else if (down || up) {
                // Closed, the arrows step the value itself, which is how a
                // select behaves everywhere.
                result.chosen = step(selected.value_or(0), count, down);
            }
        }
    }

    if (!state.open) {
        state.list.offset = 0.0f;
        return result;
    }

    // ---- the list ---------------------------------------------------------
    //
    // Said now rather than when the box was built, because the arrow keys above
    // are what moved the highlight. Announcing the value it had before the key
    // would tell a reader about the row they have just left.
    //
    // `activeDescendant` is what makes an open list usable at all: focus stays
    // on the box — deliberately, so Tab cannot fall into the popup — and this
    // is the only way to say which row the keys are on. It is the separation
    // `SelectState::highlighted` exists for, said to the reader rather than to
    // the painter.
    ui.accessible(boxNode, {.state = {.expanded = Flag::True},
                            .relations = {.activeDescendant =
                                              state.highlighted
                                                  ? std::string_view(
                                                        listId + "." +
                                                        std::to_string(*state.highlighted))
                                                  : std::string_view{}}});

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

    // Keeping the highlight on screen. Done before the view is built, so the
    // offset the rows are laid out against is the one this frame decided.
    if (state.highlighted) revealRow(state.list, rowsOf(), *state.highlighted);

    auto list = popover(ui, input, listId, id, popoverOptions);
    {
        // As many rows as `maxVisible` allows, and the rest behind a scroll.
        // Every row is built either way — a select is dozens of options, not
        // the fifty thousand `virtualList` exists for.
        const std::size_t visible = std::min(count, std::max<std::size_t>(1, options.maxVisible));
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

        for (std::size_t i = 0; i < count; ++i) {
            const std::string itemId = listId + "." + std::to_string(i);
            MenuItemOptions row;
            row.selected = selected && *selected == i;
            // A list of values, not of commands: the tick goes on the right.
            row.checkSide = CheckSide::Trailing;
            row.highlighted = state.highlighted == i;
            row.focusable = false;
            // Values, not commands — see `MenuItemOptions::role`.
            row.role = Role::Option;
            const bool chosen = menuItem(ui, input, itemId, items[i], row);
            if (chosen) {
                result.chosen = i;
                close();
            }
        }
        (void)view;
    }
    (void)list;

    // A click anywhere else closes the list. `dragging` empty means the press
    // landed outside every tagged node.
    if (input.pointerDown() && input.dragging().empty()) close();

    return result;
}

}  // namespace gbui
