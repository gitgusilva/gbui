#include "gbui/widgets/datePicker.hpp"

#include <algorithm>
#include <chrono>
#include <string>

#include "detail.hpp"
#include "gbui/widgets/button.hpp"
#include "gbui/widgets/icon.hpp"
#include "gbui/widgets/popover.hpp"
#include "gbui/widgets/text.hpp"

namespace gbui {

// What this shares with its siblings, rather than a copy in each.
using namespace detail;

namespace {

namespace cal = std::chrono;

cal::year_month_day toYmd(const Date& date) {
    return cal::year_month_day{cal::year{date.year},
                               cal::month{static_cast<unsigned>(date.month)},
                               cal::day{static_cast<unsigned>(date.day)}};
}

/** How many days a month has, which is the one piece of calendar arithmetic
 *  nobody should write again. */
unsigned daysInMonth(int year, int month) {
    const cal::year_month_day_last last{cal::year{year},
                                        cal::month_day_last{cal::month{
                                            static_cast<unsigned>(month)}}};
    return static_cast<unsigned>(last.day());
}

/** 0 for Sunday, through 6 for Saturday. */
int weekdayOf(const Date& date) {
    return static_cast<int>(cal::weekday{cal::sys_days{toYmd(date)}}.c_encoding());
}

Date monthShifted(const Date& from, int months) {
    int year = from.year;
    int month = from.month + months;
    while (month > 12) { month -= 12; ++year; }
    while (month < 1) { month += 12; --year; }
    // Clamped, so stepping from the 31st into a shorter month lands on its
    // last day rather than rolling into the next one.
    const int day = std::min(from.day, static_cast<int>(daysInMonth(year, month)));
    return Date{year, month, std::max(1, day)};
}

bool withinBounds(const Date& date, const DatePickerOptions& options) {
    if (options.minimum.valid() && date < options.minimum) return false;
    if (options.maximum.valid() && options.maximum < date) return false;
    return true;
}

}  // namespace

bool Date::valid() const {
    return year != 0 && month >= 1 && month <= 12 && day >= 1 && toYmd(*this).ok();
}

long long Date::serial() const {
    if (year == 0) return 0;
    return cal::sys_days{toYmd(*this)}.time_since_epoch().count();
}

Date Date::fromSerial(long long days) {
    const cal::year_month_day ymd{cal::sys_days{cal::days{days}}};
    return Date{static_cast<int>(ymd.year()), static_cast<int>(static_cast<unsigned>(ymd.month())),
                static_cast<int>(static_cast<unsigned>(ymd.day()))};
}

Date Date::today() {
    const auto now = cal::floor<cal::days>(cal::system_clock::now());
    const cal::year_month_day ymd{now};
    return Date{static_cast<int>(ymd.year()), static_cast<int>(static_cast<unsigned>(ymd.month())),
                static_cast<int>(static_cast<unsigned>(ymd.day()))};
}

std::string formatDate(const Date& date, std::string_view pattern,
                       const CalendarLocale& locale) {
    std::string out;
    if (!date.valid()) return out;

    const auto pad = [](int value, int width) {
        std::string digits = std::to_string(value);
        while (static_cast<int>(digits.size()) < width) digits.insert(digits.begin(), '0');
        return digits;
    };
    const auto monthName = [&](std::size_t length) {
        const std::string_view full = locale.months[static_cast<std::size_t>(date.month - 1)];
        return std::string(full.substr(0, std::min(length, full.size())));
    };
    const auto weekdayName = [&](std::size_t length) {
        const auto index = static_cast<std::size_t>(weekdayOf(date));
        const std::string_view full = locale.weekdayNames[index];
        return std::string(full.substr(0, std::min(length, full.size())));
    };
    /** How many of the same letter start here. */
    const auto run = [&](std::size_t at) {
        std::size_t n = 1;
        while (at + n < pattern.size() && pattern[at + n] == pattern[at]) ++n;
        return n;
    };

    for (std::size_t i = 0; i < pattern.size();) {
        const char c = pattern[i];
        // Quoted text is copied out verbatim, which is how a pattern says "de"
        // without the `d` being read as a day. Two quotes are one literal quote.
        if (c == '\'') {
            if (i + 1 < pattern.size() && pattern[i + 1] == '\'') {
                out.push_back('\'');
                i += 2;
                continue;
            }
            ++i;
            while (i < pattern.size() && pattern[i] != '\'') out.push_back(pattern[i++]);
            if (i < pattern.size()) ++i;
            continue;
        }

        const std::size_t count = run(i);
        switch (c) {
            case 'y':
                out += count >= 4 ? pad(date.year, 4) : pad(date.year % 100, 2);
                break;
            case 'M':
                if (count >= 4) out += monthName(64);
                else if (count == 3) out += monthName(3);
                else out += pad(date.month, static_cast<int>(count));
                break;
            case 'd':
                out += pad(date.day, static_cast<int>(count));
                break;
            case 'E':
                out += weekdayName(count >= 4 ? 64 : 3);
                break;
            default:
                out.append(count, c);
                break;
        }
        i += count;
    }
    return out;
}

DatePickerResult datePicker(Ui& ui, const Interaction& input, std::string_view id,
                            const Date& selected, DatePickerState& state,
                            const DatePickerOptions& options) {
    DatePickerResult result;

    // A first frame with nothing set looks at the selection, or at today.
    if (!state.visible.valid()) {
        state.visible = selected.valid() ? selected : options.today;
        if (!state.visible.valid()) state.visible = Date{1970, 1, 1};
    }
    if (!state.focusedDay.valid()) {
        state.focusedDay = selected.valid() ? selected : state.visible;
    }

    const std::string gridId = std::string(id) + ".grid";
    const std::string previousId = std::string(id) + ".previous";
    const std::string nextId = std::string(id) + ".next";

    // ---- the keyboard ------------------------------------------------------
    // The focused day moves; choosing is a separate act. That split is what
    // lets someone walk to a date and change their mind, and it is the same one
    // `select` makes between its highlight and its value.
    if (input.isFocusedWithin(gridId)) {
        for (const KeyEvent& event : input.keys()) {
            Date moved = state.focusedDay;
            switch (event.key) {
                case Key::Left: moved = Date::fromSerial(moved.serial() - 1); break;
                case Key::Right: moved = Date::fromSerial(moved.serial() + 1); break;
                case Key::Up: moved = Date::fromSerial(moved.serial() - 7); break;
                case Key::Down: moved = Date::fromSerial(moved.serial() + 7); break;
                case Key::PageUp: moved = monthShifted(moved, -1); break;
                case Key::PageDown: moved = monthShifted(moved, 1); break;
                case Key::Return:
                case Key::Space:
                    if (withinBounds(state.focusedDay, options)) {
                        result.chosen = true;
                        result.date = state.focusedDay;
                    }
                    continue;
                default: continue;
            }
            state.focusedDay = moved;
            // Walking off the edge of the month turns the page, which is what
            // makes the arrows usable rather than a trap at the boundary.
            if (moved.year != state.visible.year || moved.month != state.visible.month) {
                state.visible = Date{moved.year, moved.month, 1};
            }
        }
    }

    Style panel;
    panel.direction = Direction::Column;
    panel.gap = 8.0f;
    auto scope = ui.scope(panel);
    ui.tag(id);

    // ---- the header --------------------------------------------------------
    {
        Style header;
        header.direction = Direction::Row;
        header.align = Align::Center;
        header.gap = 4.0f;
        auto headerScope = ui.scope(header);

        button(ui, "", {.leading = Icon::ChevronLeft, .height = 26.0f, .id = previousId});
        if (input.clicked(previousId)) state.visible = monthShifted(state.visible, -1);

        {
            // Its own block, and it has to be: a `Ui::Scope` closes when it
            // leaves scope, not where it is cast to void. Without the braces
            // the *next* button below was built inside this box — centred,
            // grown and given the box's height instead of its own, which is
            // why the two arrows came out different sizes.
            Style title;
            title.grow = 1.0f;
            title.basis = 0.0f;
            title.justify = Justify::Center;
            auto titleScope = ui.scope(title);
            const std::string label =
                std::string(
                    options.locale.months[static_cast<std::size_t>(state.visible.month - 1)]) +
                " " + std::to_string(state.visible.year);
            text(ui, label,
                 {.color = Token::TextStrong, .weight = FontWeight::SemiBold, .size = 13.0f});
            (void)titleScope;
        }

        button(ui, "", {.leading = Icon::ChevronRight, .height = 26.0f, .id = nextId});
        if (input.clicked(nextId)) state.visible = monthShifted(state.visible, 1);
        (void)headerScope;
    }

    // ---- the grid ----------------------------------------------------------
    Style grid;
    grid.direction = Direction::Column;
    grid.gap = options.gap;
    auto gridScope = ui.scope(grid);
    ui.tag(gridId).focusable();

    const auto weekRow = [&] {
        Style row;
        row.direction = Direction::Row;
        row.gap = options.gap;
        return ui.scope(row);
    };

    {
        auto row = weekRow();
        for (int i = 0; i < 7; ++i) {
            const int weekday = (options.locale.firstDayOfWeek + i) % 7;
            Style cell;
            cell.width = options.cellSize;
            cell.height = 20.0f;
            cell.shrink = 0.0f;
            cell.justify = Justify::Center;
            cell.align = Align::Center;
            auto cellScope = ui.scope(cell);
            text(ui, options.locale.weekdays[static_cast<std::size_t>(weekday)],
                 {.color = Token::TextMuted, .weight = FontWeight::SemiBold, .size = 11.0f});
            (void)cellScope;
        }
        (void)row;
    }

    // The first cell is the last days of the previous month, so the 1st lands
    // under the right weekday. Those days are drawn, dimmed — a grid with holes
    // in it is harder to read than one with days you cannot pick.
    const Date first{state.visible.year, state.visible.month, 1};
    const int lead = ((weekdayOf(first) - options.locale.firstDayOfWeek) % 7 + 7) % 7;
    const long long start = first.serial() - lead;

    for (int week = 0; week < 6; ++week) {
        auto row = weekRow();
        for (int column = 0; column < 7; ++column) {
            const Date day = Date::fromSerial(start + week * 7 + column);
            const bool outside = day.month != state.visible.month;
            const bool enabled = withinBounds(day, options);
            const bool isSelected = selected.valid() && day == selected;
            const bool isFocused = day == state.focusedDay;
            const std::string dayId = std::string(id) + "." + std::to_string(day.serial());
            const bool hovered = input.isHovered(dayId) && enabled;

            Style cell;
            cell.width = options.cellSize;
            cell.height = options.cellSize;
            cell.shrink = 0.0f;
            cell.justify = Justify::Center;
            cell.align = Align::Center;
            cell.radius = 6.0f;
            if (isSelected) cell.background = Fill{Token::Accent};
            else if (hovered) cell.background = Fill{Token::SurfaceHover};
            if (isFocused && input.isFocusVisible(gridId) && !isSelected) {
                cell.outline = Outline{2.0f, 1.0f, Fill{Token::Accent}};
            }
            cell.opacity = enabled ? 1.0f : 0.4f;
            cell.cursorHint = enabled ? Cursor::Pointer : Cursor::NotAllowed;

            auto cellScope = ui.scope(cell);
            ui.tag(dayId).cursor(cell.cursorHint);
            const Token colour = isSelected ? Token::AccentFg
                                 : outside  ? Token::TextMuted
                                            : Token::Text;
            text(ui, std::to_string(day.day),
                 {.color = colour,
                  .weight = isSelected ? FontWeight::SemiBold : FontWeight::Regular,
                  .size = 12.0f});

            // Today gets a dot rather than a ring, so it cannot be mistaken for
            // the selection or for the keyboard's position.
            if (options.showToday && options.today.valid() && day == options.today && !isSelected) {
                Style dot;
                dot.position = Position::Absolute;
                dot.left = options.cellSize / 2.0f - 2.0f;
                dot.top = options.cellSize - 8.0f;
                dot.width = 4.0f;
                dot.height = 4.0f;
                dot.radius = 2.0f;
                dot.background = Fill{Token::Accent};
                ui.add(dot);
            }
            (void)cellScope;

            if (enabled && input.clicked(dayId)) {
                result.chosen = true;
                result.date = day;
                state.focusedDay = day;
                if (outside) state.visible = Date{day.year, day.month, 1};
            }
        }
        (void)row;
    }
    (void)gridScope;
    (void)scope;

    return result;
}

DateFieldResult dateField(Ui& ui, const Interaction& input, std::string_view id,
                          const Date& selected, DatePickerState& state,
                          const DateFieldOptions& options) {
    DateFieldResult result;
    result.date = selected;

    const std::string triggerId = std::string(id) + ".trigger";
    const std::string clearId = std::string(id) + ".clear";
    const bool hasValue = selected.valid();

    Style trigger;
    trigger.direction = Direction::Row;
    trigger.align = Align::Center;
    trigger.gap = 8.0f;
    trigger.width = options.width;
    trigger.minHeight = options.height;
    trigger.padding = Edges::symmetric(0.0f, 10.0f);
    trigger.radius = ui.design().controlRadius;
    const FieldPalette palette = paletteForField(options.disabled, false, input.isHovered(triggerId));
    trigger.background = palette.background;
    trigger.border = Border{1.0f, Fill{palette.border}};
    if (input.isFocusVisible(triggerId)) trigger.outline = Outline{2.0f, 2.0f, Fill{Token::Accent}};
    trigger.opacity = opacityFor(options.disabled);
    trigger.cursorHint = options.disabled ? Cursor::NotAllowed : Cursor::Pointer;

    {
        auto scope = ui.scope(trigger);
        ui.tag(triggerId).focusable(!options.disabled).cursor(trigger.cursorHint);
        icon(ui, Icon::ClockFading, {.color = Token::TextMuted, .size = 14.0f});
        // The placeholder is muted and the value is not, which is the whole of
        // how a reader tells "nothing chosen" from "chosen".
        text(ui,
             hasValue ? formatDate(selected, options.pattern, options.locale)
                      : std::string(options.placeholder),
             {.color = hasValue ? Token::Text : Token::TextMuted, .size = 12.0f, .grow = 1.0f});
        if (options.clearable && hasValue && !options.disabled) {
            Style clear;
            clear.width = 18.0f;
            clear.height = 18.0f;
            clear.shrink = 0.0f;
            clear.radius = 4.0f;
            clear.justify = Justify::Center;
            clear.align = Align::Center;
            if (input.isHovered(clearId)) clear.background = Fill{Token::SurfaceHover};
            clear.cursorHint = Cursor::Pointer;
            auto clearScope = ui.scope(clear);
            ui.tag(clearId).cursor(Cursor::Pointer);
            icon(ui, Icon::X, {.color = Token::TextMuted, .size = 11.0f});
            (void)clearScope;
        } else {
            icon(ui, Icon::ChevronDown, {.color = Token::TextMuted, .size = 13.0f});
        }
        (void)scope;
    }

    if (options.disabled) return result;
    if (input.clicked(clearId)) {
        result.date = Date{};
        result.changed = true;
        return result;
    }
    if (activated(input, triggerId, false)) state.open = !state.open;
    if (!state.open) return result;

    PopoverOptions popoverOptions;
    popoverOptions.placement = Placement::Bottom;
    popoverOptions.minWidth = 240.0f;
    popoverOptions.maxWidth = 320.0f;
    popoverOptions.padding = Edges::all(10.0f);
    // Scrolls inside rather than overflowing. `popover` already works out
    // how much room there is and caps the box at it; without a scroll that cap
    // is just a clip, and a calendar opened near the bottom of the window loses
    // its last week with no way to reach it.
    popoverOptions.scroll = ScrollAxis::Vertical;

    auto surface = popover(ui, input, std::string(id) + ".popover", triggerId, popoverOptions);
    const DatePickerResult picked =
        datePicker(ui, input, std::string(id) + ".calendar", selected, state, options);
    if (picked.chosen) {
        result.date = picked.date;
        result.changed = true;
        // Choosing is the whole point of opening it, so it closes itself.
        state.open = false;
    }
    (void)surface;

    if (options.dismissOnOutsideClick && input.pointerDown() && input.dragging().empty()) {
        state.open = false;
    }
    if (options.dismissOnEscape) {
        for (const KeyEvent& event : input.keys()) {
            if (event.key == Key::Escape) state.open = false;
        }
    }
    return result;
}

}  // namespace gbui
