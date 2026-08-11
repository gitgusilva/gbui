// A number, with step buttons and the wheel.
#pragma once

#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"

namespace gbui {

/** Where the step buttons go.
 *
 * `Sides` reads best when there is room. `Stacked` is the classic spin box —
 * two half-height arrows on one side — and is what a narrow field wants,
 * because the value keeps the whole width instead of sharing it with two
 * buttons. `None` leaves the wheel and the arrow keys. */
enum class StepperPlacement { Sides, Stacked, None };

struct NumberFieldOptions {
    double minimum = -1e18;
    double maximum = 1e18;
    double step = 1.0;
    /** Digits after the point, for both display and rounding. */
    int decimals = 0;
    std::string_view suffix{};   ///< "px", "%", " ms"
    bool disabled = false;
    bool readOnly = false;
    StepperPlacement steppers = StepperPlacement::Sides;
    /** Zero takes the active design's control height. */
    float height = 0.0f;
    float width = 120.0f;
    /** Below this the steppers are dropped to `Stacked` automatically, so a
     *  field in a narrow column shows its value rather than two buttons. */
    float stackedBelow = 110.0f;
};

struct NumberFieldResult {
    double value = 0.0;
    bool changed = false;
};

/** The value is clamped and rounded before it is returned, so a caller never
 *  sees one outside the range. */
[[nodiscard]] NumberFieldResult numberField(Ui& ui, const Interaction& input, std::string_view id, double value,
                              const NumberFieldOptions& options = {});

}  // namespace gbui
