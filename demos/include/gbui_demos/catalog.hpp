// One live example per component.
//
// `gbui::meta` says what the component set *is* — every option, its type, its
// default, its documentation — and none of that can be turned back into a
// call, because C++ has no reflection. So the running half is written by hand,
// once per component, and the two are held together by a coverage check rather
// than by good intentions: `gbui_demo --coverage` fails when a component in
// the metadata has no example here.
//
// **Demonstration material, like everything else under `demos/`.** Not
// installed, no API stability, nothing outside this directory links it.
//
// An example is a function, not a class, and they all share one bag of state.
// A checkbox example that had to define a struct to hold one `bool` would be
// three lines of ceremony around one line of point.
#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "gbui/input/interaction.hpp"
#include "gbui/input/textEdit.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/chart.hpp"
#include "gbui/widgets/components.hpp"
#include "gbui/widgets/containers.hpp"
#include "gbui/widgets/controls.hpp"
#include "gbui/widgets/overlays.hpp"

namespace gbui::demos::catalog {

/**
 * Everything the examples keep between frames, in one place.
 *
 * Shared rather than one struct per example, and deliberately: the toolkit's
 * whole contract is that a component holds no state and the application does,
 * so an example that did not carry its own value would be demonstrating the
 * wrong thing. What it does not need is fifty structs to say so.
 *
 * Reset when the reader picks a different component, so an example always
 * opens in the state its author chose.
 */
struct State {
    bool on = true;
    bool off = false;
    std::size_t choice = 1;
    std::size_t tab = 0;
    double fraction = 0.62;
    float clock = 0.0f;
    /** Seconds since the previous frame, for anything that advances. */
    float delta = 0.0f;
    /** Where the marquee example has slid to. It stops while it is hovered. */
    MarqueeState marquee{};
    /** The toast queue. Seeded rather than empty, because an example that
     *  shows nothing until the reader presses something shows nothing. */
    ToastState toasts{};
    /** Where the before-and-after seam has been left. */
    float seam = 0.55f;
    /** Which slide the carousel is on, and whether it is still playing. */
    CarouselState carousel{};

    TextEditState text{"themes/nord", 11, 11};
    /** A number box's state is a string like any other input's: while it has
     *  the keyboard the text is the value, and `42.` is on the way to one. */
    TextEditState minutes{"42", 2, 2};
    TextEditState secret{"hunter2", 7, 7};
    TextareaState note{};
    /** What the `field` example is complaining about, so the example shows the
     *  invalid state rather than only describing it. */
    std::string_view fieldError{};
    RichDocument document{};
    RichEditorState rich{};

    ScrollState scroll{};
    ScrollState scroll2{};
    ScrollState listScroll{};
    TableState table{};
    SelectState select{};
    DonutState donut{};
    ChartView view{};

    ColorPickerState colour{};
    Date date{2026, 8, 12};
    DatePickerState calendar{};
    Time time{14, 30, 0};
    TimePickerState clock2{};
    DateTime stamp{Date{2026, 8, 12}, Time{14, 30, 0}};
    DateTimeFieldState stampState{};

    bool modalOpen = false;
    Vec2 modalAt{};
    bool popoverOpen = true;

    /** The values a chart example plots. Filled once, so a still of the
     *  gallery is the same picture on every machine. */
    std::vector<double> series{};
    std::vector<double> other{};

    /**
     * Pixels for the `image` example, drawn into rather than loaded.
     *
     * The gallery runs in a browser and in a headless CI job, neither of which
     * has a file to read — and the toolkit decodes nothing anyway. Generating
     * them is what makes the example honest: it shows the element taking the
     * eight-bit RGBA an application's own decoder would hand it.
     */
    std::vector<std::uint8_t> picture{};
    int pictureSide = 0;
    /** Four more of the same, in different hues, for the gallery. Drawn rather
     *  than loaded for the reason above: the catalogue decodes nothing, and a
     *  screenshot has to match on every machine. */
    std::vector<std::vector<std::uint8_t>> plates{};
    int plateSide = 0;
    GalleryState gallery{};
};

/** One component, and the smallest honest use of it. */
struct Example {
    /** Matches `gbui::meta::ComponentInfo::name`, which is what the coverage
     *  check compares and what the documentation looks it up by. */
    std::string_view component;
    /**
     * Builds it into whatever box the host provides.
     *
     * A plain function pointer rather than a `std::function`: an example
     * captures nothing — everything it needs is the `State` it is handed — and
     * a table of fifty type-erased closures is a lot of machinery for that.
     */
    void (*build)(Ui& ui, const Interaction& input, State& state);
};

/** Every example, in the order the metadata lists its components. */
const std::vector<Example>& examples();

/** One by component name, or nullptr. */
const Example* find(std::string_view component);

/** A fresh `State` with the series a chart example expects already filled. */
State freshState();

}  // namespace gbui::demos::catalog
