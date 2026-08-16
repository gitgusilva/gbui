#include "gbui/anim/animator.hpp"

#include "gbui/input/interaction.hpp"
#include "gbui/layout/layout.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/style/theme.hpp"
#include "gbui/widgets/controls.hpp"
#include "harness.hpp"

using namespace gbui;

namespace {

constexpr Transition kLinear{.duration = 1.0f, .easing = Easing::Linear};

/** Runs the clock for `seconds` in frames a real loop would deliver. Ticking
 *  once with a huge delta is not the same thing: the animator deliberately caps
 *  a single step, so that a window that was dragged does not finish every
 *  animation at once. */
void advance(Animator& animator, float seconds) {
    constexpr float kFrame = 0.05f;
    for (float elapsed = 0.0f; elapsed + 1e-5f < seconds; elapsed += kFrame) {
        animator.tick(kFrame);
    }
}

float step(Animator& animator, float seconds, float target,
           const Transition& transition = kLinear) {
    advance(animator, seconds);
    return animator.animate("id", "value", target, transition);
}

}  // namespace

TEST("a value is its target the first time it is seen") {
    Animator animator;
    // Nothing slides into place on the frame it appears — a window that opens
    // with three switches already on does not play three animations.
    CHECK_NEAR(animator.animate("id", "value", 5.0f, kLinear), 5.0f);
    CHECK(!animator.animating());
}

TEST("a changed target is travelled to over the duration") {
    Animator animator;
    animator.animate("id", "value", 0.0f, kLinear);

    // The frame the target changes still reports the old value: the move has
    // not been ticked yet.
    CHECK_NEAR(animator.animate("id", "value", 1.0f, kLinear), 0.0f);
    CHECK_NEAR(step(animator, 0.25f, 1.0f), 0.25f);
    CHECK_NEAR(step(animator, 0.25f, 1.0f), 0.5f);
    CHECK(animator.animating());
    CHECK_NEAR(step(animator, 0.5f, 1.0f), 1.0f);
    // It lands exactly on the target rather than near it, and stops running.
    CHECK(!animator.animating());
}

TEST("a target that changes mid-flight is picked up from where the value is") {
    Animator animator;
    animator.animate("id", "value", 0.0f, kLinear);
    animator.animate("id", "value", 1.0f, kLinear);
    CHECK_NEAR(step(animator, 0.5f, 1.0f), 0.5f);

    // Reversed halfway: it goes back from 0.5, not from 1.
    animator.animate("id", "value", 0.0f, kLinear);
    CHECK_NEAR(step(animator, 0.5f, 0.0f), 0.25f);
    CHECK_NEAR(step(animator, 0.5f, 0.0f), 0.0f);
}

TEST("a zero duration is an immediate change") {
    Animator animator;
    const Transition instant{.duration = 0.0f};
    animator.animate("id", "value", 0.0f, instant);
    CHECK_NEAR(animator.animate("id", "value", 1.0f, instant), 1.0f);
    CHECK(!animator.animating());
}

TEST("easing shapes the travel without moving the ends") {
    Animator eased;
    const Transition curve{.duration = 1.0f, .easing = Easing::EaseOut};
    eased.animate("id", "value", 0.0f, curve);
    eased.animate("id", "value", 1.0f, curve);

    // Ease-out is past halfway at the halfway point — that is what makes it
    // read as responding rather than as hesitating.
    const float middle = step(eased, 0.5f, 1.0f, curve);
    CHECK(middle > 0.5f);
    CHECK(middle < 1.0f);
    CHECK_NEAR(step(eased, 0.5f, 1.0f, curve), 1.0f);
}

TEST("properties of the same control do not collide") {
    Animator animator;
    animator.animate("switch", "on", 0.0f, kLinear);
    animator.animate("switch", "hover", 0.0f, kLinear);
    animator.animate("switch", "on", 1.0f, kLinear);
    advance(animator, 0.5f);

    CHECK_NEAR(animator.animate("switch", "on", 1.0f, kLinear), 0.5f);
    CHECK_NEAR(animator.animate("switch", "hover", 0.0f, kLinear), 0.0f);
}

TEST("a colour travels channel by channel") {
    Animator animator;
    const Color from{0, 0, 0, 1.0f};
    const Color to{100, 200, 40, 0.0f};
    animator.animate("id", "fill", from, kLinear);
    animator.animate("id", "fill", to, kLinear);

    advance(animator, 0.5f);
    const Color half = animator.animate("id", "fill", to, kLinear);
    CHECK_EQ(static_cast<int>(half.r), 50);
    CHECK_EQ(static_cast<int>(half.g), 100);
    CHECK_EQ(static_cast<int>(half.b), 20);
    CHECK_NEAR(half.a, 0.5f);
}

TEST("an enormous delta does not finish everything at once") {
    Animator animator;
    animator.animate("id", "value", 0.0f, kLinear);
    animator.animate("id", "value", 1.0f, kLinear);

    // A breakpoint, a drag, or a machine asleep. Running slightly slow beats
    // teleporting every animation on the frame the window wakes up.
    animator.tick(30.0f);
    CHECK(animator.animate("id", "value", 1.0f, kLinear) < 1.0f);
}

TEST("a key nobody asks about any more is forgotten") {
    Animator animator;
    animator.animate("row", "hover", 0.0f, kLinear);
    CHECK_EQ(animator.size(), std::size_t{1});

    // A row scrolled out of a virtualised list stops being built; its state
    // must not accumulate for the life of the process.
    for (int frame = 0; frame < 200; ++frame) animator.tick(0.016f);
    CHECK_EQ(animator.size(), std::size_t{0});
}

// ---- the switch, as the first component to use it --------------------------

namespace {

/** Builds the switch and reports where its knob sits inside the track. */
float knobTravel(Animator& animator, bool on) {
    Arena arena;
    Ui ui(arena);
    Theme theme = Theme::dark();
    Interaction input;
    ui.setAnimator(&animator);

    {
        auto column = ui.column({.width = 200.0f});
        (void)toggle(ui, input, "s", on, {});
        (void)column;
    }
    LayoutContext context;
    context.theme = &theme;
    layout(arena, ui.root(), Rect{0, 0, 200, 100}, context);

    // The knob is the last node the switch adds inside its track, and it is
    // positioned rather than justified — which is the point of the change.
    float knob = 0.0f;
    for (std::size_t i = 0; i < arena.size(); ++i) {
        const Node& node = arena[NodeId{static_cast<std::uint32_t>(i)}];
        if (node.style.position != Position::Absolute) continue;
        if (node.style.width != node.style.height) continue;  // the round knob
        knob = std::max(knob, node.style.left);
    }
    return knob;
}

}  // namespace

TEST("the switch knob travels instead of jumping") {
    Animator animator;
    // Off, and off is where it starts: no animation on the first frame.
    CHECK_NEAR(knobTravel(animator, false), 0.0f);

    // Turned on, the knob has not moved yet…
    knobTravel(animator, true);
    advance(animator, 0.09f);
    const float middle = knobTravel(animator, true);
    CHECK(middle > 0.0f);

    // …and a moment later it is against the far end.
    advance(animator, 0.25f);
    const float settled = knobTravel(animator, true);
    CHECK(settled > middle);
    CHECK(!animator.animating());
}

TEST("without an animator a switch still lands on its target") {
    // The opt-out path: no animator, no motion, and nothing at a half-position.
    Animator none;
    Arena arena;
    Ui ui(arena);
    Theme theme = Theme::dark();
    Interaction input;
    {
        auto column = ui.column({.width = 200.0f});
        (void)toggle(ui, input, "s", true, {});
        (void)column;
    }
    LayoutContext context;
    context.theme = &theme;
    layout(arena, ui.root(), Rect{0, 0, 200, 100}, context);

    float knob = 0.0f;
    for (std::size_t i = 0; i < arena.size(); ++i) {
        const Node& node = arena[NodeId{static_cast<std::uint32_t>(i)}];
        if (node.style.position == Position::Absolute &&
            node.style.width == node.style.height) {
            knob = std::max(knob, node.style.left);
        }
    }
    CHECK(knob > 0.0f);
    CHECK_EQ(none.size(), std::size_t{0});
}

// ---- easing curves ---------------------------------------------------------

TEST("every curve starts at zero and ends at one") {
    // The three overshooting families leave the range in the middle, but they
    // still have to land: an animation that ends at 0.98 leaves the control
    // permanently slightly wrong.
    const Easing all[] = {
        Easing::Linear, Easing::Ease, Easing::EaseIn, Easing::EaseOut, Easing::EaseInOut,
        Easing::SineIn, Easing::SineOut, Easing::SineInOut,
        Easing::QuadIn, Easing::QuadOut, Easing::QuadInOut,
        Easing::CubicIn, Easing::CubicOut, Easing::CubicInOut,
        Easing::QuartIn, Easing::QuartOut, Easing::QuartInOut,
        Easing::QuintIn, Easing::QuintOut, Easing::QuintInOut,
        Easing::ExpoIn, Easing::ExpoOut, Easing::ExpoInOut,
        Easing::CircIn, Easing::CircOut, Easing::CircInOut,
        Easing::BackIn, Easing::BackOut, Easing::BackInOut,
        Easing::ElasticIn, Easing::ElasticOut, Easing::ElasticInOut,
        Easing::BounceIn, Easing::BounceOut, Easing::BounceInOut,
        Easing::Spring,
    };
    for (const Easing curve : all) {
        CHECK_NEAR(ease(curve, 0.0f), 0.0f);
        CHECK_NEAR(ease(curve, 1.0f), 1.0f);
    }
}

TEST("an in curve is slow to leave and an out curve is slow to arrive") {
    // The defining property, and the reason to pick one over the other.
    CHECK(ease(Easing::QuadIn, 0.5f) < 0.5f);
    CHECK(ease(Easing::QuadOut, 0.5f) > 0.5f);
    CHECK(ease(Easing::EaseIn, 0.5f) < 0.5f);
    CHECK(ease(Easing::EaseOut, 0.5f) > 0.5f);
    // In-out is symmetric about the middle.
    CHECK_NEAR(ease(Easing::QuadInOut, 0.5f), 0.5f);
    CHECK_NEAR(ease(Easing::CubicInOut, 0.5f), 0.5f);
}

TEST("the CSS keywords are the curves CSS specifies") {
    // `ease-in` is cubic-bezier(0.42, 0, 1, 1), which is not t³ — the two differ
    // by more than a rounding error, and this is what that used to be.
    CHECK(ease(Easing::EaseIn, 0.5f) > ease(Easing::CubicIn, 0.5f));
    CHECK_NEAR(ease(Easing::EaseIn, 0.5f), ease(CubicBezier{0.42f, 0.0f, 1.0f, 1.0f}, 0.5f));
    CHECK_NEAR(ease(Easing::EaseOut, 0.25f), ease(CubicBezier{0.0f, 0.0f, 0.58f, 1.0f}, 0.25f));
    // A bezier whose controls sit on the diagonal is the identity.
    CHECK_NEAR(ease(CubicBezier{0.0f, 0.0f, 1.0f, 1.0f}, 0.37f), 0.37f);
}

TEST("back and elastic overshoot, which is what they are for") {
    bool overshot = false;
    for (int i = 0; i <= 100; ++i) {
        const float t = static_cast<float>(i) / 100.0f;
        if (ease(Easing::BackOut, t) > 1.0f) overshot = true;
    }
    CHECK(overshot);
    // …and an in curve undershoots below zero on the way out.
    bool undershot = false;
    for (int i = 0; i <= 100; ++i) {
        if (ease(Easing::BackIn, static_cast<float>(i) / 100.0f) < 0.0f) undershot = true;
    }
    CHECK(undershot);
}

TEST("a transition can carry a curve the enum does not have") {
    Animator animator;
    const Transition custom{.duration = 1.0f, .bezier = CubicBezier{0.0f, 0.0f, 0.58f, 1.0f}};
    animator.animate("id", "value", 0.0f, custom);
    animator.animate("id", "value", 1.0f, custom);
    advance(animator, 0.25f);
    // The bezier wins over `easing`, which was left at its default.
    CHECK_NEAR(animator.animate("id", "value", 1.0f, custom),
               ease(CubicBezier{0.0f, 0.0f, 0.58f, 1.0f}, 0.25f));
}

// ---- one-shots -------------------------------------------------------------

TEST("a pulse that has never fired reads as finished") {
    Animator animator;
    // So an effect drawn only while `progress < 1` draws nothing on the frame
    // its button first appears.
    CHECK_NEAR(animator.pulse("b", "ripple", false, {.duration = 0.4f}), 1.0f);
    CHECK(!animator.animating());
}

TEST("a pulse runs once and ends where it began") {
    Animator animator;
    const Transition shot{.duration = 0.4f};
    animator.pulse("b", "ripple", false, shot);

    CHECK_NEAR(animator.pulse("b", "ripple", true, shot), 0.0f);
    advance(animator, 0.2f);
    const float middle = animator.pulse("b", "ripple", false, shot);
    CHECK(middle > 0.4f);
    CHECK(middle < 0.6f);
    CHECK(animator.animating());

    advance(animator, 0.25f);
    CHECK_NEAR(animator.pulse("b", "ripple", false, shot), 1.0f);
    CHECK(!animator.animating());
}

TEST("pressing again restarts the pulse rather than queueing one") {
    Animator animator;
    const Transition shot{.duration = 0.4f};
    animator.pulse("b", "ripple", true, shot);
    advance(animator, 0.3f);
    CHECK(animator.pulse("b", "ripple", false, shot) > 0.5f);

    // A second press starts over from nothing, which is what a second click on
    // the same button should look like.
    CHECK_NEAR(animator.pulse("b", "ripple", true, shot), 0.0f);
}

TEST("a latch keeps its value until it is told otherwise") {
    Animator animator;
    CHECK_NEAR(animator.latch("b", "ripple.x", 12.0f, true), 12.0f);
    // The component is rebuilt every frame and hands over a new pointer
    // position each time; without `set` the origin has to stay where the press
    // put it.
    CHECK_NEAR(animator.latch("b", "ripple.x", 99.0f, false), 12.0f);
    advance(animator, 0.2f);
    CHECK_NEAR(animator.latch("b", "ripple.x", 99.0f, false), 12.0f);
    CHECK_NEAR(animator.latch("b", "ripple.x", 40.0f, true), 40.0f);
}
