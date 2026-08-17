#include "gbui/widgets/pagination.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "detail.hpp"
#include "gbui/widgets/icon.hpp"
#include "gbui/widgets/text.hpp"

namespace gbui {

using namespace detail;

namespace {

/** A page to draw, or the gap between two runs of them. `kGap` is not a page
 *  number, so it is spelt as one that cannot be. */
constexpr std::size_t kGap = static_cast<std::size_t>(-1);

/**
 * The window: first, last, and `around` either side of the current page, with a
 * gap wherever the run jumps.
 *
 * Built as a set of *page numbers* rather than as a run of buttons, because the
 * ends and the middle can overlap — page 2 of 5 with `around = 1` is 1 2 3 …
 * 5, and the naive version emits a gap of one page, which is an ellipsis
 * standing in for a single button nobody can now reach.
 */
std::vector<std::size_t> window(std::size_t current, std::size_t count, std::size_t around) {
    std::vector<std::size_t> pages;
    if (count == 0) return pages;

    std::vector<bool> keep(count, false);
    keep[0] = true;
    keep[count - 1] = true;
    const std::size_t from = current > around ? current - around : 0;
    const std::size_t to = std::min(count - 1, current + around);
    for (std::size_t at = from; at <= to; ++at) keep[at] = true;

    for (std::size_t at = 0; at < count; ++at) {
        if (keep[at]) {
            pages.push_back(at);
            continue;
        }
        // A gap of exactly one page is drawn as the page: an ellipsis that
        // stands in for a single button is strictly worse than the button.
        const bool single =
            at + 1 < count && keep[at + 1] && !pages.empty() && pages.back() == at - 1;
        if (single) {
            pages.push_back(at);
        } else if (!pages.empty() && pages.back() != kGap) {
            pages.push_back(kGap);
        }
    }
    return pages;
}

}  // namespace

PaginationResult pagination(Ui& ui, const Interaction& input, std::string_view id,
                            std::size_t current, std::size_t pageCount,
                            const PaginationOptions& options) {
    PaginationResult result;
    if (pageCount == 0) return result;
    current = std::min(current, pageCount - 1);

    const std::string previousId = std::string(id) + ".previous";
    const std::string nextId = std::string(id) + ".next";

    Style row;
    row.direction = Direction::Row;
    row.align = Align::Center;
    row.gap = 4.0f;
    auto rowScope = ui.scope(row);
    ui.tag(id);
    ui.accessible({.role = Role::Group, .name = options.name});

    /** One square button. Everything here is one of these. */
    const auto cell = [&](std::string_view cellId, bool active, bool dead) {
        Style box;
        box.align = Align::Center;
        box.justify = Justify::Center;
        box.minWidth = 28.0f;
        box.minHeight = 28.0f;
        box.shrink = 0.0f;
        box.padding = Edges::symmetric(0.0f, 6.0f);
        box.radius = 6.0f;
        box.opacity = opacityFor(dead);
        box.cursorHint = dead ? Cursor::NotAllowed : Cursor::Pointer;
        if (active) {
            box.background = Fill{Token::Accent, 0.18f};
            box.border = Border{1.0f, Fill{Token::Accent, 0.5f}};
        } else if (input.isHovered(cellId) && !dead) {
            box.background = Fill{Token::SurfaceHover};
        }
        if (input.isFocusVisible(cellId)) box.outline = Outline{2.0f, 1.0f, Fill{Token::Accent}};
        return box;
    };

    if (options.arrows) {
        const bool dead = current == 0;
        {
            const Style box = cell(previousId, false, dead);
            auto scope = ui.scope(box);
            ui.tag(previousId).focusable(!dead).cursor(box.cursorHint);
            ui.accessible(
                {.role = Role::Button, .name = "Previous page", .state = {.disabled = flag(dead)}});
            icon(ui, Icon::ChevronLeft, {.color = Token::TextMuted, .size = 14.0f});
            (void)scope;
        }
        if (!dead && activated(input, previousId, false)) result.chosen = current - 1;
    }

    for (const std::size_t page : window(current, pageCount, options.around)) {
        if (page == kGap) {
            Style gap;
            gap.align = Align::Center;
            gap.justify = Justify::Center;
            gap.minWidth = 20.0f;
            gap.minHeight = 28.0f;
            gap.shrink = 0.0f;
            auto scope = ui.scope(gap);
            // Hidden: a reader running through the buttons wants the pages, and
            // "ellipsis" between two of them is noise. The count is already in
            // each button's own name.
            ui.accessible({.hidden = true});
            text(ui, "…", {.color = Token::TextMuted, .size = options.size});
            (void)scope;
            continue;
        }

        const bool active = page == current;
        const std::string pageId = std::string(id) + "." + std::to_string(page);
        {
            const Style box = cell(pageId, active, false);
            auto scope = ui.scope(box);
            // The page you are on takes no press and no focus: it is a control
            // that would do nothing, and `current` is how a reader is told which
            // one it is.
            if (active) {
                ui.tag(pageId);
                ui.accessible({.role = Role::None,
                               .name = "Page " + std::to_string(page + 1) + " of " +
                                       std::to_string(pageCount),
                               .state = {.current = Flag::True}});
            } else {
                ui.tag(pageId).focusable().cursor(Cursor::Pointer);
                ui.accessible({.role = Role::Button,
                               .name = "Page " + std::to_string(page + 1),
                               .positionInSet = page + 1,
                               .setSize = pageCount});
            }
            text(ui, std::to_string(page + 1),
                 {.color = active ? Token::TextStrong : Token::TextMuted,
                  .weight = active ? FontWeight::Medium : FontWeight::Regular,
                  .size = options.size});
            (void)scope;
        }
        if (!active && activated(input, pageId, false)) result.chosen = page;
    }

    if (options.arrows) {
        const bool dead = current + 1 >= pageCount;
        {
            const Style box = cell(nextId, false, dead);
            auto scope = ui.scope(box);
            ui.tag(nextId).focusable(!dead).cursor(box.cursorHint);
            ui.accessible(
                {.role = Role::Button, .name = "Next page", .state = {.disabled = flag(dead)}});
            icon(ui, Icon::ChevronRight, {.color = Token::TextMuted, .size = 14.0f});
            (void)scope;
        }
        if (!dead && activated(input, nextId, false)) result.chosen = current + 1;
    }
    rowScope.close();

    // The arrows step a page from anywhere inside the control, so a reader who
    // has tabbed to page 7 can walk with the keyboard instead of aiming at the
    // next number.
    if (input.isFocusedWithin(id)) {
        for (const KeyEvent& event : input.keys()) {
            switch (event.key) {
                case Key::Left:
                    if (current > 0) result.chosen = current - 1;
                    break;
                case Key::Right:
                    if (current + 1 < pageCount) result.chosen = current + 1;
                    break;
                case Key::Home:
                    if (current != 0) result.chosen = 0;
                    break;
                case Key::End:
                    if (current + 1 != pageCount) result.chosen = pageCount - 1;
                    break;
                default:
                    break;
            }
        }
    }
    return result;
}

}  // namespace gbui
