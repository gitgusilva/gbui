#include "gbui/widgets/gallery.hpp"

#include <algorithm>
#include <string>

#include "detail.hpp"
#include "gbui/widgets/button.hpp"
#include "gbui/widgets/image.hpp"
#include "gbui/widgets/text.hpp"

namespace gbui {

// What this shares with its siblings, rather than a copy in each.
using namespace detail;

namespace {

/** What a picture is called: its alt, or its caption, or its place in the set.
 *  Never nothing — an unnamed picture in a set of nine is "image, image,
 *  image". */
std::string nameOf(const GalleryItem& item, std::size_t index, std::size_t count) {
    if (!item.alt.empty()) return std::string(item.alt);
    if (!item.caption.empty()) return std::string(item.caption);
    return "Image " + std::to_string(index + 1) + " of " + std::to_string(count);
}

}  // namespace

GalleryResult gallery(Ui& ui, const Interaction& input, std::string_view id,
                      const std::vector<GalleryItem>& items, GalleryState& state,
                      const GalleryOptions& options) {
    GalleryResult result;
    // Nothing at all rather than an empty frame, which looks like a picture
    // that failed to load.
    if (items.empty()) return result;

    const std::size_t count = items.size();
    const std::string stageId = std::string(id) + ".stage";
    const std::string previousId = std::string(id) + ".previous";
    const std::string nextId = std::string(id) + ".next";
    const std::string stripId = std::string(id) + ".thumbnails";

    const std::size_t before = std::min(state.current, count - 1);
    std::size_t current = before;

    const auto step = [&](std::ptrdiff_t by) {
        const auto here = static_cast<std::ptrdiff_t>(current);
        const auto last = static_cast<std::ptrdiff_t>(count - 1);
        if (options.loop) {
            const auto span = static_cast<std::ptrdiff_t>(count);
            current = static_cast<std::size_t>(((here + by) % span + span) % span);
        } else {
            current = static_cast<std::size_t>(std::clamp(here + by, std::ptrdiff_t{0}, last));
        }
    };

    // ---- getting about -----------------------------------------------------
    if (input.isFocused(stageId)) {
        for (const KeyEvent& event : input.keys()) {
            if (event.key == Key::Left) step(-1);
            if (event.key == Key::Right) step(1);
            if (event.key == Key::Home) current = 0;
            if (event.key == Key::End) current = count - 1;
        }
    }
    if (input.clicked(previousId)) step(-1);
    if (input.clicked(nextId)) step(1);
    for (std::size_t i = 0; i < count; ++i) {
        if (input.clicked(std::string(id) + ".thumb." + std::to_string(i))) current = i;
    }

    state.current = current;
    result.current = current;
    result.changed = current != before;

    const GalleryItem& showing = items[current];

    Style outer;
    outer.direction = Direction::Column;
    outer.gap = 8.0f;
    outer.width = options.width;
    outer.grow = options.grow;
    if (options.grow > 0.0f) outer.basis = 0.0f;
    auto outerScope = ui.scope(outer);
    ui.tag(id).accessible({.role = Role::Group, .name = options.name});

    // ---- the picture -------------------------------------------------------
    {
        Style stage;
        stage.height = options.height;
        stage.justify = Justify::Center;
        stage.align = Align::Center;
        stage.background = Fill{Token::BgElevated};
        stage.radius = ui.design().controlRadius;
        stage.overflow = Overflow::Hidden;
        if (input.isFocusVisible(stageId)) {
            stage.outline = Outline{2.0f, 2.0f, Fill{Token::Accent}};
        }
        auto stageScope = ui.scope(stage);
        // The keyboard stop for the whole gallery — one, not one per thumbnail,
        // which is the roving pattern the tab strip and the calendar already
        // use. `Image` rather than a `Group` around an image, because there is
        // exactly one thing here and wrapping it would put a level between the
        // reader and it.
        ui.tag(stageId).focusable().accessible({
            .role = Role::Image,
            .name = nameOf(showing, current, count),
            .positionInSet = current + 1,
            .setSize = count,
        });

        image(ui, showing.image,
              {.width = Length::percent(100),
               .height = Length::percent(100),
               .fit = options.fit,
               .alt = showing.alt.empty() ? showing.caption : showing.alt});

        // The arrows sit *over* the picture, which is where a viewer puts them
        // and the only place that does not steal height from it. Out of the
        // flow, so the picture is centred in the whole stage rather than in
        // what is left after them.
        if (options.navigators && count > 1) {
            Style over;
            over.position = Position::Absolute;
            over.left = 0.0f;
            over.top = 0.0f;
            over.width = Length::percent(100);
            over.height = Length::percent(100);
            over.direction = Direction::Row;
            over.align = Align::Center;
            over.justify = Justify::SpaceBetween;
            over.padding = Edges::symmetric(0.0f, 8.0f);
            auto overScope = ui.scope(over);
            button(ui, input, "",
                   {.leading = Icon::ChevronLeft,
                    .disabled = !options.loop && current == 0,
                    .id = previousId,
                    .name = "Previous image"});
            button(ui, input, "",
                   {.leading = Icon::ChevronRight,
                    .disabled = !options.loop && current + 1 >= count,
                    .id = nextId,
                    .name = "Next image"});
            (void)overScope;
        }
        (void)stageScope;
    }

    if (options.captions && !showing.caption.empty()) {
        text(ui, showing.caption,
             {.color = Token::TextMuted, .size = 12.0f, .align = TextAlign::Center,
              .overflow = TextOverflow::Wrap});
    }

    // ---- the strip ---------------------------------------------------------
    if (options.thumbnails && count > 1) {
        constexpr float kThumbGap = 6.0f;
        // Kept in view before the strip is built, so the offset the thumbnails
        // are laid out against is the one this frame decided — the same order
        // `select` and `timePicker` use for the same reason.
        revealRow(state.thumbnails, RowMetrics{options.thumbnailSize, kThumbGap, 0.0f}, current);

        ScrollOptions strip;
        strip.axis = ScrollAxis::Horizontal;
        strip.direction = Direction::Row;
        strip.gap = kThumbGap;
        strip.grow = 0.0f;
        strip.height = options.thumbnailSize + 10.0f;
        // The stage owns the keyboard stop: Tab should reach the gallery once,
        // not once and then again for its own scroller.
        strip.focusable = false;
        auto scroller = scrollArea(ui, input, stripId + ".scroll", state.thumbnails, strip);
        // A set of choices with exactly one in force, walked from a single stop
        // — the pattern the carousel's dots use, and for the same reasons.
        ui.accessible({
            .role = Role::TabList,
            .name = options.name,
            .relations = {.activeDescendant = std::string(id) + ".thumb." +
                                              std::to_string(current)},
        });

        for (std::size_t i = 0; i < count; ++i) {
            const std::string thumbId = std::string(id) + ".thumb." + std::to_string(i);
            const bool chosen = i == current;
            Style cell;
            cell.width = options.thumbnailSize;
            cell.height = options.thumbnailSize;
            cell.shrink = 0.0f;
            cell.radius = 4.0f;
            cell.overflow = Overflow::Hidden;
            cell.background = Fill{Token::Bg};
            // The border rather than an outline, so a chosen thumbnail does not
            // move its neighbours — the mistake that makes most strips jump.
            cell.border = Border{2.0f, Fill{chosen                    ? Token::Accent
                                            : input.isHovered(thumbId) ? Token::BorderStrong
                                                                       : Token::Border}};
            cell.opacity = chosen ? 1.0f : 0.7f;
            cell.cursorHint = Cursor::Pointer;
            auto cellScope = ui.scope(cell);
            ui.tag(thumbId).cursor(Cursor::Pointer).accessible({
                .role = Role::Tab,
                .name = nameOf(items[i], i, count),
                .state = {.selected = flag(chosen)},
                .positionInSet = i + 1,
                .setSize = count,
            });
            image(ui, items[i].image,
                  {.width = Length::percent(100),
                   .height = Length::percent(100),
                   .fit = ImageFit::Cover});
            (void)cellScope;
        }
        (void)scroller;
    }
    (void)outerScope;

    return result;
}

}  // namespace gbui
