#include "gbui/widgets/text.hpp"



namespace gbui {

NodeId text(Ui& ui, std::string_view value, const TextOptions& options) {
    TextStyle textStyle;
    textStyle.color = Fill{options.color};
    textStyle.weight = options.weight;
    textStyle.slant = options.slant;
    textStyle.role = options.role;
    textStyle.size = options.size;
    textStyle.align = options.align;
    textStyle.overflow = options.overflow;
    textStyle.maxLines = options.maxLines;
    textStyle.lineHeight = options.lineHeight;
    textStyle.colorGradient = options.gradient;
    textStyle.decoration = TextDecoration{options.underline, options.strikeThrough};

    Style style;
    style.grow = options.grow;
    // A growing run starts from nothing and takes what is left, rather than
    // from its natural width, which would push its siblings out of the row.
    if (options.grow > 0.0f) style.basis = 0.0f;
    return ui.label(value, textStyle, style);
}

NodeId richText(Ui& ui, const std::vector<TextSpan>& spans, const RichTextOptions& options) {
    Style row;
    row.direction = Direction::Row;
    // Centred, not baselined: see the header. For runs of one size the two are
    // the same, and pretending to a baseline pass that does not exist would be
    // worse than saying so.
    row.align = Align::Center;
    row.gap = options.gap;
    row.grow = options.grow;
    row.wrap = options.wrap;
    if (options.grow > 0.0f) row.basis = 0.0f;
    if (options.align != TextAlign::Start) {
        row.justify = options.align == TextAlign::Center ? Justify::Center : Justify::End;
    }

    auto scope = ui.scope(row);
    for (const TextSpan& span : spans) {
        TextStyle style;
        style.color = Fill{span.color};
        style.colorGradient = span.gradient;
        style.weight = span.weight;
        style.slant = span.slant;
        style.role = span.role;
        style.size = span.size;
        // Runs in a sentence do not elide independently — one of them cut with
        // an ellipsis mid-phrase reads as a bug. They clip together instead.
        style.overflow = TextOverflow::Clip;
        style.decoration = TextDecoration{span.underline, span.strikeThrough};
        Style box;
        // A span that can shrink never reaches the width that would push it to
        // the next line; it just gets narrower. Wrapping needs them rigid.
        if (options.wrap) box.shrink = 0.0f;
        ui.label(span.text, style, box);
    }
    return scope.id();
}

NodeId strong(Ui& ui, std::string_view value, TextOptions options) {
    options.weight = FontWeight::SemiBold;
    if (options.color == Token::Text) options.color = Token::TextStrong;
    return text(ui, value, options);
}

NodeId emphasis(Ui& ui, std::string_view value, TextOptions options) {
    options.slant = FontSlant::Italic;
    return text(ui, value, options);
}

NodeId sectionHeading(Ui& ui, std::string_view value) {
    TextOptions options;
    options.color = Token::TextMuted;
    options.weight = FontWeight::SemiBold;
    options.size = 11.0f;
    const NodeId id = text(ui, value, options);
    // The one run of text in this file that carries a role.
    //
    // A plain run does not, and that is deliberate: a `Node` already holds its
    // string, so the accessibility tree reads the text straight off it and a
    // record per run would double what a text-heavy screen costs to say nothing
    // new. A heading is different — "UNSTAGED (1)" is a landmark a reader jumps
    // between, and nothing about the node says so.
    ui.accessible({.role = Role::Heading, .name = value});
    return id;
}

}  // namespace gbui
