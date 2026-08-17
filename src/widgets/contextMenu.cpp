#include "gbui/widgets/contextMenu.hpp"

#include <algorithm>
#include <string>

#include "detail.hpp"
#include "gbui/widgets/popover.hpp"
#include "gbui/widgets/scroll.hpp"
#include "gbui/widgets/spacing.hpp"

namespace gbui {

using namespace detail;

namespace {

constexpr float kMenuPadding = 5.0f;
constexpr float kRowGap = 1.0f;

/** Which entries the highlight may land on: not separators, not disabled rows.
 *  A highlight that stops on either looks stuck, and pressing Enter on it does
 *  nothing — which reads as a broken menu rather than as a skipped row. */
std::vector<std::size_t> reachable(const std::vector<MenuEntry>& entries) {
    std::vector<std::size_t> out;
    out.reserve(entries.size());
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (!entries[i].separator() && !entries[i].disabled) out.push_back(i);
    }
    return out;
}

/** Wraps, because a menu walked to its end should come back round rather than
 *  stop dead — which is what every desktop menu does. */
std::size_t nextOf(std::size_t at, std::size_t count, bool forward) {
    return forward ? (at + 1) % count : (at + count - 1) % count;
}

/**
 * One menu, wherever it is anchored.
 *
 * The pointer and the keyboard paths are identical from here on, which is the
 * whole reason `menu` and `contextMenu` are one implementation: what differs is
 * a rectangle.
 */
MenuResult menuImpl(Ui& ui, const Interaction& input, std::string_view id, Rect anchor,
                    const std::vector<MenuEntry>& entries, MenuState& state,
                    const MenuOptions& options) {
    MenuResult result;

    const std::vector<std::size_t> walkable = reachable(entries);
    /** Where the highlight is, as a position in `walkable`. */
    std::optional<std::size_t> at;
    for (std::size_t i = 0; i < walkable.size(); ++i) {
        if (entries[walkable[i]].id == state.highlighted) at = i;
    }

    PopoverOptions surface;
    surface.placement = options.placement;
    surface.gap = options.gap;
    surface.margin = options.margin;
    surface.flip = options.flip;
    surface.shift = options.shift;
    surface.bounds = options.bounds;
    surface.padding = Edges::all(kMenuPadding);
    surface.gapBetweenItems = kRowGap;
    surface.minWidth = options.width > 0.0f ? options.width : 160.0f;
    surface.role = Role::Menu;
    surface.name = options.name;
    // A ceiling in pixels rather than a row count, because that is what the
    // placement engine needs to decide which side to open on — a menu that will
    // be clipped to fourteen rows must be *placed* as fourteen rows or it flips
    // above an anchor it would have fitted below.
    if (options.maxVisible > 0 && entries.size() > options.maxVisible) {
        surface.maxHeight = static_cast<float>(options.maxVisible) *
                                (kMenuItemHeight + kRowGap) +
                            2.0f * kMenuPadding;
        surface.scroll = ScrollAxis::Vertical;
        surface.scrollState = &state.list;
    }

    auto box = popover(ui, input, id, anchor, surface);
    // **One Tab stop, on the menu.** Nine commands that each took the keyboard
    // would be nine Tab presses to cross one menu; ARIA's menu is one stop and
    // the arrows do the walking. It is also what lets Escape and Enter be read
    // here rather than by whichever row happens to be focused.
    ui.tag(id).focusable();

    for (std::size_t i = 0; i < entries.size(); ++i) {
        const MenuEntry& entry = entries[i];
        if (entry.separator()) {
            menuSeparator(ui);
            continue;
        }
        const std::string rowId = std::string(id) + "." + std::string(entry.id);
        MenuItemOptions row;
        row.leading = entry.icon;
        row.shortcut = entry.shortcut;
        row.selected = entry.checked;
        row.checkSide = CheckSide::Leading;
        row.highlighted = at && walkable[*at] == i;
        row.disabled = entry.disabled;
        row.danger = entry.danger;
        // The menu holds the keyboard, so a row is a target and not a stop.
        row.focusable = false;
        row.positionInSet = i + 1;
        row.setSize = entries.size();
        if (menuItem(ui, input, rowId, entry.label, row)) result.chosen = entry.id;
    }
    box.close();

    // ---- the keys ----------------------------------------------------------
    if (input.isFocusedWithin(id)) {
        for (const KeyEvent& event : input.keys()) {
            if (event.key == Key::Escape) {
                result.dismissed = true;
                continue;
            }
            if (event.key == Key::Return || event.key == Key::Space) {
                if (at) result.chosen = entries[walkable[*at]].id;
                continue;
            }
            if (walkable.empty()) continue;
            switch (event.key) {
                case Key::Down:
                    at = at ? nextOf(*at, walkable.size(), true) : 0;
                    break;
                case Key::Up:
                    at = at ? nextOf(*at, walkable.size(), false) : walkable.size() - 1;
                    break;
                case Key::Home: at = 0; break;
                case Key::End: at = walkable.size() - 1; break;
                default: break;
            }
        }
        state.highlighted = at ? std::string(entries[walkable[*at]].id) : std::string{};
    } else {
        // Not focused yet — the frame it opened on. Ask for the keyboard so the
        // arrows work without the reader having to click a row first.
        result.focus = ui.qualify(id);
    }

    // A press anywhere else puts it away. The caller owns the flag, so this is
    // reported rather than acted on.
    if (options.dismissOnOutsideClick && pressedOutside(input, {ui.qualify(id)})) {
        result.dismissed = true;
    }
    return result;
}

}  // namespace

MenuResult menu(Ui& ui, const Interaction& input, std::string_view id, std::string_view anchorId,
                const std::vector<MenuEntry>& entries, MenuState& state,
                const MenuOptions& options) {
    return menuImpl(ui, input, id, input.frameOf(anchorId), entries, state, options);
}

MenuResult contextMenu(Ui& ui, const Interaction& input, std::string_view id, Vec2 at,
                       const std::vector<MenuEntry>& entries, MenuState& state,
                       const MenuOptions& options) {
    // A point is a zero-sized rectangle, which the placement engine reads the
    // edges of like any other anchor — so a menu at the pointer flips off the
    // bottom of the window and shifts off its right the same way a dropdown
    // does, with no second code path to get wrong.
    MenuOptions atPointer = options;
    // No gap: a context menu's corner belongs *at* the pointer, and six pixels
    // of air reads as a menu that missed.
    atPointer.gap = 0.0f;
    if (atPointer.placement == Placement::Auto) atPointer.placement = Placement::BottomStart;
    return menuImpl(ui, input, id, Rect{at.x, at.y, 0.0f, 0.0f}, entries, state, atPointer);
}

}  // namespace gbui
