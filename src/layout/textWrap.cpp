#include "gbui/layout/textWrap.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace gbui {
namespace {

bool isContinuation(char c) { return (static_cast<unsigned char>(c) & 0xC0U) == 0x80U; }

/** The character boundary at or before `index`. `index == size` is one. */
std::size_t boundaryAtOrBefore(std::string_view text, std::size_t index) {
    index = std::min(index, text.size());
    while (index > 0 && index < text.size() && isContinuation(text[index])) --index;
    return index;
}

/** The end of the first character, so a break always makes progress. */
std::size_t firstBoundary(std::string_view text) {
    std::size_t index = 1;
    while (index < text.size() && isContinuation(text[index])) ++index;
    return std::min(index, text.size());
}

/**
 * The longest prefix of `word` that fits, in bytes, or 0 if not one character
 * does. Binary search rather than a scan: width grows with the prefix, and a
 * 4000-character line of base64 would otherwise measure four thousand times.
 */
std::size_t longestPrefix(std::string_view word, float maxWidth, const MeasureLineWidth& width) {
    std::size_t low = 1;
    std::size_t high = word.size();
    std::size_t best = 0;
    while (low <= high) {
        const std::size_t probe = low + (high - low) / 2;
        const std::size_t cut = boundaryAtOrBefore(word, probe);
        if (cut == 0) {
            low = probe + 1;  // the probe landed inside the first character
            continue;
        }
        if (width(word.substr(0, cut)) <= maxWidth) {
            best = cut;
            // Past the *probe*, not past the cut: snapping back to a boundary
            // can land below `low`, and moving there is no move at all.
            low = probe + 1;
        } else {
            high = cut - 1;
        }
    }
    return best;
}

/** Whether a line may end immediately after this byte.
 *
 * A hyphen and a slash break *after* themselves, keeping the separator on the
 * line it belongs to — "gbui-" then "toolkit", never "gbui" then "-toolkit". */
bool breaksAfter(char c) { return c == '-' || c == '/'; }

}  // namespace

WrappedText wrapText(std::string_view text, float maxWidth, int maxLines,
                     const MeasureLineWidth& width, WordBreak wordBreak) {
    WrappedText out;
    const bool bounded = maxWidth > 0.0f && !std::isinf(maxWidth);
    const std::size_t lineLimit =
        maxLines > 0 ? static_cast<std::size_t>(maxLines)
                     : std::numeric_limits<std::size_t>::max();

    // Returns false once the line limit is reached, which unwinds both loops.
    const auto emit = [&](std::string_view line, float lineWidth) {
        out.lines.push_back({line, lineWidth});
        out.widest = std::max(out.widest, lineWidth);
        return out.lines.size() < lineLimit;
    };

    std::size_t paragraphStart = 0;
    while (paragraphStart <= text.size()) {
        std::size_t newline = text.find('\n', paragraphStart);
        const bool last = newline == std::string_view::npos;
        if (last) newline = text.size();

        std::size_t end = newline;
        if (end > paragraphStart && text[end - 1] == '\r') --end;  // CRLF
        const std::string_view paragraph = text.substr(paragraphStart, end - paragraphStart);

        // An empty paragraph is a blank line, and a blank line has height.
        if (paragraph.empty()) {
            if (!emit(paragraph, 0.0f)) {
                out.truncated = !last;
                return out;
            }
            if (last) return out;
            paragraphStart = newline + 1;
            continue;
        }

        const auto skipSpaces = [&](std::size_t i) {
            while (i < paragraph.size() && paragraph[i] == ' ') ++i;
            return i;
        };

        std::size_t cursor = skipSpaces(0);
        std::size_t lineStart = cursor;
        std::size_t lineEnd = cursor;
        float lineWidth = 0.0f;
        bool blank = true;  // nothing committed to the current line yet

        while (cursor < paragraph.size()) {
            // The next place a line could end. `Anywhere` says every character
            // boundary is one, so the run to consider is the whole rest of the
            // paragraph and the fitting is left to the prefix search below.
            std::size_t wordEnd = cursor;
            if (wordBreak == WordBreak::Anywhere) {
                while (wordEnd < paragraph.size() && paragraph[wordEnd] != ' ') ++wordEnd;
            } else {
                while (wordEnd < paragraph.size() && paragraph[wordEnd] != ' ') {
                    ++wordEnd;
                    // Stop *after* the separator, not before it.
                    if (wordBreak == WordBreak::Normal && breaksAfter(paragraph[wordEnd - 1]) &&
                        wordEnd < paragraph.size() && paragraph[wordEnd] != ' ') {
                        break;
                    }
                }
            }

            const std::string_view candidate =
                paragraph.substr(lineStart, wordEnd - lineStart);
            const float candidateWidth = width(candidate);

            if (bounded && candidateWidth > maxWidth) {
                if (!blank) {
                    // The word belongs on the next line; measure it again there.
                    if (!emit(paragraph.substr(lineStart, lineEnd - lineStart), lineWidth)) {
                        out.truncated = true;
                        return out;
                    }
                    lineStart = cursor;
                    lineEnd = cursor;
                    lineWidth = 0.0f;
                    blank = true;
                    continue;
                }

                // Alone on its line and still too wide. `KeepAll` lets it
                // overflow rather than cutting it, which is the whole point of
                // asking for it: half a hash is not a hash.
                if (wordBreak == WordBreak::KeepAll) {
                    lineEnd = wordEnd;
                    lineWidth = candidateWidth;
                    blank = false;
                    cursor = skipSpaces(wordEnd);
                    continue;
                }
                const std::string_view word = paragraph.substr(cursor, wordEnd - cursor);
                std::size_t cut = longestPrefix(word, maxWidth, width);
                if (cut == 0) cut = firstBoundary(word);  // always make progress
                if (cut < word.size()) {
                    const std::string_view head = word.substr(0, cut);
                    if (!emit(head, width(head))) {
                        out.truncated = true;
                        return out;
                    }
                    cursor += cut;
                    lineStart = cursor;
                    lineEnd = cursor;
                    lineWidth = 0.0f;
                    continue;
                }
            }

            lineEnd = wordEnd;
            lineWidth = candidateWidth;
            blank = false;
            cursor = skipSpaces(wordEnd);
        }

        if (!blank && !emit(paragraph.substr(lineStart, lineEnd - lineStart), lineWidth)) {
            // Whatever is left of this paragraph is spaces; the paragraphs
            // after it are the text being dropped.
            out.truncated = !last;
            return out;
        }

        if (last) return out;
        paragraphStart = newline + 1;
    }
    return out;
}

}  // namespace gbui
