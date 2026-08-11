// What a design system decides beyond its colours.
//
// A theme is 24 colours and a type scale. That is not a design system. Material,
// Cupertino and Fluent differ in colour, yes, but they differ far more in
// *shape and behaviour*: how round a control is, how tall, whether a press
// throws ink from the point you touched, how fast anything moves and on what
// curve. Repainting Material's palette onto square controls with no press
// feedback does not produce Material; it produces the same toolkit in purple.
//
// So those decisions live here, beside the palette rather than inside every
// call site. A component asks the active design what a press looks like instead
// of taking a `ripple` flag from whoever wrote the screen — which is the
// difference between a design system and a pile of options.
#pragma once

#include <string>
#include <vector>

#include "gbui/anim/animator.hpp"
#include "gbui/style/theme.hpp"

namespace gbui {

/** How a control acknowledges a press. */
enum class PressFeedback {
    /** The surface changes and nothing else — the desktop default. */
    Surface,
    /** Ink grows from the point pressed. Material's, and only Material's. */
    Ripple,
};

/**
 * What every chart looks like unless it says otherwise.
 *
 * Charts have more knobs than any other component, and setting them per call
 * site is how a dashboard ends up with six charts that disagree about tick
 * counts and line weights. So the defaults live here, with the rest of the
 * design, and an individual chart overrides only what it must.
 */
/** When a chart's readout appears. */
enum class ChartTrigger {
    /** While the pointer is over the plot. */
    Hover,
    /** After a click, and until the next one elsewhere — which is what a reader
     *  on a touchpad or a touchscreen needs, and what lets them read a long
     *  readout without keeping the pointer still. */
    Click,
    /** Never. The caller is drawing its own readout from `ChartResult`. */
    None,
};

/** How a chart's readout looks, for the whole application at once. */
struct ChartTooltipStyle {
    ChartTrigger trigger = ChartTrigger::Hover;
    Token background = Token::BgOverlay;
    Token border = Token::Border;
    float radius = 6.0f;
    float padding = 8.0f;
    float fontSize = 11.0f;
    /** A swatch of the series colour beside each row. */
    bool swatches = true;
    /** Name the category above the values. */
    bool showCategory = true;
};

struct ChartStyle {
    float lineThickness = 1.5f;
    /** Shading under a line, when a series does not ask for its own. */
    float fillAlpha = 0.0f;
    bool grid = true;
    int tickCount = 4;
    float axisWidth = 34.0f;

    float donutThickness = 0.42f;
    float donutPadAngle = 1.5f;

    /**
     * The colours series cycle through when they do not name one.
     *
     * Tokens rather than colours, so a chart re-themes with everything else —
     * which is the difference between a chart that belongs to the application
     * and one that was pasted into it.
     */
    std::vector<Token> palette{Token::Graph1, Token::Graph2, Token::Graph3, Token::Graph4,
                               Token::Graph5, Token::Graph6, Token::Graph7, Token::Graph8};

    ChartTooltipStyle tooltip{};
};

struct Design {
    std::string id = "gitbox";
    std::string name = "GitBox";

    /** Corner rounding for controls. The theme's `Typography::radius` stays the
     *  authority for panels and cards; this is the one controls use, because
     *  the two are not the same number in every system — Cupertino rounds a
     *  button far more than it rounds a pane. */
    float controlRadius = 6.0f;
    /** The height a button, field or select takes when nothing overrides it. */
    float controlHeight = 30.0f;
    /** Border thickness on controls that have one. Fluent draws a hairline;
     *  Material mostly draws none and leans on fill. */
    float borderWidth = 1.0f;

    PressFeedback press = PressFeedback::Surface;
    /** Alpha of the ripple's ink, when there is one. */
    float rippleAlpha = 0.20f;

    // ---- the controls that give a design system away ---------------------
    //
    // Radius and height are not enough to tell these apart. What a person
    // recognises at a glance is the *switch*: Material's is 52x32 with a thumb
    // that grows when it turns on, iOS's is 51x31 with a thumb nearly filling
    // it, and Fluent's is a 40x20 pill with a small dot. Getting those numbers
    // right is most of the difference between "themed" and "faithful", so they
    // are part of the design rather than constants in a component.

    float switchWidth = 40.0f;
    float switchHeight = 22.0f;
    /** The thumb when off. */
    float switchKnob = 14.0f;
    /** The thumb when on. Material grows it; iOS and Fluent do not. */
    float switchKnobOn = 14.0f;

    float checkboxSize = 16.0f;
    float checkboxRadius = 4.0f;
    float radioSize = 16.0f;

    /** The wash a hovered or pressed surface takes, as Material's "state
     *  layer" and every other system's quieter equivalent. */
    float hoverAlpha = 0.08f;

    /** The default transition everything animated uses, so switching design
     *  changes the *feel* and not only the look. */
    Transition motion{};

    /** Chart defaults, shared by every chart the application draws. */
    ChartStyle chart{};

    /** Whether a focus ring is drawn outside the control (desktop) or the
     *  control recolours instead. Kept as a flag rather than a policy object
     *  because there are only these two answers today. */
    bool outlineFocusRing = true;

    static Design gitbox();
    /** Material 3: large radii, tall targets, ink on press, emphasised easing. */
    static Design material();
    /** Cupertino: very round, light borders, no ink, a spring on the way in. */
    static Design cupertino();
    /** Fluent: small radii, a visible hairline, short and brisk motion. */
    static Design fluent();
};

}  // namespace gbui
