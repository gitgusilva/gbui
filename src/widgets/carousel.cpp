#include "gbui/widgets/carousel.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "detail.hpp"
#include "gbui/widgets/button.hpp"

namespace gbui {

// What this shares with its siblings, rather than a copy in each.
using namespace detail;

namespace {

/** The last index the strip may rest on, so the trailing edge lands on the last
 *  slide rather than on empty space after it. */
std::size_t lastStop(std::size_t count, float perPage) {
    const auto whole = static_cast<std::size_t>(std::max(1.0f, std::floor(perPage)));
    return count > whole ? count - whole : 0;
}

}  // namespace

CarouselResult carousel(Ui& ui, const Interaction& input, std::string_view id, std::size_t count,
                        CarouselState& state, float delta,
                        const std::function<void(Ui&, std::size_t)>& slide,
                        const CarouselOptions& options) {
    CarouselResult result;
    if (count == 0) return result;

    const bool vertical = options.orientation == CarouselOrientation::Vertical;
    const std::string viewId = std::string(id) + ".view";
    const std::string previousId = std::string(id) + ".previous";
    const std::string nextId = std::string(id) + ".next";
    const std::string playId = std::string(id) + ".play";
    const std::string dotsId = std::string(id) + ".dots";

    const float perPage = std::max(0.1f, options.slidesPerPage);
    const std::size_t stop = lastStop(count, perPage);
    const std::size_t before = std::min(state.first, stop);
    std::size_t first = before;

    /** One step, wrapping only when the caller asked for it. */
    const auto step = [&](std::ptrdiff_t by) {
        const auto here = static_cast<std::ptrdiff_t>(first);
        const auto limit = static_cast<std::ptrdiff_t>(stop);
        if (!options.loop) {
            first = static_cast<std::size_t>(std::clamp(here + by, std::ptrdiff_t{0}, limit));
            return;
        }
        const std::ptrdiff_t span = limit + 1;
        first = static_cast<std::size_t>(((here + by) % span + span) % span);
    };

    // ---- the keys ----------------------------------------------------------
    // The pair that matches the strip: a vertical carousel answering Left and
    // Right would be a carousel whose keys point the wrong way.
    if (input.isFocused(viewId)) {
        for (const KeyEvent& event : input.keys()) {
            if (event.key == (vertical ? Key::Up : Key::Left)) step(-1);
            if (event.key == (vertical ? Key::Down : Key::Right)) step(1);
            if (event.key == Key::Home) first = 0;
            if (event.key == Key::End) first = stop;
        }
    }
    if (input.clicked(previousId)) step(-1);
    if (input.clicked(nextId)) step(1);

    // ---- autoplay ----------------------------------------------------------
    //
    // Paused while the pointer is over it or the keyboard is inside it, and
    // stopped for good by the button below. All three are the same rule: a
    // reader looking at one slide should not have it taken away.
    // Measured against the **slides**, not the whole carousel. Reading one is
    // what should stop the clock; reaching for a control is not, and the wider
    // test had a worse consequence than being wrong: pressing Play left the
    // keyboard on the Play button, which counted as attending, so the carousel
    // acknowledged the press and then refused to move.
    const Rect viewFrame = input.frameOf(viewId);
    const bool attended =
        viewFrame.contains(input.pointer()) || input.isFocusedWithin(viewId);
    if (input.clicked(playId)) {
        state.playing = !state.playing;
        state.elapsed = 0.0f;
        result.playingChanged = true;
    }
    if (options.autoplay > 0.0 && state.playing && !attended) {
        state.elapsed += delta;
        if (static_cast<double>(state.elapsed) >= options.autoplay) {
            state.elapsed = 0.0f;
            // Autoplay always wraps, whatever `loop` says. A strip that plays
            // itself to the end and stops has spent the reader's attention and
            // then quietly become a still picture.
            first = first >= stop ? 0 : first + 1;
        }
    } else if (attended) {
        state.elapsed = 0.0f;
    }

    state.first = first;
    result.first = first;
    result.changed = first != before;

    // ---- the frame ---------------------------------------------------------
    Style outer;
    outer.direction = Direction::Column;
    outer.gap = 8.0f;
    outer.width = options.width;
    outer.grow = options.grow;
    if (options.grow > 0.0f) outer.basis = 0.0f;
    auto outerScope = ui.scope(outer);
    // A named region, which is what a reader jumps *to*. The slides carry the
    // "3 of 8" that says where they are once they are in it.
    ui.tag(id).accessible({.role = Role::Group, .name = options.name});

    {
        Style row;
        row.direction = Direction::Row;
        row.align = Align::Center;
        row.gap = 6.0f;
        auto rowScope = ui.scope(row);

        if (options.navigators) {
            button(ui, input, "",
                   {.leading = vertical ? Icon::ChevronUp : Icon::ChevronLeft,
                    .disabled = !options.loop && first == 0,
                    .id = previousId,
                    .name = "Previous slide"});
        }

        // ---- the viewport --------------------------------------------------
        Style view;
        view.grow = 1.0f;
        view.basis = 0.0f;
        view.minWidth = 0.0f;
        view.height = options.height;
        view.overflow = Overflow::Hidden;
        view.radius = ui.design().controlRadius;
        if (input.isFocusVisible(viewId)) view.outline = Outline{2.0f, 2.0f, Fill{Token::Accent}};
        auto viewScope = ui.scope(view);
        ui.tag(viewId).focusable();

        // How big one slide is, and how far one step moves. Both in pixels, so
        // both come from last frame's viewport — the estimate-then-correct that
        // every positioned thing here makes, and unavoidable when the answer is
        // a distance rather than a share.
        const float along = vertical ? viewFrame.height : viewFrame.width;
        const float size = std::max(1.0f, (along - (perPage - 1.0f) * options.gap) / perPage);
        const float pitch = size + options.gap;

        // Animated, so a step is a movement rather than a cut — and because the
        // target only changes when the reader does something, standing still
        // costs nothing.
        const float target = -static_cast<float>(first) * pitch;
        const float offset =
            ui.animate(id, "at", target, {.duration = 0.28f, .easing = Easing::EaseOut});

        Style strip;
        strip.position = Position::Absolute;
        strip.left = vertical ? 0.0f : offset;
        strip.top = vertical ? offset : 0.0f;
        strip.direction = vertical ? Direction::Column : Direction::Row;
        strip.gap = options.gap;
        auto stripScope = ui.scope(strip);

        const auto visible = static_cast<std::size_t>(std::ceil(perPage));
        for (std::size_t i = 0; i < count; ++i) {
            Style cell;
            if (vertical) {
                cell.height = size;
                cell.width = Length::percent(100);
            } else {
                cell.width = size;
                cell.height = options.height;
            }
            cell.shrink = 0.0f;
            cell.minWidth = 0.0f;
            cell.minHeight = 0.0f;
            cell.overflow = Overflow::Hidden;
            auto cellScope = ui.scope(cell);

            // Every slide is built, and the ones off screen are **hidden from
            // the tree** rather than left in it. That is what PrimeVue does and
            // it is right: a reader walking a carousel is walking what is on
            // screen, and eight slides all present at once turns a control into
            // a list they have to get out of.
            const bool onScreen = i >= first && i < first + visible;
            ui.tag(std::string(id) + "." + std::to_string(i)).accessible({
                .role = Role::Group,
                .hidden = !onScreen,
                .positionInSet = i + 1,
                .setSize = count,
            });
            if (slide) slide(ui, i);
            (void)cellScope;
        }
        // Closed here, and it has to be `close()` rather than a cast: a scope
        // ends where it *dies*, and `(void)scope` only silences the unused
        // warning. Without these two the next button was built inside the
        // strip, laid out as a sixth-and-a-half slide twelve hundred pixels
        // off the right edge. That is the fifth time this exact mistake has
        // been made in this tree, which is why the header says so at length.
        stripScope.close();
        viewScope.close();

        if (options.navigators) {
            button(ui, input, "",
                   {.leading = vertical ? Icon::ChevronDown : Icon::ChevronRight,
                    .disabled = !options.loop && first >= stop,
                    .id = nextId,
                    .name = "Next slide"});
        }
        (void)rowScope;
    }

    // ---- the dots, and the pause -------------------------------------------
    if (options.indicators || options.autoplay > 0.0) {
        Style bar;
        bar.direction = Direction::Row;
        bar.align = Align::Center;
        bar.justify = Justify::Center;
        bar.gap = 6.0f;
        auto barScope = ui.scope(bar);

        if (options.indicators) {
            Style dots;
            dots.direction = Direction::Row;
            dots.align = Align::Center;
            dots.gap = 6.0f;
            auto dotsScope = ui.scope(dots);
            // A tab list, which is one of the two patterns ARIA blesses for a
            // carousel and the one that fits: the dots are a set of choices,
            // exactly one is in force, and the arrows move between them.
            ui.tag(dotsId).accessible({
                .role = Role::TabList,
                .relations = {.activeDescendant = std::string(id) + ".dot." +
                                                  std::to_string(first)},
            });

            for (std::size_t i = 0; i <= stop; ++i) {
                const std::string dotId = std::string(id) + ".dot." + std::to_string(i);
                const bool current = i == first;
                Style dot;
                dot.width = current ? 18.0f : 7.0f;
                dot.height = 7.0f;
                dot.shrink = 0.0f;
                dot.radius = 3.5f;
                dot.background = Fill{current                  ? Token::Accent
                                      : input.isHovered(dotId) ? Token::TextMuted
                                                               : Token::BorderStrong};
                dot.cursorHint = Cursor::Pointer;
                if (input.isFocusVisible(dotId)) {
                    dot.outline = Outline{2.0f, 2.0f, Fill{Token::Accent}};
                }
                ui.add(dot);
                // Not focusable: the strip is the keyboard stop and the dots
                // follow it, which is the roving pattern `tabs` already uses.
                ui.tag(dotId).cursor(Cursor::Pointer).accessible({
                    .role = Role::Tab,
                    .name = "Slide " + std::to_string(i + 1),
                    .state = {.selected = flag(current)},
                    .positionInSet = i + 1,
                    .setSize = stop + 1,
                });
                if (input.clicked(dotId)) {
                    first = i;
                    state.first = i;
                    result.first = i;
                    result.changed = i != before;
                }
            }
            (void)dotsScope;
        }

        // **Not optional**, and there is no option for it. Anything that moves
        // on its own needs a way to stop it, and a switch to remove the way to
        // stop it is a switch labelled "make this inaccessible".
        if (options.autoplay > 0.0) {
            button(ui, input, "",
                   {.leading = state.playing ? Icon::Minus : Icon::ChevronRight,
                    .height = 22.0f,
                    .iconSize = 12.0f,
                    .id = playId,
                    .name = state.playing ? "Pause the carousel" : "Play the carousel"});
        }
        (void)barScope;
    }
    (void)outerScope;

    return result;
}

}  // namespace gbui
