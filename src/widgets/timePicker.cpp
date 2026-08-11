#include "gbui/widgets/timePicker.hpp"

#include <algorithm>
#include <vector>

#include "detail.hpp"
#include "gbui/widgets/listRow.hpp"
#include "gbui/widgets/text.hpp"
#include "gbui/widgets/virtualList.hpp"

namespace gbui {

// What this shares with its siblings, rather than a copy in each.
using namespace detail;

namespace {

std::string padded(int value) {
    return (value < 10 ? "0" : "") + std::to_string(value);
}

/** Whether a range was actually asked for. Both ends at midnight means "no
 *  bounds", which is the only sentinel a type with no invalid state can carry. */
bool bounded(const TimePickerOptions& options) {
    return options.minimum != options.maximum;
}

bool allowed(const Time& time, const TimePickerOptions& options) {
    if (!bounded(options)) return true;
    if (options.minimum < options.maximum) {
        return !(time < options.minimum) && !(options.maximum < time);
    }
    // A range that wraps midnight — 22:00 to 06:00 — is two ranges, and saying
    // so is better than quietly refusing every time in it.
    return !(time < options.minimum) || !(options.maximum < time);
}

}  // namespace

Time Time::fromSeconds(int seconds) {
    const int wrapped = ((seconds % 86400) + 86400) % 86400;
    return Time{wrapped / 3600, wrapped % 3600 / 60, wrapped % 60};
}

std::string formatTime(const Time& time, std::string_view pattern, const ClockLocale& locale) {
    std::string out;
    if (!time.valid()) return out;

    const auto pad = [](int value, std::size_t width) {
        std::string digits = std::to_string(value);
        while (digits.size() < width) digits.insert(digits.begin(), '0');
        return digits;
    };
    const auto run = [&](std::size_t at) {
        std::size_t n = 1;
        while (at + n < pattern.size() && pattern[at + n] == pattern[at]) ++n;
        return n;
    };

    for (std::size_t i = 0; i < pattern.size();) {
        const char c = pattern[i];
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
            case 'H': out += pad(time.hour, count); break;
            case 'h': {
                // Twelve-hour counts 12, 1..11 — midnight and noon are both 12,
                // which is the one case a modulo gets wrong on its own.
                const int twelve = time.hour % 12 == 0 ? 12 : time.hour % 12;
                out += pad(twelve, count);
                break;
            }
            case 'm': out += pad(time.minute, count); break;
            case 's': out += pad(time.second, count); break;
            case 'a': out += time.hour < 12 ? locale.am : locale.pm; break;
            default: out.append(count, c); break;
        }
        i += count;
    }
    return out;
}

TimePickerResult timePicker(Ui& ui, const Interaction& input, std::string_view id,
                            const Time& selected, TimePickerState& state,
                            const TimePickerOptions& options) {
    TimePickerResult result;
    result.time = selected.valid() ? selected : Time{};

    Style row;
    row.direction = Direction::Row;
    row.gap = 6.0f;
    row.height = options.height;
    auto scope = ui.begin(row);
    ui.tag(id);

    /** One scrolling column of values, with the chosen one kept in view. */
    const auto column = [&](std::string_view part, ScrollState& scroll, int& lastShown,
                            const std::vector<int>& values, int current,
                            const auto& labelFor, const auto& onPick) {
        const std::string columnId = std::string(id) + "." + std::string(part);

        // Scrolled to the selection only when the selection *moved*.
        //
        // Doing it every frame is the bug it looks like: the column snaps back
        // the instant the reader scrolls it, because the picker and the pointer
        // are fighting over the same offset. A column is theirs to move once
        // the value is on screen.
        if (current != lastShown) {
            const auto at = std::find(values.begin(), values.end(), current);
            if (at != values.end()) {
                revealRow(scroll, RowMetrics{options.rowHeight, 2.0f, 4.0f},
                          static_cast<std::size_t>(std::distance(values.begin(), at)));
            }
            lastShown = current;
        }

        Style frame;
        frame.width = options.columnWidth;
        frame.shrink = 0.0f;
        frame.border = Border{1.0f, Fill{Token::Border}};
        frame.radius = 6.0f;
        auto frameScope = ui.begin(frame);

        ScrollOptions view;
        view.axis = ScrollAxis::Vertical;
        view.gap = 2.0f;
        view.padding = Edges::all(4.0f);
        view.scrollbarWidth = 6.0f;
        auto scrollScope = beginScroll(ui, input, columnId, scroll, view);

        for (std::size_t i = 0; i < values.size(); ++i) {
            const int value = values[i];
            const std::string rowId = columnId + "." + std::to_string(value);
            Time candidate = result.time;
            onPick(candidate, value);
            const bool enabled = allowed(candidate, options);
            const bool chosen = value == current;

            Style cell;
            cell.direction = Direction::Row;
            cell.justify = Justify::Center;
            cell.align = Align::Center;
            cell.height = options.rowHeight;
            cell.shrink = 0.0f;
            cell.radius = 4.0f;
            if (chosen) cell.background = Fill{Token::Accent};
            else if (input.isHovered(rowId) && enabled) cell.background = Fill{Token::SurfaceHover};
            cell.opacity = enabled ? 1.0f : 0.35f;
            cell.cursorHint = enabled ? Cursor::Pointer : Cursor::NotAllowed;

            auto cellScope = ui.begin(cell);
            ui.tag(rowId).cursor(cell.cursorHint);
            text(ui, labelFor(value),
                 {.color = chosen ? Token::AccentFg : Token::Text,
                  .weight = chosen ? FontWeight::SemiBold : FontWeight::Regular,
                  .role = FontRole::Mono, .size = 12.0f});
            (void)cellScope;

            if (enabled && input.clicked(rowId)) {
                onPick(result.time, value);
                result.changed = true;
            }
        }
        (void)scrollScope;
        (void)frameScope;
    };

    // ---- hours -------------------------------------------------------------
    std::vector<int> hours;
    if (options.use24Hour) {
        for (int h = 0; h < 24; ++h) hours.push_back(h);
    } else {
        // 12, 1, 2 … 11 — the order a clock face reads in.
        hours.push_back(12);
        for (int h = 1; h < 12; ++h) hours.push_back(h);
    }
    const int shownHour = options.use24Hour ? result.time.hour
                          : result.time.hour % 12 == 0 ? 12
                                                       : result.time.hour % 12;
    column("hour", state.hours, state.shownHour, hours, shownHour,
           [&](int value) { return options.use24Hour ? padded(value) : std::to_string(value); },
           [&](Time& time, int value) {
               if (options.use24Hour) {
                   time.hour = value;
               } else {
                   // The value is always 0–23: twelve-hour is a display, so
                   // picking "3" keeps whichever half of the day it was in.
                   const bool afternoon = time.hour >= 12;
                   const int base = value % 12;
                   time.hour = afternoon ? base + 12 : base;
               }
           });

    // ---- minutes and seconds -----------------------------------------------
    const auto range = [](int step) {
        std::vector<int> out;
        for (int v = 0; v < 60; v += std::max(1, step)) out.push_back(v);
        return out;
    };
    column("minute", state.minutes, state.shownMinute, range(options.minuteStep),
           result.time.minute,
           [](int value) { return padded(value); },
           [](Time& time, int value) { time.minute = value; });

    if (options.showSeconds) {
        column("second", state.seconds, state.shownSecond, range(options.secondStep),
               result.time.second,
               [](int value) { return padded(value); },
               [](Time& time, int value) { time.second = value; });
    }

    // ---- the half of the day -----------------------------------------------
    if (!options.use24Hour) {
        Style half;
        half.direction = Direction::Column;
        half.gap = 4.0f;
        half.width = options.columnWidth;
        half.shrink = 0.0f;
        auto halfScope = ui.begin(half);
        for (int side = 0; side < 2; ++side) {
            const bool afternoon = side == 1;
            const std::string sideId = std::string(id) + (afternoon ? ".pm" : ".am");
            const bool chosen = (result.time.hour >= 12) == afternoon;

            Style cell;
            cell.direction = Direction::Row;
            cell.justify = Justify::Center;
            cell.align = Align::Center;
            cell.height = options.rowHeight;
            cell.shrink = 0.0f;
            cell.radius = 4.0f;
            cell.border = Border{1.0f, Fill{Token::Border}};
            if (chosen) cell.background = Fill{Token::Accent};
            else if (input.isHovered(sideId)) cell.background = Fill{Token::SurfaceHover};
            cell.cursorHint = Cursor::Pointer;
            auto cellScope = ui.begin(cell);
            ui.tag(sideId).cursor(Cursor::Pointer);
            text(ui, afternoon ? options.locale.pm : options.locale.am,
                 {.color = chosen ? Token::AccentFg : Token::Text,
                  .weight = FontWeight::SemiBold, .size = 11.0f});
            (void)cellScope;

            if (input.clicked(sideId) && !chosen) {
                result.time.hour = afternoon ? result.time.hour + 12 : result.time.hour - 12;
                result.changed = true;
            }
        }
        (void)halfScope;
    }
    (void)scope;

    return result;
}

}  // namespace gbui
