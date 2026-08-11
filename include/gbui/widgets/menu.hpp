// The rows inside a popover.
#pragma once

#include <optional>
#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/icons.hpp"

namespace gbui {

/** The height of one row. A list that scrolls has to know it before it lays a
 *  row out, so it is named rather than repeated. */
inline constexpr float kMenuItemHeight = 28.0f;

/**
 * Which side the tick for a chosen item goes.
 *
 * Not one answer, because the two lists this serves are read differently. A
 * **menu** is a list of commands, and every desktop menu reserves a gutter down
 * its leading edge for state — the tick sits there, lined up with the icons of
 * the items that have them. A **select** is a list of values, where the leading
 * edge belongs to the labels being compared and the tick belongs out of their
 * way on the right, which is where macOS, Material and Fluent all put it.
 */
enum class CheckSide { Leading, Trailing };

struct MenuItemOptions {
    std::optional<Icon> leading;
    std::string_view shortcut{};
    /** The chosen value: a check mark and the strong text colour. */
    bool selected = false;
    /** Where that check mark goes. Leading suits a menu, trailing a select. */
    CheckSide checkSide = CheckSide::Leading;
    /** Drawn as though the pointer were on it. That is what walking a list with
     *  the arrow keys looks like — the highlight moves and the pointer does
     *  not — and it is a different question from `selected`, which is the value
     *  the list would return. */
    bool highlighted = false;
    bool disabled = false;
    bool danger = false;
    /** Whether Tab can land on it. A list whose owner drives the highlight — a
     *  select — turns this off, so Tab leaves the control instead of walking
     *  into an open popup and back out of the window's order. */
    bool focusable = true;
};

/** Returns true on the frame it was chosen. */
bool menuItem(Ui& ui, const Interaction& input, std::string_view id, std::string_view label,
              const MenuItemOptions& options = {});

/** A rule between groups of items. */
void menuSeparator(Ui& ui);

}  // namespace gbui
