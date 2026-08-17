#include "gbui/widgets/accordion.hpp"

#include <string>

#include "detail.hpp"
#include "gbui/widgets/icon.hpp"
#include "gbui/widgets/text.hpp"

namespace gbui {

using namespace detail;

namespace {

/** The next header the arrows may land on, skipping disabled ones and stopping
 *  at the ends. Not wrapping, unlike a radio group: a stack of sections has a
 *  top and a bottom the reader can see, and wrapping from the last to the first
 *  reads as the list having scrolled. */
std::optional<std::size_t> step(const std::vector<AccordionSection>& sections, std::size_t from,
                                bool forward) {
    if (forward) {
        for (std::size_t at = from + 1; at < sections.size(); ++at) {
            if (!sections[at].disabled) return at;
        }
    } else {
        for (std::size_t at = from; at > 0; --at) {
            if (!sections[at - 1].disabled) return at - 1;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> firstEnabled(const std::vector<AccordionSection>& sections,
                                        bool fromEnd) {
    for (std::size_t i = 0; i < sections.size(); ++i) {
        const std::size_t at = fromEnd ? sections.size() - 1 - i : i;
        if (!sections[at].disabled) return at;
    }
    return std::nullopt;
}

}  // namespace

AccordionResult accordion(Ui& ui, const Interaction& input, std::string_view id,
                          const std::vector<AccordionSection>& sections, AccordionState& state,
                          const AccordionOptions& options) {
    AccordionResult result;

    Style stack;
    stack.direction = Direction::Column;
    stack.gap = options.gap;
    stack.minHeight = 0.0f;
    auto stackScope = ui.scope(stack);
    ui.tag(id);
    if (!options.name.empty()) {
        ui.accessible({.role = Role::Group, .name = options.name});
    }

    /** Where the keyboard is, as an index — `state.focused` is an id so that a
     *  reordered list keeps the keyboard on the same *section*. */
    std::optional<std::size_t> focusedAt;
    for (std::size_t i = 0; i < sections.size(); ++i) {
        if (sections[i].id == state.focused) focusedAt = i;
    }

    for (std::size_t i = 0; i < sections.size(); ++i) {
        const AccordionSection& section = sections[i];
        const bool open = state.isOpen(section.id);
        const std::string headerId = std::string(id) + "." + std::string(section.id);
        const bool hovered = input.isHovered(headerId);

        Style shell;
        shell.direction = Direction::Column;
        shell.radius = 8.0f;
        shell.background = Fill{Token::BgElevated};
        shell.border = Border{1.0f, Fill{open ? Token::BorderStrong : Token::Border}};
        shell.overflow = Overflow::Hidden;
        shell.minHeight = 0.0f;
        auto shellScope = ui.scope(shell);

        {
            Style header;
            header.direction = Direction::Row;
            header.align = Align::Center;
            header.gap = 10.0f;
            header.minHeight = 40.0f;
            header.padding = Edges::symmetric(8.0f, 12.0f);
            header.radius = 0.0f;
            if (hovered && !section.disabled) header.background = Fill{Token::SurfaceHover};
            if (input.isFocusVisible(headerId)) {
                header.outline = Outline{2.0f, -2.0f, Fill{Token::Accent}};
            }
            header.opacity = opacityFor(section.disabled);
            header.cursorHint = section.disabled ? Cursor::NotAllowed : Cursor::Pointer;

            auto headerScope = ui.scope(header);
            ui.tag(headerId).focusable(!section.disabled).cursor(header.cursorHint);
            // A button that says what pressing it will do. `expanded` is the
            // whole of that: without it a reader presses to find out, and the
            // press is the thing they were trying to decide about.
            ui.accessible({
                .role = Role::Button,
                .name = section.title,
                .description = section.detail,
                .state = {.expanded = flag(open), .disabled = flag(section.disabled)},
                .positionInSet = i + 1,
                .setSize = sections.size(),
            });

            if (section.icon) {
                icon(ui, *section.icon, {.color = Token::TextMuted, .size = 15.0f});
            }
            {
                Style column;
                column.direction = Direction::Column;
                column.gap = 2.0f;
                column.grow = 1.0f;
                column.basis = 0.0f;
                column.minWidth = 0.0f;
                auto columnScope = ui.scope(column);
                text(ui, section.title, {.color = Token::TextStrong, .weight = FontWeight::Medium});
                if (!section.detail.empty()) {
                    text(ui, section.detail, {.color = Token::TextMuted, .size = 11.5f});
                }
                (void)columnScope;
            }
            if (!section.badge.empty()) {
                text(ui, section.badge, {.color = Token::TextMuted, .size = 11.5f});
            }
            // Down when closed and up when open, the same disclosure the select
            // box uses — a chevron that only ever points one way is a
            // decoration.
            icon(ui, open ? Icon::ChevronUp : Icon::ChevronDown,
                 {.color = Token::TextMuted, .size = 14.0f});
            (void)headerScope;
        }

        // Only while it is open, and that is the point: a closed section costs
        // nothing rather than being built and hidden.
        if (open && section.body) {
            Style panel;
            panel.direction = Direction::Column;
            panel.gap = 8.0f;
            panel.padding = Edges{0.0f, 12.0f, 12.0f, 12.0f};
            panel.minHeight = 0.0f;
            auto panelScope = ui.scope(panel);
            // Named after its header, which is what makes a reader landing in
            // the body know which section they are in.
            ui.accessible({.role = Role::Group, .name = section.title});
            section.body(ui);
            (void)panelScope;
        }
        shellScope.close();

        if (section.disabled) continue;
        if (activated(input, headerId, false)) {
            result.toggled = section.id;
            state.focused = std::string(section.id);
            if (options.exclusive) {
                const bool wasOpen = open;
                state.open.clear();
                if (!wasOpen) state.open.emplace(section.id);
            } else {
                state.toggle(section.id);
            }
        }
    }
    stackScope.close();

    // The arrows move between headers rather than inside them. Read after the
    // stack is built, so a header that has just appeared can be moved to.
    if (focusedAt || !state.focused.empty()) {
        const std::size_t from = focusedAt.value_or(0);
        for (const KeyEvent& event : input.keys()) {
            std::optional<std::size_t> target;
            switch (event.key) {
                case Key::Down:
                    target = step(sections, from, true);
                    break;
                case Key::Up:
                    target = step(sections, from, false);
                    break;
                case Key::Home:
                    target = firstEnabled(sections, false);
                    break;
                case Key::End:
                    target = firstEnabled(sections, true);
                    break;
                default:
                    break;
            }
            if (target && input.isFocused(std::string(id) + "." + std::string(sections[from].id))) {
                state.focused = std::string(sections[*target].id);
                result.focus =
                    ui.qualify(std::string(id) + "." + std::string(sections[*target].id));
            }
        }
    }
    return result;
}

}  // namespace gbui
