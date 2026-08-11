// Runs of text, and the two semantic shorthands over them.
#pragma once

#include <string_view>
#include <vector>

#include "gbui/scene/ui.hpp"

namespace gbui {

struct TextOptions {
    Token color = Token::Text;
    FontWeight weight = FontWeight::Regular;
    FontSlant slant = FontSlant::Normal;
    FontRole role = FontRole::Ui;
    float size = kAuto;
    TextAlign align = TextAlign::Start;
    /** Give the run the leftover space on the main axis. A commit subject wants
     *  this: it takes what is left after the fixed columns and elides the rest,
     *  instead of pushing them off the row. */
    float grow = 0.0f;
    TextOverflow overflow = TextOverflow::Ellipsis;
    /** With `overflow = Wrap`, how many lines the run may take. Zero is
     *  unlimited; a count clamps it and ellipsises the last line. */
    int maxLines = 0;
    /** A multiplier over the font size. Zero takes the face's own line height,
     *  which is tight for a paragraph and right for a single-line label. */
    float lineHeight = 0.0f;
    bool underline = false;
    bool strikeThrough = false;
    /** A gradient across the run, instead of `color`. Two stops is the common
     *  case: `Gradient::linear(Fill{Token::Accent}, Fill{Token::Text}, 90)`. */
    Gradient gradient{};
};

/** A run of text with the theme's colours applied — the common case that would
 *  otherwise be two structs at every call site. */
NodeId text(Ui& ui, std::string_view value, const TextOptions& options = {});

/** Small, uppercase, muted: the "UNSTAGED (1)" kind of heading. */
NodeId sectionHeading(Ui& ui, std::string_view value);

/**
 * One run inside a line of mixed text.
 *
 * The unit is a *span* because a node holds one `TextStyle` and therefore one
 * colour: "on branch **main**, 3 files changed" is three runs, not one string
 * with markup in it. Keeping them as data rather than as a marked-up string is
 * what stops this becoming a parser.
 */
struct TextSpan {
    std::string_view text;
    Token color = Token::Text;
    /** Across this span alone. Two spans with the same gradient each get their
     *  own sweep — that is what per-span means. */
    Gradient gradient{};
    FontWeight weight = FontWeight::Regular;
    FontSlant slant = FontSlant::Normal;
    FontRole role = FontRole::Ui;
    float size = kAuto;
    bool underline = false;
    bool strikeThrough = false;
};

struct RichTextOptions {
    TextAlign align = TextAlign::Start;
    float grow = 0.0f;
    /** Space between spans. Zero butts them together, which is what a sentence
     *  wants; the spaces come from the spans themselves. */
    float gap = 0.0f;
    /**
     * Lets the line wrap **between spans**.
     *
     * Not the same as wrapping text: the break can only fall where one span
     * ends and the next begins, so a single long span still overflows its line
     * rather than breaking inside itself. Splitting a sentence into more spans
     * gives the layout more places to break. Real wrapping through mixed runs
     * needs an inline formatting context, which the engine does not have.
     */
    bool wrap = false;
};

/**
 * A line of text whose runs differ — in colour, weight, slant or decoration.
 *
 * Built as a row of runs sharing a line, which is the honest shape given that a
 * node carries one style. With `wrap` it becomes a *wrapping* row, so the line
 * breaks between spans — see the caveat on that option. A paragraph that has to
 * break mid-sentence still wants `text` with `TextOverflow::Wrap` and one
 * colour, or a gradient across the whole run.
 *
 * Runs are centred against each other rather than sitting on a shared baseline,
 * because `Align::Baseline` is not implemented yet. Spans of one size — the
 * usual case — look identical either way.
 */
NodeId richText(Ui& ui, const std::vector<TextSpan>& spans, const RichTextOptions& options = {});

/** Semantic shorthands, in the sense HTML gives them: `strong` is importance
 *  and `emphasis` is stress, and each happens to be drawn with a weight or a
 *  slant. They exist so a call site says what it means rather than how it
 *  looks, which is what lets the look change later. */
NodeId strong(Ui& ui, std::string_view value, TextOptions options = {});
NodeId emphasis(Ui& ui, std::string_view value, TextOptions options = {});

}  // namespace gbui
