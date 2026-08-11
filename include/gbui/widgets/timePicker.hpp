// A time of day, chosen from columns of hours and minutes.
#pragma once

#include <string>
#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/scroll.hpp"

namespace gbui {

/** A time of day, with no date and no zone. */
struct Time {
    int hour = 0;    ///< 0..23, always — twelve-hour is a *display*, not a value
    int minute = 0;  ///< 0..59
    int second = 0;  ///< 0..59

    bool valid() const {
        return hour >= 0 && hour < 24 && minute >= 0 && minute < 60 && second >= 0 && second < 60;
    }
    /** Seconds since midnight, so two times compare and step without anyone
     *  writing modular arithmetic again. */
    int secondsOfDay() const { return hour * 3600 + minute * 60 + second; }
    static Time fromSeconds(int seconds);

    friend bool operator==(const Time& a, const Time& b) {
        return a.hour == b.hour && a.minute == b.minute && a.second == b.second;
    }
    friend bool operator!=(const Time& a, const Time& b) { return !(a == b); }
    friend bool operator<(const Time& a, const Time& b) {
        return a.secondsOfDay() < b.secondsOfDay();
    }
};

/** The words a clock needs. Separate from `CalendarLocale` because a time and a
 *  date are chosen independently and a caller may want only one. */
struct ClockLocale {
    std::string_view am = "AM";
    std::string_view pm = "PM";
};

/**
 * Formats a time by a pattern, in the same grammar `formatDate` uses.
 *
 * | | |
 * |---|---|
 * | `HH` `H` | hour, 0–23, padded or not |
 * | `hh` `h` | hour, 1–12 |
 * | `mm` `m` | minute |
 * | `ss` `s` | second |
 * | `a`      | `AM` or `PM`, from the locale |
 *
 * Text in single quotes is copied verbatim, so `HH'h'mm` gives `14h30`.
 */
std::string formatTime(const Time& time, std::string_view pattern,
                       const ClockLocale& locale = {});

/** Where each column is scrolled to. Owned by the application. */
struct TimePickerState {
    ScrollState hours{};
    ScrollState minutes{};
    ScrollState seconds{};
    /** The value each column was last scrolled *to*, so the picker only forces
     *  a column into view when the selection moved. Without it, every frame
     *  drags the column back and the reader cannot scroll it at all. */
    int shownHour = -1;
    int shownMinute = -1;
    int shownSecond = -1;
};

struct TimePickerOptions {
    /** Twelve-hour display with an AM/PM column. The *value* is always 0–23. */
    bool use24Hour = true;
    bool showSeconds = false;
    /** Coarser columns: 5 or 15 for a picker nobody needs to the minute. */
    int minuteStep = 1;
    int secondStep = 1;
    /** Times outside these cannot be chosen. Leave both at midnight for none —
     *  a range of nothing is not a range anybody wants. */
    Time minimum{0, 0, 0};
    Time maximum{0, 0, 0};
    ClockLocale locale{};
    float height = 176.0f;
    float columnWidth = 58.0f;
    float rowHeight = 28.0f;
};

struct TimePickerResult {
    bool changed = false;
    Time time{};
};

/**
 * Columns of hours, minutes and — when asked for — seconds, each scrolled to
 * its own value.
 *
 * Columns rather than steppers because a time is picked far more often than it
 * is nudged: "quarter past two" is two glances, and two number fields make it
 * two edits. Each column keeps its selection in view, so opening the picker on
 * 23:45 does not show midnight.
 */
TimePickerResult timePicker(Ui& ui, const Interaction& input, std::string_view id,
                            const Time& selected, TimePickerState& state,
                            const TimePickerOptions& options = {});

}  // namespace gbui
