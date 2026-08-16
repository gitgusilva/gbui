#include "gbui/widgets/colorPicker.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "detail.hpp"
#include "gbui/widgets/popover.hpp"
#include "gbui/widgets/text.hpp"

namespace gbui {

// What this shares with its siblings, rather than a copy in each.
using namespace detail;

namespace {

/** The six pure hues, plus red again so the rail closes on itself. */
Gradient hueRamp() {
    Gradient ramp;
    ramp.kind = GradientKind::Linear;
    ramp.angle = 90.0f;  // left to right
    for (int i = 0; i <= 6; ++i) {
        const float hue = static_cast<float>(i) * 60.0f;
        ramp.stops.push_back({static_cast<float>(i) / 6.0f,
                              Fill{Hsv{hue, 1.0f, 1.0f, 1.0f}.toColor()}});
    }
    return ramp;
}

/** Where a pointer landed inside a rectangle, 0..1 on each axis. */
Vec2 fractionIn(const Rect& box, Vec2 point) {
    if (box.width <= 0.0f || box.height <= 0.0f) return {};
    return {std::clamp((point.x - box.x) / box.width, 0.0f, 1.0f),
            std::clamp((point.y - box.y) / box.height, 0.0f, 1.0f)};
}

/**
 * The little ring that marks a position on the square or a rail.
 *
 * A ring rather than a dot so the colour underneath stays visible, which is the
 * one thing a marker must not hide — and **kept inside its box**. A ring centred
 * on a value at either extreme would hang half outside the control, which reads
 * as a rendering fault rather than as a value at the end of its range. The
 * *centre* is clamped, not the value, so the colour it marks is still exact.
 */
void marker(Ui& ui, float left, float top, float size, Color shows, Vec2 box) {
    const float half = size / 2.0f;
    if (box.x > 0.0f) left = std::clamp(left, half, std::max(half, box.x - half));
    if (box.y > 0.0f) top = std::clamp(top, half, std::max(half, box.y - half));

    // Filled with the colour it marks, inside a white ring.
    //
    // A hollow ring lets the background through, which is the right instinct
    // but the wrong result: over a mid-grey it disappears. A white ring reads
    // on every hue in the square, and filling the middle turns the marker into
    // a *sample* of the choice rather than a hole in it.
    Style ring;
    ring.position = Position::Absolute;
    ring.left = left - half;
    ring.top = top - half;
    ring.width = size;
    ring.height = size;
    ring.radius = half;
    ring.background = Fill{shows};
    ring.border = Border{2.0f, Fill{Color{255, 255, 255}}};
    ui.add(ring);

    // A hairline of shadow outside it, so the white ring survives a white
    // corner of the square.
    Style halo;
    halo.position = Position::Absolute;
    halo.left = left - half - 1.0f;
    halo.top = top - half - 1.0f;
    halo.width = size + 2.0f;
    halo.height = size + 2.0f;
    halo.radius = half + 1.0f;
    halo.border = Border{1.0f, Fill{Color{0, 0, 0}, 0.35f}};
    ui.add(halo);
}

/** A chequerboard, so a translucent colour reads as translucent rather than as
 *  a darker opaque one. */
void chequer(Ui& ui, float width, float height) {
    constexpr float kCell = 6.0f;
    const int columns = static_cast<int>(std::ceil(width / kCell));
    const int rows = std::max(1, static_cast<int>(std::ceil(height / kCell)));
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < columns; ++x) {
            if ((x + y) % 2 == 0) continue;
            Style cell;
            cell.position = Position::Absolute;
            cell.left = static_cast<float>(x) * kCell;
            cell.top = static_cast<float>(y) * kCell;
            cell.width = std::min(kCell, width - static_cast<float>(x) * kCell);
            cell.height = std::min(kCell, height - static_cast<float>(y) * kCell);
            cell.background = Fill{Color{255, 255, 255}, 0.16f};
            ui.add(cell);
        }
    }
}

}  // namespace

ColorPickerResult colorPicker(Ui& ui, const Interaction& input, std::string_view id,
                              ColorPickerState& state, const ColorPickerOptions& options) {
    ColorPickerResult result;
    const Hsv before = state.value;

    const std::string squareId = std::string(id) + ".square";
    const std::string hueId = std::string(id) + ".hue";
    const std::string alphaId = std::string(id) + ".alpha";

    // ---- the drags ---------------------------------------------------------
    // All three read last frame's rectangle and the live pointer, so a drag
    // keeps working after the pointer leaves the control — which is what makes
    // a picker usable at the edges, where the interesting colours are.
    const Rect squareFrame = input.frameOf(squareId);
    if (input.dragging() == squareId && !squareFrame.empty()) {
        const Vec2 at = fractionIn(squareFrame, input.pointer());
        state.value.saturation = at.x;
        state.value.value = 1.0f - at.y;
    }
    const Rect hueFrame = input.frameOf(hueId);
    if (input.dragging() == hueId && !hueFrame.empty()) {
        state.value.hue = fractionIn(hueFrame, input.pointer()).x * 360.0f;
    }
    const Rect alphaFrame = input.frameOf(alphaId);
    if (input.dragging() == alphaId && !alphaFrame.empty()) {
        state.value.alpha = fractionIn(alphaFrame, input.pointer()).x;
    }

    // ---- the keys ----------------------------------------------------------
    //
    // **A picker reachable only by pointer is a picker most people cannot
    // use**, and until this the square had no keyboard at all. That is not a
    // smaller gap than a missing role, it is a worse one: a role at least says
    // the control is there. All three targets are Tab stops now and all three
    // answer the arrows.
    //
    // The steps are the ones a reader can actually aim with. One percent moves
    // a 150-pixel square by a pixel and takes a hundred presses to cross it;
    // five gets there in twenty and is still finer than a hand on a trackpad.
    // Shift is the coarse gesture everywhere else, so it is ten times the step
    // here. Home and End go to the ends of the axis the key belongs to, which
    // is the only unambiguous reading of them on a two-dimensional control.
    const auto arrows = [&](std::string_view target, float& along, float step, float low,
                            float high, float* upDown = nullptr) {
        if (!input.isFocused(target)) return;
        for (const KeyEvent& event : input.keys()) {
            const float delta = step * (event.modifiers.shift ? 10.0f : 1.0f);
            switch (event.key) {
                case Key::Left: along = std::clamp(along - delta, low, high); break;
                case Key::Right: along = std::clamp(along + delta, low, high); break;
                case Key::Home: along = low; break;
                case Key::End: along = high; break;
                // The second axis, where there is one, and always 0 to 1: the
                // only control with two is the square, and its vertical axis is
                // brightness.
                case Key::Up:
                    if (upDown) *upDown = std::clamp(*upDown + delta, 0.0f, 1.0f);
                    break;
                case Key::Down:
                    if (upDown) *upDown = std::clamp(*upDown - delta, 0.0f, 1.0f);
                    break;
                default: break;
            }
        }
    };
    // Up and Down are *brightness* on the square and nothing on a rail: a rail
    // is one axis, and swallowing the vertical arrows there would take them
    // from whatever the picker is sitting in.
    arrows(squareId, state.value.saturation, 0.05f, 0.0f, 1.0f, &state.value.value);
    arrows(hueId, state.value.hue, 360.0f * 0.02f, 0.0f, 360.0f);
    arrows(alphaId, state.value.alpha, 0.05f, 0.0f, 1.0f);

    const Color current = state.color();
    const Color pureHue = Hsv{state.value.hue, 1.0f, 1.0f, 1.0f}.toColor();

    Style panel;
    panel.direction = Direction::Column;
    panel.gap = options.gap;
    panel.width = options.width;
    auto scope = ui.scope(panel);
    ui.tag(id);

    // ---- the saturation / value square -------------------------------------
    {
        Style square;
        square.height = options.squareHeight;
        square.radius = 6.0f;
        square.background = Fill{pureHue};
        square.cursorHint = Cursor::Crosshair;
        if (input.isFocusVisible(squareId)) {
            square.outline = Outline{2.0f, 2.0f, Fill{Token::Accent}};
        }
        auto squareScope = ui.scope(square);
        ui.tag(squareId).focusable().cursor(Cursor::Crosshair);
        // Two dimensions in one control, and no role for that anywhere — ARIA
        // has `slider` and nothing two-dimensional. So it says what it *is*
        // rather than pretending to be one axis of itself, and the value it
        // reports is the colour, which is the answer either way.
        //
        // The description carries the keys, because a reader has no other way
        // to find out that a `Group` answers the arrows: nothing about the role
        // implies it, and the affordance a sighted user gets is the crosshair.
        ui.accessible({
            .role = Role::Group,
            .name = "Saturation and brightness",
            .description = "Arrow keys adjust; hold Shift for larger steps",
            .value = {.present = true, .text = current.hex()},
        });

        // White across, black down. Two gradients over the hue make the ramp
        // two-dimensional without the painter needing a shader.
        const auto wash = [&](Gradient gradient) {
            Style layer;
            layer.position = Position::Absolute;
            layer.left = 0.0f;
            layer.top = 0.0f;
            layer.width = Length::percent(100);
            layer.height = Length::percent(100);
            layer.radius = 6.0f;
            layer.backgroundGradient = std::move(gradient);
            ui.add(layer);
        };
        wash(Gradient::linear(Fill{Color{255, 255, 255}}, Fill{Color{255, 255, 255}, 0.0f}, 90.0f));
        wash(Gradient::linear(Fill{Color{0, 0, 0}, 0.0f}, Fill{Color{0, 0, 0}}, 180.0f));

        if (!squareFrame.empty()) {
            marker(ui, state.value.saturation * squareFrame.width,
                   (1.0f - state.value.value) * squareFrame.height, 14.0f, current,
                   {squareFrame.width, squareFrame.height});
        }
        (void)squareScope;
    }

    // ---- the rails ---------------------------------------------------------
    const auto rail = [&](std::string_view railId, const Rect& frame, Gradient gradient,
                          float position, bool chequered) {
        Style track;
        track.height = options.railHeight;
        track.radius = options.railHeight / 2.0f;
        track.cursorHint = Cursor::Pointer;
        if (input.isFocusVisible(railId)) {
            track.outline = Outline{2.0f, 2.0f, Fill{Token::Accent}};
        }
        auto trackScope = ui.scope(track);
        ui.tag(railId).focusable().cursor(Cursor::Pointer);
        // A rail is a slider in everything but its drawing, so it says so and
        // reports where it is on its own scale — 0 to 360 for the hue, 0 to 1
        // for the alpha, which is what `position` already carries.
        ui.accessible({
            .role = Role::Slider,
            .name = chequered ? "Opacity" : "Hue",
            .value = {.present = true, .now = position, .minimum = 0.0, .maximum = 1.0},
        });
        if (chequered && !frame.empty()) chequer(ui, frame.width, frame.height);
        {
            Style fill;
            fill.position = Position::Absolute;
            fill.left = 0.0f;
            fill.top = 0.0f;
            fill.width = Length::percent(100);
            fill.height = Length::percent(100);
            fill.radius = options.railHeight / 2.0f;
            fill.backgroundGradient = std::move(gradient);
            ui.add(fill);
        }
        if (!frame.empty()) {
            // Sized to the rail rather than beyond it, so the ring cannot spill
            // above and below a 14-pixel track. The hue marker shows the hue
            // itself, not the shaded colour — it is a hue picker.
            marker(ui, position * frame.width, frame.height / 2.0f, options.railHeight,
                   chequered ? current : pureHue, {frame.width, frame.height});
        }
        (void)trackScope;
    };

    if (options.hue) {
        rail(hueId, hueFrame, hueRamp(), state.value.hue / 360.0f, false);
    }
    if (options.alpha) {
        rail(alphaId, alphaFrame,
             Gradient::linear(Fill{current.withAlpha(0.0f)}, Fill{current.withAlpha(1.0f)}, 90.0f),
             state.value.alpha, true);
    }

    // ---- the readout -------------------------------------------------------
    if (options.showHex) {
        Style readoutRow;
        readoutRow.direction = Direction::Row;
        readoutRow.align = Align::Center;
        readoutRow.gap = 8.0f;
        auto row = ui.scope(readoutRow);
        {
            // The swatch sits on a chequerboard too, or a 10% colour would look
            // like a pale one rather than a transparent one.
            Style swatch;
            swatch.width = 26.0f;
            swatch.height = 20.0f;
            swatch.shrink = 0.0f;
            swatch.radius = 4.0f;
            swatch.border = Border{1.0f, Fill{Token::Border}};
            auto swatchScope = ui.scope(swatch);
            chequer(ui, 26.0f, 20.0f);
            Style over;
            over.position = Position::Absolute;
            over.left = 0.0f;
            over.top = 0.0f;
            over.width = Length::percent(100);
            over.height = Length::percent(100);
            over.background = Fill{current};
            ui.add(over);
            (void)swatchScope;
        }
        std::string readout = current.hex();
        if (options.alpha) {
            char buffer[8];
            std::snprintf(buffer, sizeof(buffer), "  %3.0f%%", state.value.alpha * 100.0f);
            readout += buffer;
        }
        text(ui, readout, {.color = Token::Text, .role = FontRole::Mono, .size = 12.0f});
        (void)row;
    }

    // ---- the swatches ------------------------------------------------------
    if (!options.swatches.empty()) {
        Style swatchRow;
        swatchRow.direction = Direction::Row;
        swatchRow.gap = 6.0f;
        swatchRow.wrap = true;
        auto row = ui.scope(swatchRow);
        for (std::size_t i = 0; i < options.swatches.size(); ++i) {
            const std::string swatchId = std::string(id) + ".swatch." + std::to_string(i);
            Style swatch;
            swatch.width = 20.0f;
            swatch.height = 20.0f;
            swatch.shrink = 0.0f;
            swatch.radius = 4.0f;
            swatch.background = Fill{options.swatches[i]};
            swatch.border = Border{1.0f, Fill{input.isHovered(swatchId) ? Token::BorderStrong
                                                                       : Token::Border}};
            swatch.cursorHint = Cursor::Pointer;
            ui.add(swatch);
            ui.tag(swatchId).cursor(Cursor::Pointer);
            // The hex is the name here rather than the value: a swatch is a
            // button that sets a colour, and there is nothing else to call it.
            ui.accessible({
                .role = Role::Button,
                .name = options.swatches[i].hex(),
                .positionInSet = i + 1,
                .setSize = options.swatches.size(),
            });
            if (input.clicked(swatchId)) {
                // A swatch sets the hue too, which a round trip through RGB
                // would lose for a grey — so it is taken from the swatch and
                // only overwritten when the swatch has one.
                const Hsv picked = Hsv::fromColor(options.swatches[i]);
                state.value.saturation = picked.saturation;
                state.value.value = picked.value;
                if (picked.saturation > 0.0f) state.value.hue = picked.hue;
                if (options.alpha) state.value.alpha = picked.alpha;
            }
        }
        (void)row;
    }
    (void)scope;

    result.color = state.color();
    result.changed = state.value.hue != before.hue || state.value.saturation != before.saturation ||
                     state.value.value != before.value || state.value.alpha != before.alpha;
    return result;
}

ColorPickerResult colorField(Ui& ui, const Interaction& input, std::string_view id,
                             ColorPickerState& state, const ColorFieldOptions& options) {
    ColorPickerResult result;
    result.color = state.color();

    const std::string triggerId = std::string(id) + ".trigger";
    const Color current = state.color();

    // ---- the trigger -------------------------------------------------------
    Style trigger;
    trigger.direction = Direction::Row;
    trigger.align = Align::Center;
    trigger.gap = 8.0f;
    trigger.minHeight = options.height;
    trigger.padding = Edges::symmetric(0.0f, 4.0f);
    trigger.shrink = 0.0f;
    trigger.radius = ui.design().controlRadius;
    trigger.background = Fill{Token::Bg};
    trigger.border = Border{1.0f, Fill{input.isHovered(triggerId) && !options.disabled
                                           ? Token::Accent
                                           : Token::BorderStrong}};
    if (input.isFocusVisible(triggerId)) trigger.outline = Outline{2.0f, 2.0f, Fill{Token::Accent}};
    trigger.opacity = opacityFor(options.disabled);
    trigger.cursorHint = options.disabled ? Cursor::NotAllowed : Cursor::Pointer;

    {
        auto scope = ui.scope(trigger);
        ui.tag(triggerId).focusable(!options.disabled).cursor(trigger.cursorHint);
        // The hex is the value, and it is the value whether or not
        // `showHexOnTrigger` draws it: a swatch is a colour a reader cannot
        // have, and `#3b82f6` is the only form of it that survives being read
        // out. The name is the caller's, because a colour picker in a form is
        // "Accent" and one in a chart editor is "Series 3".
        ui.accessible({
            .role = Role::ComboBox,
            .name = options.name,
            .state = {.expanded = flag(state.open), .disabled = flag(options.disabled)},
            .value = {.present = true, .text = current.hex()},
        });

        {
            // Its own block: a Scope closes when it *leaves scope*, so anything
            // written after it in the same block would still land inside the
            // swatch — which is exactly where the hex label went.
            Style swatch;
            swatch.width = options.height - 8.0f;
            swatch.height = options.height - 8.0f;
            swatch.shrink = 0.0f;
            swatch.radius = 4.0f;
            // On a chequerboard, so a translucent choice reads as translucent.
            auto swatchScope = ui.scope(swatch);
            if (options.alpha) chequer(ui, options.height - 8.0f, options.height - 8.0f);
            Style over;
            over.position = Position::Absolute;
            over.left = 0.0f;
            over.top = 0.0f;
            over.width = Length::percent(100);
            over.height = Length::percent(100);
            over.radius = 4.0f;
            over.background = Fill{current};
            over.border = Border{1.0f, Fill{Color{255, 255, 255}, 0.15f}};
            ui.add(over);
            (void)swatchScope;
        }

        if (options.showHexOnTrigger) {
            text(ui, current.hex(),
                 {.color = Token::TextMuted, .role = FontRole::Mono, .size = 10.0f});
        }
        (void)scope;
    }

    if (options.disabled) return result;
    if (activated(input, triggerId, false)) state.open = !state.open;
    if (!state.open) return result;

    // ---- the popover -------------------------------------------------------
    PopoverOptions popoverOptions;
    popoverOptions.placement = Placement::Bottom;
    popoverOptions.minWidth = options.width;
    popoverOptions.maxWidth = options.width + 24.0f;
    popoverOptions.padding = Edges::all(12.0f);
    popoverOptions.gapBetweenItems = 0.0f;
    // Scrolls inside rather than overflowing. `popover` caps itself at the room
    // the side it landed on actually has, and without a scroll — and the state
    // that scroll needs, which is the half that was missing here — that cap is
    // just a clip: a picker opened near the bottom of the window loses its
    // swatches with no way to reach them.
    popoverOptions.scroll = ScrollAxis::Vertical;
    popoverOptions.scrollState = &state.popup;

    auto surface = popover(ui, input, std::string(id) + ".popover", triggerId, popoverOptions);
    ColorPickerOptions inner = options;
    inner.width = options.width;
    result = colorPicker(ui, input, std::string(id) + ".picker", state, inner);
    (void)surface;

    // A press anywhere else closes it — the same rule `select` follows, and the
    // same caveat: the application owns "open", so this is the component being
    // helpful rather than the component keeping state.
    //
    // Outside the popover *and* outside the trigger: a press on a rail is a
    // drag on the picker and a press on the trigger is that button's own
    // business. `pressedOutside` is the shared version of a test three
    // components used to spell three different ways.
    if (options.dismissOnOutsideClick &&
        pressedOutside(input, {ui.qualify(std::string(id) + ".popover"),
                               ui.qualify(triggerId)})) {
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
