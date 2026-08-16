#include "gbui/widgets/progress.hpp"

#include <algorithm>
#include <cmath>

namespace gbui {

NodeId progressBar(Ui& ui, const ProgressOptions& options) {
    Style track;
    track.width = options.width;
    track.grow = options.grow;
    track.height = options.height;
    track.radius = options.height / 2.0f;
    track.background = Fill{Token::BgOverlay};
    track.overflow = Overflow::Hidden;
    track.align = Align::Center;

    auto scope = ui.scope(track);
    // An indeterminate bar reports `busy` and no value at all, which is exactly
    // what it means: something is happening and nobody knows how much of it is
    // left. Reporting zero instead would announce "0 percent" forever.
    const bool determinate = options.value >= 0.0;
    ui.accessible({
        .role = Role::ProgressBar,
        .name = options.name,
        .state = {.busy = flag(!determinate)},
        .value = {.present = determinate,
                  .now = std::clamp(options.value, 0.0, 1.0),
                  .minimum = 0.0,
                  .maximum = 1.0},
    });

    Style fill;
    fill.height = options.height;
    fill.radius = options.height / 2.0f;
    fill.background = Fill{options.color};
    if (options.value >= 0.0) {
        fill.grow = 0.0f;
        // A determinate bar is a fraction of the track, so the fraction is the
        // basis and the remainder is a spacer.
        fill.basis = 0.0f;
        fill.grow = static_cast<float>(std::clamp(options.value, 0.0, 1.0));
        ui.add(fill);
        Style rest;
        rest.grow = 1.0f - fill.grow;
        rest.basis = 0.0f;
        ui.add(rest);
    } else {
        // Indeterminate: a short bar sliding along the track. The phase comes
        // from the caller, because the toolkit has no clock of its own yet.
        const float sweep = std::fmod(std::fabs(options.phase), 1.0f);
        Style before;
        before.basis = 0.0f;
        before.grow = std::max(0.0f, sweep - 0.15f);
        ui.add(before);
        fill.basis = 0.0f;
        fill.grow = 0.3f;
        ui.add(fill);
        Style after;
        after.basis = 0.0f;
        after.grow = std::max(0.0f, 1.0f - sweep - 0.15f);
        ui.add(after);
    }
    (void)scope;
    return scope.id();
}

}  // namespace gbui
