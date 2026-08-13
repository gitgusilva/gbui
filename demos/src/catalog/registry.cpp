#include "gbui_demos/catalog.hpp"

#include <algorithm>
#include <cmath>

#include "catalog.hpp"

namespace gbui::demos::catalog {

State freshState() {
    State state;
    // Two smooth series, filled once. Fixed rather than simulated: a chart
    // that repaints with different numbers every second is demonstrating the
    // animation, not the chart, and a screenshot of the gallery would never
    // match twice.
    state.series.reserve(48);
    state.other.reserve(48);
    for (int i = 0; i < 48; ++i) {
        const float t = static_cast<float>(i) * 0.35f;
        state.series.push_back(30.0 + 12.0 * std::sin(t) + 5.0 * std::sin(t * 2.7f));
        state.other.push_back(28.0 + 9.0 * std::sin(t * 0.8f + 1.2f));
    }

    // A picture, drawn rather than loaded: two colours on a diagonal, with a
    // hole punched through the middle so the example shows what an image with
    // real transparency does over what is behind it.
    constexpr int kSide = 64;
    state.pictureSide = kSide;
    state.picture.assign(static_cast<std::size_t>(kSide) * kSide * 4, 0);
    for (int y = 0; y < kSide; ++y) {
        for (int x = 0; x < kSide; ++x) {
            const float u = static_cast<float>(x) / (kSide - 1);
            const float v = static_cast<float>(y) / (kSide - 1);
            const float ring = std::hypot(static_cast<float>(x) - 32.0f,
                                          static_cast<float>(y) - 32.0f);
            std::uint8_t* p =
                state.picture.data() +
                (static_cast<std::size_t>(y) * kSide + static_cast<std::size_t>(x)) * 4;
            p[0] = static_cast<std::uint8_t>(40.0f + 200.0f * u);
            p[1] = static_cast<std::uint8_t>(90.0f + 120.0f * (1.0f - v));
            p[2] = static_cast<std::uint8_t>(230.0f - 120.0f * u);
            p[3] = ring < 11.0f ? 0 : 255;
        }
    }
    return state;
}

const std::vector<Example>& examples() {
    static const std::vector<Example> table = [] {
        std::vector<Example> out;
        out.reserve(52);
        // The same order the metadata groups them in, so a reader moving
        // between the two never has to translate.
        addComponentExamples(out);
        addContainerExamples(out);
        addControlExamples(out);
        addOverlayExamples(out);
        addChartExamples(out);
        return out;
    }();
    return table;
}

const Example* find(std::string_view component) {
    const auto& all = examples();
    const auto match = std::find_if(all.begin(), all.end(), [&](const Example& example) {
        return example.component == component;
    });
    return match == all.end() ? nullptr : &*match;
}

}  // namespace gbui::demos::catalog
