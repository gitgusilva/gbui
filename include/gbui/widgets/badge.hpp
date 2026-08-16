// A pill — a branch name, a counter, a status chip.
#pragma once

#include <string_view>

#include "gbui/scene/ui.hpp"

namespace gbui {

struct BadgeOptions {
    Token background = Token::BgOverlay;
    /** The text on the wash. Paired with `background`, and the contrast
     *  between the two is the caller's to get right — a pill is one node and
     *  there is nothing here that can check it. */
    Token foreground = Token::TextMuted;
};

NodeId badge(Ui& ui, std::string_view value, const BadgeOptions& options = {});

}  // namespace gbui
