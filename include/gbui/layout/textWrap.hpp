// Line breaking: turning one run of text into the lines that fit a width.
//
// It lives beside the layout engine because both sides of the toolkit need the
// *same* answer. Layout asks how tall a paragraph is; painting asks which
// characters are on line three. A second implementation in the painter is how
// text ends up drawn on lines its box was never sized for.
//
// Measurement is a callback for the same reason it is one in `LayoutContext`:
// this file knows where a line may break, and nothing about fonts.
#pragma once

#include <functional>
#include <string_view>
#include <vector>

#include "gbui/style/style.hpp"

namespace gbui {

/** One laid-out line. `text` is a slice of the string that was wrapped, so a
 *  wrapped run allocates no characters — only the vector of lines. */
struct TextLine {
    std::string_view text;
    float width = 0.0f;
};

/** Measures one unbroken line. Supplied by whoever has the font. */
using MeasureLineWidth = std::function<float(std::string_view)>;

struct WrappedText {
    std::vector<TextLine> lines;
    /** The widest line — what the run needs in order not to overflow. */
    float widest = 0.0f;
    /** `maxLines` cut the run short: there is text after the last line, and a
     *  painter should end that line with an ellipsis. */
    bool truncated = false;
};

/**
 * Breaks `text` into lines no wider than `maxWidth`, greedily — the algorithm
 * every UI toolkit uses, and the one CSS describes. Knuth-Plass minimises
 * raggedness across a paragraph and is what a typesetter wants; a resizing
 * window is not a typesetter, and reflowing every line when the last word
 * changes is the wrong trade here.
 *
 * `maxLines` of zero or less means unlimited.
 *
 * The rules, in the order they apply:
 *
 * - A newline always breaks, and an empty paragraph still produces a line, so
 *   a blank line in a commit message keeps its height.
 * - Lines break at spaces. The spaces at a break are dropped rather than drawn
 *   at the end of a line, and leading spaces on a continuation line go too.
 * - A single word too wide for the whole line is broken between characters, on
 *   a UTF-8 boundary. CSS would let it overflow instead; this is what
 *   `overflow-wrap: break-word` does, and it is the right default for a
 *   toolkit whose containers clip — an unbroken URL should be readable on three
 *   lines rather than half-drawn on one. It costs nothing in the normal case,
 *   because `min-width: auto` already reserves the longest word's width.
 *
 * With `maxWidth` infinite or non-positive only the newlines break, which is
 * how an intrinsic-width measurement asks "how wide would this like to be".
 */
WrappedText wrapText(std::string_view text, float maxWidth, int maxLines,
                     const MeasureLineWidth& width, WordBreak wordBreak = WordBreak::Normal);

}  // namespace gbui
