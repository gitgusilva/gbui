// Which page you are on, and the ones you can reach from here.
//
// Previous, next, and a window of page numbers. The interesting part is the
// window: a thousand pages cannot all be buttons, so the numbers around the
// current page are drawn and the rest collapse to ellipses — with the **first and
// last always present**, because "jump to the end" is the second most common
// thing anybody does with a paginator and hiding it makes them click next forty
// times.
//
// ---- when not to use one ----------------------------------------------------
//
// Pages are for data a reader *navigates* — search results they will come back
// to, a list they want to bookmark a position in. For data they *scan*, a
// virtualised list is better: `virtualList` builds only the visible rows of fifty
// thousand, and nobody has to decide what a page is. Reach for this when the
// underlying query is paged anyway, and for that reason.
#pragma once

#include <cstddef>
#include <optional>
#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"

namespace gbui {

struct PaginationOptions {
    /**
     * How many numbered buttons to draw around the current page, on each side.
     *
     * One gives `1 … 4 5 6 … 20`, which is the shape most sites settled on: far
     * enough to step without aiming, small enough to fit on a narrow window.
     */
    std::size_t around = 1;
    /** Draw the previous and next arrows. Off leaves only the numbers, for a
     *  strip that sits under something with its own paging controls. */
    bool arrows = true;
    /** What the whole control is for — "Search results". A paginator with no
     *  name is a row of numbers a reader has to infer the subject of. */
    std::string_view name = "Pagination";
    float size = 12.5f;
};

struct PaginationResult {
    /** The page the reader asked for, zero-based, or nothing. Never the page
     *  they are already on: that button is `current` and takes no press. */
    std::optional<std::size_t> chosen{};
};

/**
 * A paginator for `pageCount` pages, sitting on `current` (zero-based).
 *
 * Left and Right step a page while the control has focus, Home and End reach the
 * ends. The current page carries `current` — ARIA's `aria-current`, not
 * `selected`: the reader is being told where they are, not that they chose it
 * from a set of options they could choose differently.
 */
PaginationResult pagination(Ui& ui, const Interaction& input, std::string_view id,
                            std::size_t current, std::size_t pageCount,
                            const PaginationOptions& options = {});

}  // namespace gbui
