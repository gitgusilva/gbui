#include "gbui/widgets/segmented.hpp"

#include <string>

#include "detail.hpp"
#include "gbui/widgets/icon.hpp"
#include "gbui/widgets/text.hpp"

namespace gbui {

using namespace detail;

namespace {

/** The next segment the keyboard may land on, skipping disabled ones. Wraps,
 *  because a radio group walked with the arrows should not stop dead at either
 *  end. Returns where it started when nothing else is reachable. */
std::size_t nextEnabled(const std::vector<Segment>& segments, std::size_t from, bool forward) {
    const std::size_t count = segments.size();
    for (std::size_t step = 1; step <= count; ++step) {
        const std::size_t at =
            forward ? (from + step) % count : (from + count - (step % count)) % count;
        if (!segments[at].disabled) return at;
    }
    return from;
}

}  // namespace

std::optional<std::size_t> segmented(Ui& ui, const Interaction& input, std::string_view id,
                                     const std::vector<Segment>& segments, std::size_t selected,
                                     const SegmentedOptions& options) {
    std::optional<std::size_t> chosen;
    if (segments.empty()) return chosen;

    const float height = options.height > 0.0f ? options.height : ui.design().controlHeight - 2.0f;

    Style track;
    track.direction = Direction::Row;
    track.align = Align::Center;
    track.gap = 2.0f;
    track.padding = Edges::all(2.0f);
    track.radius = ui.design().controlRadius;
    track.background = Fill{Token::BgOverlay, 0.7f};
    track.border = Border{1.0f, Fill{Token::Border}};
    track.opacity = opacityFor(options.disabled);
    track.minHeight = 0.0f;
    if (!options.stretch) track.shrink = 0.0f;

    auto trackScope = ui.scope(track);
    // **One Tab stop, on the group.** A strip of five segments that each took
    // the keyboard would be five Tab presses to cross a single choice, which is
    // why ARIA's radio group is one stop and the arrows do the moving.
    ui.tag(id).focusable(!options.disabled);
    ui.accessible({
        .role = Role::RadioGroup,
        .name = options.name,
        .state = {.disabled = flag(options.disabled)},
    });

    for (std::size_t i = 0; i < segments.size(); ++i) {
        const Segment& segment = segments[i];
        const bool on = i == selected;
        const bool dead = options.disabled || segment.disabled;
        const std::string segmentId = std::string(id) + "." + std::to_string(i);
        const bool hovered = input.isHovered(segmentId);

        Style cell;
        cell.direction = Direction::Row;
        cell.align = Align::Center;
        cell.justify = Justify::Center;
        cell.gap = 6.0f;
        cell.height = height;
        cell.minHeight = 0.0f;
        cell.minWidth = 0.0f;
        cell.padding = Edges::symmetric(0.0f, 10.0f);
        cell.radius = ui.design().controlRadius - 2.0f;
        if (options.stretch) {
            cell.grow = 1.0f;
            cell.basis = 0.0f;
        } else {
            cell.shrink = 0.0f;
        }
        // The chosen one is a raised surface rather than a wash of the accent:
        // a strip where the selection is the loudest thing on the row competes
        // with whatever it is controlling.
        if (on) {
            cell.background = Fill{Token::BgElevated};
            cell.border = Border{1.0f, Fill{Token::BorderStrong}};
        } else if (hovered && !dead) {
            cell.background = Fill{Token::SurfaceHover};
        }
        cell.opacity = opacityFor(segment.disabled);
        cell.cursorHint = dead ? Cursor::NotAllowed : Cursor::Pointer;

        {
            auto cellScope = ui.scope(cell);
            // Not focusable: the group holds the keyboard and the segments are
            // reached with the arrows. They are still hit targets.
            ui.tag(segmentId).cursor(cell.cursorHint);
            ui.accessible({
                .role = Role::Radio,
                .name = segment.name.empty() ? segment.label : segment.name,
                .state = {.checked = flag(on), .disabled = flag(dead)},
                .positionInSet = i + 1,
                .setSize = segments.size(),
            });
            if (segment.icon) {
                icon(ui, *segment.icon,
                     {.color = on ? Token::TextStrong : Token::TextMuted, .size = 14.0f});
            }
            if (!segment.label.empty()) {
                text(ui, segment.label,
                     {.color = on ? Token::TextStrong : Token::TextMuted,
                      .weight = on ? FontWeight::Medium : FontWeight::Regular,
                      .size = options.size});
            }
            (void)cellScope;
        }
        if (!dead && activated(input, segmentId, false)) chosen = i;
    }
    trackScope.close();

    if (options.disabled) return chosen;

    // The arrows **move and choose** in one press. A radio group that only moved
    // a highlight would need a second press to commit, which is not how any
    // platform's radio group behaves and not what the reader expects from a row
    // whose whole point is that the answer is visible.
    if (input.isFocused(id)) {
        for (const KeyEvent& event : input.keys()) {
            switch (event.key) {
                case Key::Left:
                case Key::Up:
                    chosen = nextEnabled(segments, selected, false);
                    break;
                case Key::Right:
                case Key::Down:
                    chosen = nextEnabled(segments, selected, true);
                    break;
                case Key::Home:
                    chosen = nextEnabled(segments, segments.size() - 1, true);
                    break;
                case Key::End:
                    chosen = nextEnabled(segments, 0, false);
                    break;
                default:
                    break;
            }
        }
    }
    return chosen;
}

}  // namespace gbui
