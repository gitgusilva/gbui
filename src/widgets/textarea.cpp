#include "gbui/widgets/textarea.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

#include "detail.hpp"

namespace gbui {

// What this shares with its siblings, rather than a copy in each.
using namespace detail;

namespace {

/** The hard lines of a string, including the empty one a trailing newline
 *  leaves behind — a caret sits on that line and it has to be drawn. */
std::vector<std::string_view> linesOf(std::string_view text) {
    std::vector<std::string_view> lines;
    std::size_t start = 0;
    while (true) {
        const std::size_t newline = text.find('\n', start);
        if (newline == std::string_view::npos) {
            lines.push_back(text.substr(start));
            return lines;
        }
        lines.push_back(text.substr(start, newline - start));
        start = newline + 1;
    }
}

/**
 * Which line an offset falls on, and where it sits within it.
 *
 * Not `Position`: that name is the style enum for how a node is placed, and a
 * second one in an anonymous namespace makes every use of either ambiguous.
 */
struct LineColumn {
    std::size_t line = 0;
    std::size_t column = 0;   ///< bytes into the line, not characters
};

LineColumn lineColumnOf(std::string_view text, std::size_t offset) {
    offset = std::min(offset, text.size());
    LineColumn at;
    for (std::size_t i = 0; i < offset; ++i) {
        if (text[i] == '\n') {
            ++at.line;
            at.column = 0;
        } else {
            ++at.column;
        }
    }
    return at;
}

/** The offset of the start of a line. */
std::size_t startOfLine(const std::vector<std::string_view>& lines, std::string_view text,
                        std::size_t line) {
    if (lines.empty()) return 0;
    const std::string_view row = lines[std::min(line, lines.size() - 1)];
    return static_cast<std::size_t>(row.data() - text.data());
}

}  // namespace

TextEditResult textarea(Ui& ui, const Interaction& input, std::string_view id,
                        TextareaState& state, const TextareaOptions& options) {
    TextEditState& edit = state.edit;
    const bool hovered = input.isHovered(id);
    const bool focused = input.isFocused(id);
    const bool editable = !options.disabled && !options.readOnly;
    const std::string contentId = std::string(id) + ".content";
    const std::string viewId = std::string(id) + ".view";

    const FieldPalette palette = paletteForField(options.disabled, options.readOnly, hovered);

    TextStyle runStyle;
    runStyle.overflow = TextOverflow::Clip;
    const float lineHeight = ui.canMeasure() ? ui.measure("Ag", runStyle).height : 16.0f;

    // ---- the pointer places the caret --------------------------------------
    //
    // Two steps, because there are two axes: the row comes from y, and the
    // offset within that row from x. Last frame's rectangle, like every other
    // component that answers a click — it is also the rectangle the user was
    // pointing at, which is the one that should answer.
    //
    // The press is answered for anything *inside* the field and not only for
    // the field itself, and it has to be: the lines sit in a `scrollArea`, and
    // the run above them ignores the pointer, so a click on the text resolves
    // to a node the scroll view built — `note.view.content` — and never to the
    // box. A text field has no such layer and needed no such check; here,
    // without it, every click was quietly ignored and the caret stayed wherever
    // the keyboard had left it.
    //
    // Every node this component builds is tagged under `id`, which is the
    // convention the whole library uses for exactly this reason.
    const auto isOurs = [&](std::string_view tag) {
        return tag == id || (tag.size() > id.size() + 1 && tag.compare(0, id.size(), id) == 0 &&
                             tag[id.size()] == '.');
    };
    const Rect contentFrame = input.frameOf(contentId);
    const std::string_view held = input.dragging();
    const bool draggingHere = isOurs(held);
    const bool pressedHere = draggingHere && input.pressStarted(held);
    if (editable && contentFrame.height > 0.0f && (pressedHere || draggingHere)) {
        const std::vector<std::string_view> lines = linesOf(edit.text);
        const float localY = input.pointer().y - contentFrame.y;
        const auto row = static_cast<std::size_t>(
            std::clamp(std::floor(localY / std::max(1.0f, lineHeight)), 0.0f,
                       static_cast<float>(lines.size() - 1)));
        const std::size_t rowStart = startOfLine(lines, edit.text, row);
        const float localX = input.pointer().x - contentFrame.x;
        const std::size_t column = offsetAtX(ui, lines[row], runStyle, localX);

        edit.caret = rowStart + column;
        if (pressedHere) edit.anchor = edit.caret;
    }

    TextEditResult result;
    if (focused && editable) {
        const TextEditOptions editing{.multiline = true,
                                      .submitOnModifiedReturn = options.submitOnModifiedReturn};
        result = applyInput(edit, input.keys(), input.text(), editing);
    }
    edit.clampToText();

    // Recomputed after the edit: this frame draws what was just typed.
    const std::vector<std::string_view> lines = linesOf(edit.text);
    const bool empty = edit.text.empty();
    const LineColumn caretAt = lineColumnOf(edit.text, edit.caret);

    // ---- how tall the box is -----------------------------------------------
    //
    // `rows` unless it is allowed to grow, and then as many lines as there are
    // up to `maxRows`. Never fewer than `rows`, so a box does not shrink under
    // the reader as they delete.
    const auto visibleRows = [&] {
        const int wanted = static_cast<int>(lines.size());
        if (options.maxRows <= 0) return options.rows;
        return std::clamp(wanted, options.rows, std::max(options.rows, options.maxRows));
    }();
    const Edges padding = Edges::symmetric(7.0f, 10.0f);
    const float viewport = static_cast<float>(visibleRows) * lineHeight;

    // Keeping the caret in view, before the content is built so the offset the
    // lines are laid out against is the one this frame decided. Only when
    // something moved it: a reader who has scrolled up to check something stays
    // where they put themselves until they type again.
    if (focused && (result.changed || result.moved)) {
        const float top = static_cast<float>(caretAt.line) * lineHeight;
        const float bottom = top + lineHeight;
        state.view.offset = std::clamp(state.view.offset, bottom - viewport, top);
        state.view.offset = std::max(0.0f, state.view.offset);
    }

    Style box;
    box.direction = Direction::Column;
    box.width = options.width;
    box.grow = options.grow;
    box.minWidth = 96.0f;
    box.background = palette.background;
    box.border = Border{1.0f, Fill{palette.border}};
    // `isFocused`, not `isFocusVisible`, for the reason `textField` gives: a box
    // that will swallow the next keystroke has to say so however focus arrived.
    if (focused && editable) box.outline = Outline{2.0f, 2.0f, Fill{Token::Accent}};
    box.radius = ui.design().controlRadius;
    box.overflow = Overflow::Hidden;
    box.opacity = opacityFor(options.disabled);
    box.cursorHint = editable ? Cursor::Text : Cursor::Default;

    auto scope = ui.scope(box);
    ui.tag(id).focusable(!options.disabled).cursor(box.cursorHint);

    {
        ScrollOptions view;
        view.axis = ScrollAxis::Vertical;
        view.direction = Direction::Column;
        view.padding = padding;
        view.maxHeight = viewport + padding.vertical();
        view.grow = 0.0f;
        // The box owns the keyboard stop, not the view inside it: Tab should
        // land on the field once, not on the field and then on its scroller.
        view.focusable = false;
        auto scroller = scrollArea(ui, input, viewId, state.view, view);

        Style content;
        content.direction = Direction::Column;
        // `rows` is a floor as well as a ceiling, and the floor goes here
        // rather than on the box: the scroll view compares this content against
        // its viewport to decide whether there is anything to scroll, and a
        // box padded out to `rows` while its content was one line would report
        // itself scrollable with nowhere to go. An empty two-row box is two
        // rows of content, which is also what it looks like.
        content.minHeight = viewport;
        auto contentScope = ui.scope(content);
        // Tagged for its rectangle alone — a click on the text still belongs to
        // the field, which is what `ignoresPointer` says.
        ui.tag(contentId).ignoresPointer();

        // The placeholder stands in for the whole content, and steps aside once
        // the box has the keyboard: a hint you are being asked to type over is
        // noise the moment you start.
        if (empty && !(focused && editable)) {
            runStyle.color = Fill{Token::TextMuted};
            ui.label(options.placeholder, runStyle);
        } else {
            runStyle.color = Fill{palette.label};
            for (const std::string_view line : lines) {
                Style row;
                row.minHeight = lineHeight;
                auto rowScope = ui.scope(row);
                ui.label(line, runStyle);
                (void)rowScope;
            }
        }

        // ---- the marks -----------------------------------------------------
        //
        // Absolute, and measured from this content box — so a selection that
        // spans three lines is three rectangles rather than one, which is also
        // what makes the last line of a selection stop where the text does
        // instead of running to the edge of the box.
        if (focused && editable) {
            const float caretHeight = lineHeight * 0.92f;
            const float markTop = (lineHeight - caretHeight) / 2.0f;
            const auto widthOf = [&](std::string_view run) {
                return ui.canMeasure() ? ui.measure(run, runStyle).width : 0.0f;
            };

            if (edit.hasSelection()) {
                const LineColumn from = lineColumnOf(edit.text, edit.selectionStart());
                const LineColumn to = lineColumnOf(edit.text, edit.selectionEnd());
                for (std::size_t line = from.line; line <= to.line && line < lines.size(); ++line) {
                    const std::string_view row = lines[line];
                    const std::size_t begin = line == from.line ? from.column : 0;
                    const std::size_t end = line == to.line ? to.column : row.size();
                    if (end < begin) continue;

                    const float left = widthOf(row.substr(0, begin));
                    float width = widthOf(row.substr(begin, end - begin));
                    // A line selected through its break shows that break: a
                    // zero-width mark on an empty line is invisible, and a
                    // selection with a hole in it reads as two selections.
                    if (line < to.line) width = std::max(width, lineHeight * 0.3f);

                    Style mark;
                    mark.position = Position::Absolute;
                    mark.left = left;
                    mark.top = static_cast<float>(line) * lineHeight + markTop;
                    mark.width = width;
                    mark.height = caretHeight;
                    mark.radius = 2.0f;
                    mark.background = Fill{Token::Accent, 0.30f};
                    ui.add(mark);
                }
            }

            // The blink, on the same clock and the same reasoning as the single
            // line field's: solid while the reader works, flashing once they
            // stop, so it never vanishes mid-keystroke.
            constexpr float kBlinkPeriod = 1.06f;
            const float caretNow = static_cast<float>(edit.caret);
            const bool movedCaret = ui.latch(id, "caret.pos", caretNow, false) != caretNow;
            ui.latch(id, "caret.pos", caretNow, true);
            const bool active = movedCaret || !input.text().empty() || !input.keys().empty() ||
                                draggingHere;
            const float since = ui.now() - ui.latch(id, "caret.beat", ui.now(), active);
            const bool visible =
                !ui.animator() || std::fmod(since, kBlinkPeriod) < kBlinkPeriod / 2.0f;

            if (visible && caretAt.line < lines.size()) {
                const float offset = widthOf(lines[caretAt.line].substr(0, caretAt.column));
                // A hairline lands on whole pixels, or it is antialiased across
                // two columns and appears to change width as the text before it
                // changes length.
                const float caretWidth = std::max(1.0f, std::round(lineHeight / 14.0f));
                Style caret;
                caret.position = Position::Absolute;
                caret.left = std::round(contentFrame.x + offset) - contentFrame.x;
                caret.top = static_cast<float>(caretAt.line) * lineHeight + markTop;
                caret.width = caretWidth;
                caret.height = caretHeight;
                caret.background = Fill{Token::Accent};
                ui.add(caret);
            }
        }
        (void)contentScope;
        (void)scroller;
    }
    (void)scope;

    return result;
}

}  // namespace gbui
