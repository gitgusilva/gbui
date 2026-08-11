// A pill — a branch name, a counter, a status chip.
#pragma once

#include <string_view>

#include "gbui/scene/ui.hpp"

namespace gbui {

struct BadgeOptions {
    Token background = Token::BgOverlay;
    Token foreground = Token::TextMuted;
};

NodeId badge(Ui& ui, std::string_view value, const BadgeOptions& options = {});

}  // namespace gbui
