#include "gbui/widgets/kbd.hpp"

#include <cctype>
#include <string>
#include <vector>

#include "gbui/widgets/text.hpp"

namespace gbui {

namespace {

std::string_view trim(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }
    return value;
}

}  // namespace

NodeId kbd(Ui& ui, std::string_view keys, const KbdOptions& options) {
    std::vector<std::string_view> caps;
    if (options.separator.empty()) {
        caps.push_back(trim(keys));
    } else {
        std::size_t at = 0;
        while (at <= keys.size()) {
            const std::size_t next = keys.find(options.separator, at);
            const std::string_view part =
                trim(keys.substr(at, next == std::string_view::npos ? std::string_view::npos
                                                                    : next - at));
            if (!part.empty()) caps.push_back(part);
            if (next == std::string_view::npos) break;
            at = next + options.separator.size();
        }
    }

    Style row;
    row.direction = Direction::Row;
    row.align = Align::Center;
    row.gap = 3.0f;
    row.shrink = 0.0f;
    auto scope = ui.scope(row);
    const NodeId id = scope.id();

    // Read as one thing. Without this a reader meets three unlabelled groups
    // and has to assemble the shortcut themselves; with a name they are told
    // it. The caps are hidden rather than left to be read as loose letters.
    ui.accessible({.role = Role::Group,
                   .name = options.name.empty() ? keys : options.name});

    for (std::size_t i = 0; i < caps.size(); ++i) {
        if (i > 0 && !options.separator.empty()) {
            text(ui, options.separator, {.color = Token::TextMuted, .size = options.size});
        }
        Style cap;
        cap.align = Align::Center;
        cap.justify = Justify::Center;
        cap.minWidth = options.size + 10.0f;   // a single letter is still a key
        cap.minHeight = options.size + 8.0f;
        cap.shrink = 0.0f;
        cap.padding = Edges::symmetric(1.0f, 5.0f);
        cap.radius = 4.0f;
        cap.background = Fill{Token::BgOverlay};
        // The bottom edge darker than the rest, which is the whole of what
        // makes a rectangle read as a key rather than as a chip.
        cap.border = Border{1.0f, Fill{Token::BorderStrong}};
        {
            auto capScope = ui.scope(cap);
            ui.accessible({.hidden = true});
            text(ui, caps[i],
                 {.color = Token::Text, .weight = FontWeight::Medium, .role = FontRole::Mono,
                  .size = options.size});
            (void)capScope;
        }
    }
    scope.close();
    return id;
}

}  // namespace gbui
