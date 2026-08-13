#include "gbui/widgets/tabs.hpp"

#include <algorithm>
#include <string>

#include "detail.hpp"
#include "gbui/widgets/icon.hpp"
#include "gbui/widgets/text.hpp"

namespace gbui {

// What this shares with its siblings, rather than a copy in each.
using namespace detail;

namespace {

/** The next tab the arrow keys should land on, skipping disabled ones and
 *  stopping at the ends rather than wrapping — a tab strip is a row of things
 *  in an order, and walking off the end of it back to the start loses the
 *  reader's place. */
std::size_t stepOver(const std::vector<TabItem>& items, std::size_t from, bool forward) {
    const std::size_t count = items.size();
    std::size_t at = from;
    while (true) {
        if (forward) {
            if (at + 1 >= count) return from;
            ++at;
        } else {
            if (at == 0) return from;
            --at;
        }
        if (!items[at].disabled) return at;
    }
}

std::size_t firstEnabled(const std::vector<TabItem>& items, bool fromEnd) {
    const std::size_t count = items.size();
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t at = fromEnd ? count - 1 - i : i;
        if (!items[at].disabled) return at;
    }
    return 0;
}

}  // namespace

std::optional<std::size_t> tabs(Ui& ui, const Interaction& input, std::string_view id,
                                const std::vector<TabItem>& items, std::size_t selected,
                                const TabsOptions& options) {
    std::optional<std::size_t> chosen;
    if (items.empty()) return chosen;
    selected = std::min(selected, items.size() - 1);

    // The strip is the keyboard stop; the tabs inside it are not, so Tab leaves
    // the strip rather than walking through every tab in it.
    if (input.isFocused(id)) {
        for (const KeyEvent& event : input.keys()) {
            if (event.key == Key::Left || event.key == Key::Up) {
                chosen = stepOver(items, selected, false);
            } else if (event.key == Key::Right || event.key == Key::Down) {
                chosen = stepOver(items, selected, true);
            } else if (event.key == Key::Home) {
                chosen = firstEnabled(items, false);
            } else if (event.key == Key::End) {
                chosen = firstEnabled(items, true);
            }
        }
    }

    const bool vertical = options.orientation == TabsOrientation::Vertical;

    Style strip;
    strip.direction = vertical ? Direction::Column : Direction::Row;
    strip.align = vertical ? Align::Stretch : Align::End;
    strip.gap = options.gap;
    strip.shrink = 0.0f;
    if (vertical) {
        strip.width = options.thickness;
    } else {
        strip.minHeight = options.thickness;
    }
    if (options.rule) {
        // A rule along the strip's trailing edge. `Border` is all four sides,
        // so the rule is padding plus a node rather than a border — the same
        // reason `beginToolbar` grew a divider instead of a bottom border.
        strip.padding = vertical ? Edges{0.0f, 1.0f, 0.0f, 0.0f}
                                 : Edges{0.0f, 0.0f, 1.0f, 0.0f};
    }

    auto scope = ui.begin(strip);
    ui.tag(id).focusable();

    const Rect stripFrame = input.frameOf(id);
    Rect activeFrame{};

    std::string_view openGroup{};
    for (std::size_t i = 0; i < items.size(); ++i) {
        const TabItem& item = items[i];

        // A heading whenever the run changes. Not a tab: it has no id, takes no
        // index, and cannot be hovered or focused.
        if (vertical && !item.group.empty() && item.group != openGroup) {
            Style heading;
            heading.minHeight = options.itemHeight * 0.82f;
            heading.align = Align::Center;
            heading.justify = Justify::Start;
            heading.shrink = 0.0f;
            heading.padding = Edges{i == 0 ? 2.0f : 10.0f, 10.0f, 4.0f, 12.0f};
            auto headingScope = ui.begin(heading);
            text(ui, item.group,
                 {.color = Token::TextMuted, .weight = FontWeight::SemiBold, .size = 10.0f,
                  .overflow = TextOverflow::Ellipsis});
            (void)headingScope;
        }
        openGroup = item.group;

        const std::string tabId = std::string(id) + "." + std::to_string(i);
        const bool active = i == selected;
        const bool hovered = input.isHovered(tabId) && !item.disabled;
        if (active) activeFrame = input.frameOf(tabId);

        Style tab;
        tab.direction = Direction::Row;
        tab.align = Align::Center;
        tab.gap = 8.0f;
        tab.minHeight = vertical ? options.itemHeight
                                 : options.thickness - (options.rule ? 1.0f : 0.0f);
        // Vertical rows keep a lane free on the leading edge for the indicator,
        // so the label does not shift sideways when a tab becomes active.
        // Room under a horizontal label, so the indicator sits clear of its
        // descenders instead of underlining them.
        tab.padding = options.itemPadding.value_or(vertical ? Edges{0.0f, 10.0f, 0.0f, 12.0f}
                                                            : Edges{0.0f, 12.0f, 6.0f, 12.0f});
        tab.radius = 6.0f;
        tab.shrink = vertical ? 0.0f : 1.0f;
        tab.opacity = opacityFor(item.disabled);
        if (hovered && !active) tab.background = Fill{Token::SurfaceHover};
        tab.cursorHint = item.disabled ? Cursor::NotAllowed : Cursor::Pointer;
        // The ring goes on the strip, not on a tab: the strip is what the
        // keyboard actually lands on.
        if (active && input.isFocusVisible(id)) {
            tab.outline = Outline{2.0f, 1.0f, Fill{Token::Accent}};
        }

        auto tabScope = ui.begin(tab);
        ui.tag(tabId).cursor(tab.cursorHint);

        const Token label = item.disabled ? Token::TextMuted
                            : active      ? Token::TextStrong
                                          : Token::Text;
        if (item.icon) icon(ui, *item.icon, {.color = label, .size = 14.0f});
        text(ui, item.label,
             {.color = label,
              .weight = active ? FontWeight::SemiBold : FontWeight::Regular,
              .size = 12.0f,
              .grow = vertical ? 1.0f : 0.0f});
        (void)tabScope;

        if (!item.disabled && input.clicked(tabId)) chosen = i;
    }

    // The indicator, from last frame's geometry of the active tab. Animated, so
    // it travels between tabs rather than teleporting — and because the target
    // only changes when the selection does, standing still costs nothing.
    if (options.indicator && stripFrame.width > 0.0f && activeFrame.width > 0.0f) {
        // One pair of numbers whichever way the strip runs: where the bar
        // starts along the strip, and how long it is.
        const float target = vertical ? activeFrame.y - stripFrame.y
                                      : activeFrame.x - stripFrame.x;
        const float span = vertical ? activeFrame.height : activeFrame.width;
        constexpr Transition kSlide{.duration = 0.2f, .easing = Easing::EaseOut};
        const float along = ui.animate(id, "indicator.at", target, kSlide);
        const float length = ui.animate(id, "indicator.len", span, kSlide);

        Style bar;
        bar.position = Position::Absolute;
        bar.left = vertical ? options.indicatorInset : along;
        bar.top = vertical ? along
                           : stripFrame.height - options.indicatorWidth - options.indicatorInset;
        bar.width = vertical ? options.indicatorWidth : length;
        bar.height = vertical ? length : options.indicatorWidth;
        bar.radius = options.indicatorWidth / 2.0f;
        bar.background = Fill{Token::Accent};
        ui.add(bar);
    }
    (void)scope;

    if (chosen && *chosen == selected) chosen.reset();  // choosing what is chosen is not a change
    return chosen;
}


void tabPanels(Ui& ui, std::size_t selected,
               const std::vector<std::function<void(Ui&)>>& panels,
               const TabPanelsOptions& options) {
    for (std::size_t i = 0; i < panels.size(); ++i) {
        if (!panels[i]) continue;
        const bool shown = i == selected;
        if (!shown && options.lazy) continue;

        Style panel;
        panel.direction = Direction::Column;
        panel.grow = shown ? 1.0f : 0.0f;
        if (shown) {
            panel.basis = 0.0f;
        } else {
            // Present, and occupying nothing. Height rather than a visibility
            // flag because there is no such flag — and a zero-height clip is
            // what `display: none` would have to compile to here anyway.
            panel.height = 0.0f;
            panel.shrink = 0.0f;
            panel.overflow = Overflow::Hidden;
        }
        auto scope = ui.begin(panel);
        panels[i](ui);
        (void)scope;
    }
}

}  // namespace gbui
