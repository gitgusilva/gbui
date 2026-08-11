#include "gbui/style/design.hpp"

namespace gbui {

Design Design::gitbox() {
    Design d;
    d.id = "gitbox";
    d.name = "GitBox";
    d.controlRadius = 6.0f;
    d.controlHeight = 30.0f;
    d.borderWidth = 1.0f;
    // The ink, kept subtler than Material's: a desktop press should be felt
    // rather than watched, so it is quicker and fainter but still tells you
    // where you clicked.
    d.press = PressFeedback::Ripple;
    d.rippleAlpha = 0.14f;
    d.motion = Transition{.duration = 0.14f, .easing = Easing::EaseOut};
    return d;
}


Design Design::material() {
    Design d;
    d.id = "material";
    d.name = "Material";
    // Material 3's "full" corner on a button is the pill; its touch target
    // minimum is 40dp for a dense desktop control.
    d.controlRadius = 20.0f;
    d.controlHeight = 40.0f;
    d.borderWidth = 0.0f;  // filled surfaces, not outlines
    d.press = PressFeedback::Ripple;
    d.rippleAlpha = 0.22f;
    // Material's "emphasised decelerate": leaves fast, arrives slowly, and
    // takes longer than a desktop transition would.
    d.motion = Transition{.duration = 0.25f,
                          .bezier = CubicBezier{0.05f, 0.7f, 0.1f, 1.0f}};
    // M3's switch: 52x32 track, a 16dp thumb that grows to 24dp when it turns
    // on. That growth is the tell — no other system does it.
    d.switchWidth = 52.0f;
    d.switchHeight = 32.0f;
    d.switchKnob = 16.0f;
    d.switchKnobOn = 24.0f;
    // 18dp box at a 2dp corner: a Material checkbox is square, not round.
    d.checkboxSize = 18.0f;
    d.checkboxRadius = 2.0f;
    d.radioSize = 20.0f;
    d.hoverAlpha = 0.08f;  // M3's hover state layer
    return d;
}

Design Design::cupertino() {
    Design d;
    d.id = "cupertino";
    d.name = "Cupertino";
    d.controlRadius = 10.0f;
    d.controlHeight = 34.0f;
    d.borderWidth = 0.0f;
    // No ink. A press dims the control and that is all — pressing anything on
    // iOS has always been a change of opacity, not a splash.
    d.press = PressFeedback::Surface;
    d.motion = Transition{.duration = 0.3f, .easing = Easing::Spring};
    // The iOS switch: 51x31 with a 27pt thumb, which is why it reads as a
    // capsule with a ball in it rather than as a track with a dot on it.
    d.switchWidth = 51.0f;
    d.switchHeight = 31.0f;
    d.switchKnob = 27.0f;
    d.switchKnobOn = 27.0f;
    // iOS has no square checkbox in its kit; the nearest thing is a round
    // selection mark, so this rounds fully rather than pretending otherwise.
    d.checkboxSize = 22.0f;
    d.checkboxRadius = 11.0f;
    d.radioSize = 22.0f;
    d.hoverAlpha = 0.06f;
    return d;
}

Design Design::fluent() {
    Design d;
    d.id = "fluent";
    d.name = "Fluent";
    // Windows 11 rounds to 4px on controls and draws a hairline over the fill.
    d.controlRadius = 4.0f;
    d.controlHeight = 32.0f;
    d.borderWidth = 1.0f;
    d.press = PressFeedback::Surface;
    // Fluent is deliberately quick: motion is feedback, not choreography.
    d.motion = Transition{.duration = 0.12f,
                          .bezier = CubicBezier{0.33f, 0.0f, 0.67f, 1.0f}};
    // Fluent's toggle: a 40x20 pill with a small dot well inside it.
    d.switchWidth = 40.0f;
    d.switchHeight = 20.0f;
    d.switchKnob = 12.0f;
    d.switchKnobOn = 12.0f;
    d.checkboxSize = 20.0f;
    d.checkboxRadius = 4.0f;
    d.radioSize = 20.0f;
    d.hoverAlpha = 0.06f;
    return d;
}

}  // namespace gbui
