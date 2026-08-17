#include "gbui/widgets/breadcrumbs.hpp"

#include <string>

#include "detail.hpp"
#include "gbui/widgets/icon.hpp"
#include "gbui/widgets/text.hpp"

namespace gbui {

using namespace detail;

BreadcrumbsResult breadcrumbs(Ui& ui, const Interaction& input, std::string_view id,
                              const std::vector<Crumb>& trail, const BreadcrumbsOptions& options) {
    BreadcrumbsResult result;
    if (trail.empty()) return result;

    const std::size_t count = trail.size();
    const std::string ellipsisId = std::string(id) + ".more";

    // ---- what survives the collapse ----------------------------------------
    //
    // The first, the last, and as many of the tail as the budget allows — the
    // ellipsis takes one of the slots, because a trail that hid three steps
    // behind something the reader cannot see is a trail that lies about its
    // length.
    //
    // `hiddenFrom`/`hiddenTo` is the half-open run that is not drawn.
    std::size_t hiddenFrom = 0;
    std::size_t hiddenTo = 0;
    if (options.maxVisible > 0 && count > options.maxVisible) {
        // One slot for the root, one for the ellipsis, the rest for the tail.
        const std::size_t tail = options.maxVisible >= 2 ? options.maxVisible - 2 : 0;
        hiddenFrom = 1;
        hiddenTo = count - tail;
        if (hiddenTo <= hiddenFrom) hiddenTo = hiddenFrom + 1;
        if (hiddenTo > count - 1) hiddenTo = count - 1;
    }

    Style row;
    row.direction = Direction::Row;
    row.align = Align::Center;
    row.gap = 4.0f;
    row.minWidth = 0.0f;
    auto rowScope = ui.scope(row);
    ui.tag(id);
    // A group with a name, which is what turns a row of links into a trail. The
    // name is the one every screen reader's user is listening for.
    ui.accessible({.role = Role::Group, .name = options.name});

    bool drewEllipsis = false;
    for (std::size_t i = 0; i < count; ++i) {
        const bool hiding = hiddenTo > hiddenFrom && i >= hiddenFrom && i < hiddenTo;
        if (hiding) {
            if (drewEllipsis) continue;
            drewEllipsis = true;

            icon(ui, options.separator, {.color = Token::TextMuted, .size = 12.0f});

            Style more;
            more.align = Align::Center;
            more.justify = Justify::Center;
            more.minWidth = 22.0f;
            more.minHeight = 20.0f;
            more.shrink = 0.0f;
            more.radius = 4.0f;
            if (input.isHovered(ellipsisId)) more.background = Fill{Token::SurfaceHover};
            if (input.isFocusVisible(ellipsisId)) {
                more.outline = Outline{2.0f, 1.0f, Fill{Token::Accent}};
            }
            {
                auto moreScope = ui.scope(more);
                ui.tag(ellipsisId).focusable().cursor(Cursor::Pointer);
                // Says how many, because "…" is a button whose whole meaning is
                // the number it is standing in for.
                ui.accessible(
                    {.role = Role::Button,
                     .name = "Show " + std::to_string(hiddenTo - hiddenFrom) + " more steps"});
                text(ui, "…", {.color = Token::TextMuted, .size = options.size});
                (void)moreScope;
            }
            if (activated(input, ellipsisId, false)) result.expanded = true;
            continue;
        }

        if (i > 0) icon(ui, options.separator, {.color = Token::TextMuted, .size = 12.0f});

        const bool last = i + 1 == count;
        const std::string crumbId = std::string(id) + "." + std::to_string(i);
        const bool hovered = input.isHovered(crumbId);

        Style cell;
        cell.direction = Direction::Row;
        cell.align = Align::Center;
        cell.gap = 5.0f;
        cell.minWidth = 0.0f;
        cell.shrink = 1.0f;
        cell.padding = Edges::symmetric(2.0f, 5.0f);
        cell.radius = 4.0f;
        if (!last) {
            if (hovered) cell.background = Fill{Token::SurfaceHover};
            if (input.isFocusVisible(crumbId)) {
                cell.outline = Outline{2.0f, 1.0f, Fill{Token::Accent}};
            }
            cell.cursorHint = Cursor::Pointer;
        }

        {
            auto cellScope = ui.scope(cell);
            // **The last crumb is not a link.** It takes no press and no focus,
            // because it is where the reader already is — a link to the page you
            // are on is a control that does nothing, and a reader who tabs
            // through five crumbs to find that out has been misled five times.
            if (last) {
                ui.tag(crumbId);
                ui.accessible({.role = Role::None,
                               .name = trail[i].name.empty() ? trail[i].label : trail[i].name,
                               .state = {.current = Flag::True}});
            } else {
                ui.tag(crumbId).focusable().cursor(Cursor::Pointer);
                ui.accessible({.role = Role::Link,
                               .name = trail[i].name.empty() ? trail[i].label : trail[i].name});
            }
            if (trail[i].icon) {
                icon(ui, *trail[i].icon,
                     {.color = last ? Token::Text : Token::TextMuted, .size = 13.0f});
            }
            if (!trail[i].label.empty()) {
                text(ui, trail[i].label,
                     {.color = last      ? Token::TextStrong
                               : hovered ? Token::Text
                                         : Token::TextMuted,
                      .weight = last ? FontWeight::Medium : FontWeight::Regular,
                      .size = options.size,
                      .underline = !last && hovered});
            }
            (void)cellScope;
        }
        if (!last && activated(input, crumbId, false)) result.chosen = i;
    }
    rowScope.close();
    return result;
}

}  // namespace gbui
