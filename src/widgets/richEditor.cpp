#include "gbui/widgets/richEditor.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "detail.hpp"
#include "gbui/widgets/button.hpp"
#include "gbui/widgets/icon.hpp"
#include "gbui/layout/textWrap.hpp"
#include "gbui/widgets/text.hpp"
#include "gbui/widgets/tooltip.hpp"

namespace gbui {

// What this shares with its siblings, rather than a copy in each.
using namespace detail;

namespace {

/** Where every mark run starts or ends, so the text can be cut into pieces that
 *  each wear one set. */
std::vector<std::size_t> boundaries(const Block& block) {
    std::vector<std::size_t> cuts{0, block.text.size()};
    for (const MarkRange& range : block.marks) {
        if (range.from <= block.text.size()) cuts.push_back(range.from);
        if (range.to <= block.text.size()) cuts.push_back(range.to);
    }
    std::sort(cuts.begin(), cuts.end());
    cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
    return cuts;
}

/** Shifts every range to account for `inserted - removed` bytes at `at`.
 *
 * The fiddly part of a range model, and the reason it is worth writing once:
 * an edit inside a run stretches it, an edit before it moves it, and an edit
 * that spans its start eats into it. */
void shiftMarks(Block& block, std::size_t at, std::size_t removed, std::size_t inserted) {
    const auto move = [&](std::size_t offset) {
        if (offset <= at) return offset;
        if (offset >= at + removed) return offset - removed + inserted;
        return at + inserted;  // inside what was deleted: collapses to the edit
    };
    for (MarkRange& range : block.marks) {
        range.from = move(range.from);
        range.to = move(range.to);
    }
    block.marks.erase(std::remove_if(block.marks.begin(), block.marks.end(),
                                     [](const MarkRange& r) { return r.to <= r.from; }),
                      block.marks.end());
}

/** Adds or removes marks over a range, splitting whatever it lands in. */
void applyMark(Block& block, std::size_t from, std::size_t to, Mark mark, bool on,
               std::string_view href) {
    if (to <= from) return;

    std::vector<MarkRange> next;
    for (const MarkRange& range : block.marks) {
        // The parts of this run outside the edit survive unchanged.
        if (range.from < from) {
            next.push_back({range.from, std::min(range.to, from), range.marks, range.href});
        }
        if (range.to > to) {
            next.push_back({std::max(range.from, to), range.to, range.marks, range.href});
        }
        // The overlapping part takes the new mark on top of what it had.
        const std::size_t overlapFrom = std::max(range.from, from);
        const std::size_t overlapTo = std::min(range.to, to);
        if (overlapFrom < overlapTo) {
            const Mark combined = on ? (range.marks | mark) : (range.marks & ~mark);
            next.push_back({overlapFrom, overlapTo, combined,
                            on && mark == Mark::Hyperlink ? std::string(href) : range.href});
        }
    }
    if (on) {
        // …and whatever the existing runs did not cover.
        std::vector<std::pair<std::size_t, std::size_t>> covered;
        for (const MarkRange& range : block.marks) {
            const std::size_t a = std::max(range.from, from);
            const std::size_t b = std::min(range.to, to);
            if (a < b) covered.emplace_back(a, b);
        }
        std::sort(covered.begin(), covered.end());
        std::size_t at = from;
        for (const auto& [a, b] : covered) {
            if (a > at) next.push_back({at, a, mark, std::string(href)});
            at = std::max(at, b);
        }
        if (at < to) next.push_back({at, to, mark, std::string(href)});
    }

    next.erase(std::remove_if(next.begin(), next.end(),
                              [](const MarkRange& r) {
                                  return r.to <= r.from || !any(r.marks);
                              }),
               next.end());
    std::sort(next.begin(), next.end(),
              [](const MarkRange& a, const MarkRange& b) { return a.from < b.from; });
    block.marks = std::move(next);
}

/** The type a block becomes when a toolbar button is pressed — pressing the one
 *  it already is puts it back to a paragraph, which is how every editor lets a
 *  heading be un-headed. */
BlockType toggledType(BlockType current, BlockType wanted) {
    return current == wanted ? BlockType::Paragraph : wanted;
}

TextStyle styleFor(BlockType type, Mark marks) {
    TextStyle style;
    switch (type) {
        case BlockType::Heading1: style.size = 22.0f; style.weight = FontWeight::Bold; break;
        case BlockType::Heading2: style.size = 18.0f; style.weight = FontWeight::SemiBold; break;
        case BlockType::Heading3: style.size = 15.0f; style.weight = FontWeight::SemiBold; break;
        case BlockType::Code: style.role = FontRole::Mono; style.size = 12.0f; break;
        case BlockType::Quote: style.slant = FontSlant::Italic; break;
        default: break;
    }
    if (any(marks & Mark::Bold)) style.weight = FontWeight::Bold;
    if (any(marks & Mark::Italic)) style.slant = FontSlant::Italic;
    if (any(marks & Mark::Code)) style.role = FontRole::Mono;
    style.decoration = TextDecoration{any(marks & (Mark::Underline | Mark::Hyperlink)),
                                      any(marks & Mark::Strike)};
    style.color = Fill{any(marks & Mark::Hyperlink) ? Token::Accent
                       : type == BlockType::Quote ? Token::TextMuted
                                                  : Token::Text};
    style.overflow = TextOverflow::Wrap;
    return style;
}

/**
 * The width of `block.text[from, to)` **with its marks applied**.
 *
 * Cut at every mark boundary inside the range and summed, because bold is wider
 * than regular: measuring the whole run in one style breaks the line a
 * character early or late, and then the caret and the glyphs disagree about
 * where line two starts.
 */
float widthOf(const Ui& ui, const Block& block, std::size_t from, std::size_t to) {
    if (!ui.canMeasure() || to <= from) return 0.0f;
    float total = 0.0f;
    const std::vector<std::size_t> cuts = boundaries(block);
    std::size_t at = from;
    for (const std::size_t cut : cuts) {
        if (cut <= at) continue;
        const std::size_t end = std::min(cut, to);
        if (end <= at) continue;
        total += ui.measure(std::string_view(block.text).substr(at, end - at),
                            styleFor(block.type, block.marksAt(at)))
                     .width;
        at = end;
        if (at >= to) break;
    }
    if (at < to) {
        total += ui.measure(std::string_view(block.text).substr(at, to - at),
                            styleFor(block.type, block.marksAt(at)))
                     .width;
    }
    return total;
}

/** Byte offsets of each visual line's start and end. */
struct LineRange {
    std::size_t from = 0;
    std::size_t to = 0;
};

/** Breaks a block into the lines it will actually be drawn on. */
std::vector<LineRange> linesOf(const Ui& ui, const Block& block, float width) {
    std::vector<LineRange> out;
    if (block.text.empty()) {
        out.push_back({0, 0});
        return out;
    }
    const WrappedText wrapped = wrapText(
        block.text, width, 0,
        [&](std::string_view run) {
            const auto from = static_cast<std::size_t>(run.data() - block.text.data());
            return widthOf(ui, block, from, from + run.size());
        },
        styleFor(block.type, Mark::None).wordBreak);
    for (const TextLine& line : wrapped.lines) {
        const auto from = static_cast<std::size_t>(line.text.data() - block.text.data());
        out.push_back({from, from + line.text.size()});
    }
    if (out.empty()) out.push_back({0, block.text.size()});

    // Wrapping drops the spaces it broke at, which is right for drawing and
    // wrong for a caret: those bytes are still in the text, and the reader who
    // typed them expects to be able to put the caret after them. Give each line
    // back everything up to where the next one starts, so every byte of the
    // block belongs to exactly one line and no offset is unreachable.
    for (std::size_t i = 0; i + 1 < out.size(); ++i) out[i].to = out[i + 1].from;
    out.back().to = block.text.size();
    return out;
}

/** Which line an offset sits on. The last line owns the end of the text, so a
 *  caret at the very end is on the line it was typed onto rather than on a
 *  phantom line after it. */
std::size_t lineAt(const std::vector<LineRange>& lines, std::size_t offset) {
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (offset <= lines[i].to) return i;
    }
    return lines.empty() ? 0 : lines.size() - 1;
}

/** The offset on `line` nearest to `x`, cut on character boundaries. */
std::size_t offsetNear(const Ui& ui, const Block& block, const LineRange& line, float x) {
    if (!ui.canMeasure()) return line.from;
    std::size_t best = line.from;
    float previous = 0.0f;
    for (std::size_t at = line.from; at <= line.to;) {
        const float width = widthOf(ui, block, line.from, at);
        if (x < (previous + width) / 2.0f) return best;
        best = at;
        previous = width;
        if (at >= line.to) break;
        // Next character boundary.
        std::size_t next = at + 1;
        while (next < line.to &&
               (static_cast<unsigned char>(block.text[next]) & 0xC0U) == 0x80U) {
            ++next;
        }
        at = next;
    }
    return line.to;
}

}  // namespace

Mark Block::marksAt(std::size_t offset) const {
    Mark out = Mark::None;
    for (const MarkRange& range : marks) {
        if (offset >= range.from && offset < range.to) out = out | range.marks;
    }
    return out;
}

std::string RichDocument::plainText() const {
    std::string out;
    for (const Block& block : blocks) {
        out += block.text;
        out.push_back('\n');
    }
    return out;
}

std::vector<BlockChoice> defaultBlockChoices() {
    return {
        {BlockType::Paragraph, "Paragraph"}, {BlockType::Heading1, "Heading 1"},
        {BlockType::Heading2, "Heading 2"},  {BlockType::Heading3, "Heading 3"},
        {BlockType::Quote, "Quote"},         {BlockType::Code, "Code block"},
    };
}

std::vector<ToolbarItem> defaultToolbar() {
    return {
        {.action = EditorAction::Bold, .icon = Icon::Bold, .tooltip = "Bold"},
        {.action = EditorAction::Italic, .icon = Icon::Italic, .tooltip = "Italic"},
        {.action = EditorAction::Underline, .icon = Icon::Underline, .tooltip = "Underline"},
        {.action = EditorAction::Strike, .icon = Icon::Strikethrough, .tooltip = "Strikethrough"},
        {.action = EditorAction::Code, .label = "<>", .tooltip = "Inline code"},
        {.action = EditorAction::Hyperlink, .icon = Icon::Link, .tooltip = "Link"},
        {.action = EditorAction::Separator},
        {.action = EditorAction::BlockStyle, .tooltip = "Block style"},
        {.action = EditorAction::Separator},
        {.action = EditorAction::Bullet, .icon = Icon::List, .tooltip = "Bulleted list"},
        {.action = EditorAction::Numbered, .icon = Icon::ListOrdered, .tooltip = "Numbered list"},
        {.action = EditorAction::Quote, .icon = Icon::Quote, .tooltip = "Quote"},
        {.action = EditorAction::CodeBlock, .icon = Icon::Terminal, .tooltip = "Code block"},
    };
}

RichEditorResult richEditor(Ui& ui, const Interaction& input, std::string_view id,
                            RichDocument& document, RichEditorState& state,
                            const RichEditorOptions& options) {
    RichEditorResult result;
    if (document.blocks.empty()) document.blocks.push_back(Block{});
    state.block = std::min(state.block, document.blocks.size() - 1);

    const std::string bodyId = std::string(id) + ".body";
    const bool focused = input.isFocusedWithin(bodyId);

    // The edit state carries the focused block's text; sync it when the caret
    // moved to a different block, so the two never disagree about what is
    // being typed into.
    Block& active = document.blocks[state.block];
    if (state.edit.text != active.text) {
        state.edit.text = active.text;
        state.edit.caret = std::min(state.edit.caret, active.text.size());
        state.edit.anchor = std::min(state.edit.anchor, active.text.size());
    }

    // ---- what the toolbar does ---------------------------------------------
    const auto selection = [&] {
        return std::pair{state.edit.selectionStart(), state.edit.selectionEnd()};
    };
    const auto toggleMark = [&](Mark mark, std::string_view href) {
        const auto [from, to] = selection();
        if (from == to) {
            // Nothing selected: arm it for whatever is typed next, which is
            // what a reader pressing bold before typing means.
            state.pending = any(state.pending & mark) ? (state.pending & ~mark)
                                                      : (state.pending | mark);
            return;
        }
        const bool on = !any(active.marksAt(from) & mark);
        applyMark(active, from, to, mark, on, href);
        result.changed = true;
    };
    const auto setType = [&](BlockType type) {
        active.type = toggledType(active.type, type);
        result.changed = true;
    };

    Style frame;
    frame.direction = Direction::Column;
    frame.minHeight = options.minHeight;
    frame.height = options.height;
    frame.grow = options.grow;
    if (options.grow > 0.0f) frame.basis = 0.0f;
    frame.border = Border{1.0f, Fill{Token::Border}};
    frame.radius = 6.0f;
    frame.overflow = Overflow::Hidden;
    auto scope = ui.scope(frame);
    ui.tag(id);

    // ---- the toolbar -------------------------------------------------------
    if (options.showToolbar) {
        const std::vector<ToolbarItem> items =
            options.toolbar.empty() ? defaultToolbar() : options.toolbar;

        Style bar;
        bar.direction = Direction::Row;
        bar.align = Align::Center;
        bar.gap = 2.0f;
        bar.minHeight = 34.0f;
        bar.shrink = 0.0f;
        bar.wrap = true;   // a long toolbar becomes two rows rather than clipping
        bar.padding = Edges::symmetric(3.0f, 6.0f);
        bar.background = Fill{Token::BgElevated};
        auto barScope = ui.scope(bar);

        for (std::size_t i = 0; i < items.size(); ++i) {
            const ToolbarItem& item = items[i];
            if (item.action == EditorAction::Separator) {
                Style rule;
                rule.width = 1.0f;
                rule.height = 18.0f;
                rule.shrink = 0.0f;
                rule.margin = Edges::symmetric(0.0f, 4.0f);
                rule.background = Fill{Token::Border};
                ui.add(rule);
                continue;
            }

            const std::string itemId = std::string(id) + ".tool." + std::to_string(i);

            if (item.action == EditorAction::BlockStyle) {
                const std::vector<BlockChoice> choices =
                    options.blockChoices.empty() ? defaultBlockChoices() : options.blockChoices;
                std::vector<std::string> labels;
                labels.reserve(choices.size());
                std::optional<std::size_t> current;
                for (std::size_t c = 0; c < choices.size(); ++c) {
                    labels.emplace_back(choices[c].label);
                    if (choices[c].type == active.type) current = c;
                }
                const SelectResult picked =
                    select(ui, input, itemId, labels, current, state.blockStyle,
                           {{}, "Block style", false, 124.0f});
                if (picked.chosen && *picked.chosen < choices.size()) {
                    setType(choices[*picked.chosen].type);
                }
                if (!item.tooltip.empty()) tooltip(ui, input, itemId, item.tooltip);
                continue;
            }

            const Mark mark = item.action == EditorAction::Bold      ? Mark::Bold
                              : item.action == EditorAction::Italic  ? Mark::Italic
                              : item.action == EditorAction::Underline ? Mark::Underline
                              : item.action == EditorAction::Strike  ? Mark::Strike
                              : item.action == EditorAction::Code    ? Mark::Code
                              : item.action == EditorAction::Hyperlink    ? Mark::Hyperlink
                                                                     : Mark::None;
            const auto [from, to] = selection();
            const bool markOn =
                any(mark) && (from == to ? any(state.pending & mark)
                                         : any(active.marksAt(from) & mark));
            const bool typeOn =
                (item.action == EditorAction::Heading1 && active.type == BlockType::Heading1) ||
                (item.action == EditorAction::Heading2 && active.type == BlockType::Heading2) ||
                (item.action == EditorAction::Heading3 && active.type == BlockType::Heading3) ||
                (item.action == EditorAction::Bullet && active.type == BlockType::Bullet) ||
                (item.action == EditorAction::Numbered && active.type == BlockType::Numbered) ||
                (item.action == EditorAction::Quote && active.type == BlockType::Quote) ||
                (item.action == EditorAction::CodeBlock && active.type == BlockType::Code);
            const bool active_ = markOn || typeOn;

            {
            Style key;
            key.direction = Direction::Row;
            key.justify = Justify::Center;
            key.align = Align::Center;
            key.minWidth = 26.0f;
            key.minHeight = 26.0f;
            key.shrink = 0.0f;
            key.padding = Edges::symmetric(0.0f, 5.0f);
            key.radius = 4.0f;
            if (active_) key.background = Fill{Token::Accent, 0.22f};
            else if (input.isHovered(itemId)) key.background = Fill{Token::SurfaceHover};
            key.cursorHint = Cursor::Pointer;
            auto keyScope = ui.scope(key);
            ui.tag(itemId).cursor(Cursor::Pointer);
            if (!item.label.empty()) {
                text(ui, item.label,
                     {.color = active_ ? Token::Accent : Token::Text,
                      .weight = FontWeight::SemiBold, .size = 11.0f});
            } else {
                icon(ui, item.icon,
                     {.color = active_ ? Token::Accent : Token::Text, .size = 13.0f});
            }
            (void)keyScope;
            }
            // Outside the button's scope on purpose: a tooltip is anchored to
            // the control, not contained by it. Built inside, it would be laid
            // out as the button's child and stretch it.
            if (!item.tooltip.empty()) tooltip(ui, input, itemId, item.tooltip);

            if (!input.clicked(itemId)) continue;
            switch (item.action) {
                case EditorAction::Bold:
                case EditorAction::Italic:
                case EditorAction::Underline:
                case EditorAction::Strike:
                case EditorAction::Code: toggleMark(mark, {}); break;
                case EditorAction::Hyperlink: toggleMark(Mark::Hyperlink, "https://example.com"); break;
                case EditorAction::Paragraph: setType(BlockType::Paragraph); break;
                case EditorAction::Heading1: setType(BlockType::Heading1); break;
                case EditorAction::Heading2: setType(BlockType::Heading2); break;
                case EditorAction::Heading3: setType(BlockType::Heading3); break;
                case EditorAction::Bullet: setType(BlockType::Bullet); break;
                case EditorAction::Numbered: setType(BlockType::Numbered); break;
                case EditorAction::Quote: setType(BlockType::Quote); break;
                case EditorAction::CodeBlock: setType(BlockType::Code); break;
                case EditorAction::Custom:
                    if (item.onClick) {
                        item.onClick(document, state);
                        result.changed = true;
                    }
                    break;
                // Both are handled before the click path: the select reports
                // through its own result, and a separator is not a control.
                case EditorAction::BlockStyle:
                case EditorAction::Separator: break;
            }
        }
        (void)barScope;

        Style rule;
        rule.height = 1.0f;
        rule.shrink = 0.0f;
        rule.background = Fill{Token::Border};
        ui.add(rule);
    }

    // ---- typing ------------------------------------------------------------
    if (focused) {
        const std::size_t before = state.edit.text.size();
        const std::size_t caretBefore = state.edit.selectionStart();

        bool split = false;
        bool merge = false;
        for (const KeyEvent& event : input.keys()) {
            if (event.key == Key::Return) split = true;
            if (event.key == Key::Backspace && state.edit.caret == 0 &&
                !state.edit.hasSelection() && state.block > 0) {
                merge = true;
            }
        }

        // ---- moving by *visual* line -------------------------------------
        //
        // Up and Down are about what the reader can see, not about the string:
        // a paragraph that wraps to three lines has to be walked in three
        // steps. So the block is broken the same way it will be drawn, the
        // caret's x is measured on its own line, and the nearest offset on the
        // neighbouring line takes it. Only when there is no neighbouring line
        // does it fall through to the block above or below.
        std::vector<KeyEvent> forEditor;
        for (const KeyEvent& event : input.keys()) {
            const bool vertical = event.key == Key::Up || event.key == Key::Down;
            if (!vertical || split || merge) {
                forEditor.push_back(event);
                continue;
            }
            const float boxWidth = input.frameOf(std::string(id) + ".text." +
                                                 std::to_string(state.block)).width;
            if (boxWidth <= 0.0f) continue;

            const Block& current = document.blocks[state.block];
            const std::vector<LineRange> lines = linesOf(ui, current, boxWidth);
            const std::size_t line = lineAt(lines, state.edit.caret);
            const float wanted = widthOf(ui, current, lines[line].from, state.edit.caret);

            if (event.key == Key::Up && line > 0) {
                state.edit.caret = offsetNear(ui, current, lines[line - 1], wanted);
            } else if (event.key == Key::Down && line + 1 < lines.size()) {
                state.edit.caret = offsetNear(ui, current, lines[line + 1], wanted);
            } else if (event.key == Key::Up && state.block > 0) {
                --state.block;
                const Block& above = document.blocks[state.block];
                const float aboveWidth =
                    input.frameOf(std::string(id) + ".text." + std::to_string(state.block)).width;
                const std::vector<LineRange> aboveLines =
                    linesOf(ui, above, aboveWidth > 0.0f ? aboveWidth : boxWidth);
                state.edit.text = above.text;
                state.edit.caret = offsetNear(ui, above, aboveLines.back(), wanted);
            } else if (event.key == Key::Down && state.block + 1 < document.blocks.size()) {
                ++state.block;
                const Block& below = document.blocks[state.block];
                const float belowWidth =
                    input.frameOf(std::string(id) + ".text." + std::to_string(state.block)).width;
                const std::vector<LineRange> belowLines =
                    linesOf(ui, below, belowWidth > 0.0f ? belowWidth : boxWidth);
                state.edit.text = below.text;
                state.edit.caret = offsetNear(ui, below, belowLines.front(), wanted);
            } else {
                continue;  // at the very top or bottom: nowhere to go
            }
            if (!event.modifiers.shift) state.edit.anchor = state.edit.caret;
            state.pending = Mark::None;
        }

        if (split) {
            // Return cuts the block in two at the caret; the marks after the cut
            // travel with the second half.
            Block tail;
            tail.type = active.type == BlockType::Heading1 || active.type == BlockType::Heading2 ||
                                active.type == BlockType::Heading3
                            ? BlockType::Paragraph  // a heading's Return starts a paragraph
                            : active.type;
            const std::size_t at = state.edit.caret;
            tail.text = active.text.substr(at);
            for (const MarkRange& range : active.marks) {
                if (range.to <= at) continue;
                tail.marks.push_back({range.from > at ? range.from - at : 0, range.to - at,
                                      range.marks, range.href});
            }
            active.text.erase(at);
            active.marks.erase(std::remove_if(active.marks.begin(), active.marks.end(),
                                              [&](const MarkRange& r) { return r.from >= at; }),
                               active.marks.end());
            for (MarkRange& range : active.marks) range.to = std::min(range.to, at);

            document.blocks.insert(document.blocks.begin() +
                                       static_cast<std::ptrdiff_t>(state.block) + 1,
                                   std::move(tail));
            ++state.block;
            state.edit.text = document.blocks[state.block].text;
            state.edit.caret = 0;
            state.edit.anchor = 0;
            result.changed = true;
        } else if (merge) {
            Block& previous = document.blocks[state.block - 1];
            const std::size_t join = previous.text.size();
            for (const MarkRange& range : active.marks) {
                previous.marks.push_back({range.from + join, range.to + join, range.marks,
                                          range.href});
            }
            previous.text += active.text;
            document.blocks.erase(document.blocks.begin() +
                                  static_cast<std::ptrdiff_t>(state.block));
            --state.block;
            state.edit.text = document.blocks[state.block].text;
            state.edit.caret = join;
            state.edit.anchor = join;
            result.changed = true;
        } else {
            const TextEditResult edit = applyInput(state.edit, forEditor, input.text());
            if (edit.changed) {
                const std::size_t after = state.edit.text.size();
                const std::size_t removed =
                    before > after ? before - after + (state.edit.caret - caretBefore) : 0;
                const std::size_t inserted =
                    after > before ? after - before + (caretBefore - state.edit.caret) : 0;
                shiftMarks(active, std::min(caretBefore, state.edit.caret), removed, inserted);
                active.text = state.edit.text;
                // Anything armed on the toolbar lands on what was just typed.
                if (any(state.pending) && state.edit.caret > caretBefore) {
                    applyMark(active, caretBefore, state.edit.caret, state.pending, true, {});
                    state.pending = Mark::None;
                }
                result.changed = true;
            }
            if (edit.moved) state.pending = Mark::None;
        }
    }

    // ---- the blocks --------------------------------------------------------
    ScrollOptions view;
    view.axis = ScrollAxis::Vertical;
    view.padding = Edges::all(12.0f);
    view.gap = 6.0f;
    auto body = scrollArea(ui, input, bodyId, state.view, view);

    int numbered = 0;
    for (std::size_t i = 0; i < document.blocks.size(); ++i) {
        Block& block = document.blocks[i];
        numbered = block.type == BlockType::Numbered ? numbered + 1 : 0;
        const std::string blockId = std::string(id) + ".block." + std::to_string(i);
        const bool here = i == state.block && focused;

        Style row;
        row.direction = Direction::Row;
        row.gap = 8.0f;
        row.shrink = 0.0f;
        row.padding = block.type == BlockType::Quote || block.type == BlockType::Code
                          ? Edges{4.0f, 8.0f, 4.0f, 10.0f}
                          : Edges{};
        if (block.type == BlockType::Code) {
            row.background = Fill{Token::BgOverlay};
            row.radius = 4.0f;
        }
        row.cursorHint = Cursor::Text;
        auto rowScope = ui.scope(row);
        ui.tag(blockId).cursor(Cursor::Text);

        if (block.type == BlockType::Quote) {
            Style bar;
            bar.width = 3.0f;
            bar.shrink = 0.0f;
            bar.radius = 2.0f;
            bar.background = Fill{Token::BorderStrong};
            ui.add(bar);
        }
        if (block.type == BlockType::Bullet || block.type == BlockType::Numbered) {
            Style marker;
            marker.width = 20.0f;
            marker.shrink = 0.0f;
            marker.justify = Justify::End;
            auto markerScope = ui.scope(marker);
            text(ui, block.type == BlockType::Bullet ? "•" : std::to_string(numbered) + ".",
                 {.color = Token::TextMuted, .size = 12.0f});
            (void)markerScope;
        }

        // The text, laid out **line by line**.
        //
        // One wrapping run would draw the same pixels, but the caret needs to
        // know where the lines are — so the block is broken here, once, and the
        // spans of each line are drawn as their own row. That is also what lets
        // the caret sit at a measured point inside a wrapped paragraph rather
        // than at the end of it.
        const std::string textId = std::string(id) + ".text." + std::to_string(i);
        const float boxWidth = input.frameOf(textId).width;

        Style textBox;
        textBox.direction = Direction::Column;
        textBox.grow = 1.0f;
        textBox.basis = 0.0f;
        auto textScope = ui.scope(textBox);
        ui.tag(textId).ignoresPointer();

        const TextStyle base = styleFor(block.type, Mark::None);
        const float lineHeight = ui.canMeasure() ? ui.measure("Ag", base).height : 16.0f;
        const std::vector<LineRange> lines =
            boxWidth > 0.0f ? linesOf(ui, block, boxWidth) : std::vector<LineRange>{{0, block.text.size()}};

        if (block.text.empty()) {
            text(ui, i == 0 ? options.placeholder : std::string_view{},
                 {.color = Token::TextMuted, .size = 13.0f});
        } else {
            for (const LineRange& line : lines) {
                std::vector<TextSpan> spans;
                for (const std::size_t cut : boundaries(block)) {
                    if (cut >= line.to) continue;
                    const std::size_t from = std::max(cut, line.from);
                    if (from >= line.to) continue;
                    // The next boundary after this one, clamped to the line.
                    std::size_t to = line.to;
                    for (const std::size_t next : boundaries(block)) {
                        if (next > from && next < to) to = next;
                    }
                    if (to <= from) continue;
                    const Mark marks = block.marksAt(from);
                    const TextStyle style = styleFor(block.type, marks);
                    spans.push_back({std::string_view(block.text).substr(from, to - from),
                                     any(marks & Mark::Hyperlink) ? Token::Accent
                                     : block.type == BlockType::Quote ? Token::TextMuted
                                                                      : Token::Text,
                                     {}, style.weight, style.slant, style.role, style.size,
                                     style.decoration.underline, style.decoration.strikeThrough});
                }
                if (spans.empty()) {
                    spans.push_back({std::string_view(block.text).substr(line.from,
                                                                        line.to - line.from)});
                }
                richText(ui, spans);
            }
        }

        // The selection, one band per line it covers — and each band is as tall
        // as *its* line, so a selection running from a heading into a paragraph
        // is not one uniform stripe over two different sizes.
        if (here && boxWidth > 0.0f && state.edit.hasSelection()) {
            const std::size_t from = state.edit.selectionStart();
            const std::size_t to = state.edit.selectionEnd();
            for (std::size_t l = 0; l < lines.size(); ++l) {
                const std::size_t start = std::max(from, lines[l].from);
                const std::size_t end = std::min(to, lines[l].to);
                if (start >= end) continue;
                Style band;
                band.position = Position::Absolute;
                band.left = std::round(widthOf(ui, block, lines[l].from, start));
                band.top = static_cast<float>(l) * lineHeight;
                band.width = std::max(1.0f, widthOf(ui, block, start, end));
                band.height = lineHeight;
                band.radius = 2.0f;
                band.background = Fill{Token::Accent, 0.30f};
                ui.add(band);
            }
        }

        // The caret, at the measured point on its own line — and blinking, on
        // the same clock and the same rule as the text field's: solid while
        // anything is happening, flashing once the reader stops.
        const bool busy = !input.text().empty() || !input.keys().empty() ||
                          input.dragging() == blockId;
        const float caretMoved = static_cast<float>(state.edit.caret);
        const bool moved = ui.latch(id, "caret.pos", caretMoved, false) != caretMoved;
        ui.latch(id, "caret.pos", caretMoved, true);
        const float since = ui.now() - ui.latch(id, "caret.beat", ui.now(), busy || moved);
        constexpr float kBlink = 1.06f;
        const bool showCaret = !ui.animator() || std::fmod(since, kBlink) < kBlink / 2.0f;

        if (here && showCaret && boxWidth > 0.0f) {
            const std::size_t line = lineAt(lines, state.edit.caret);
            const float x = widthOf(ui, block, lines[line].from, state.edit.caret);

            Style caret;
            caret.position = Position::Absolute;
            // Snapped, for the same reason the text field's is: a hairline on a
            // fractional x is antialiased across two columns and looks like it
            // changes width as it moves.
            caret.left = std::round(x);
            caret.top = static_cast<float>(line) * lineHeight + lineHeight * 0.08f;
            caret.width = std::max(1.0f, std::round(lineHeight / 14.0f));
            caret.height = lineHeight * 0.9f;
            caret.zIndex = 1;
            caret.background = Fill{Token::Accent};
            ui.add(caret);
        }
        // Where the pointer is, as an offset into this block: the line from the
        // y, the offset from the x.
        const auto offsetUnderPointer = [&] {
            const Rect textFrame = input.frameOf(textId);
            if (textFrame.empty() || lines.empty()) return block.text.size();
            const float localY = input.pointer().y - textFrame.y;
            const auto whichLine = static_cast<std::size_t>(
                std::clamp(std::floor(localY / std::max(1.0f, lineHeight)), 0.0f,
                           static_cast<float>(lines.size() - 1)));
            return offsetNear(ui, block, lines[whichLine], input.pointer().x - textFrame.x);
        };

        // A press puts the caret where it landed; holding and moving drags a
        // selection out from there — the same gesture the text field has, and
        // the reason both need the offset-under-pointer above rather than a
        // guess at the end of the block.
        if (input.pressStarted(blockId)) {
            state.block = i;
            state.edit.text = block.text;
            const std::size_t at = offsetUnderPointer();
            state.edit.caret = at;
            state.edit.anchor = at;
            state.pending = Mark::None;
        } else if (input.dragging() == blockId && state.block == i) {
            // Only the caret moves: the anchor is where the press landed, which
            // is what makes the selection grow in both directions.
            state.edit.caret = offsetUnderPointer();
        }

    }
    (void)body;

    // Keep the caret in the view.
    //
    // Only when it *moves*, and that guard is the whole of it: run every frame,
    // this drags the view back the instant a reader scrolls away from where
    // they are typing, and the scrollbar becomes unusable. Blocks are not a
    // uniform height — a heading and a code block differ — so `revealRow` is no
    // help and the block's own frame is what gets read.
    const float here = static_cast<float>(state.block) * 1e6f +
                       static_cast<float>(state.edit.caret);
    const bool moved = ui.latch(id, "caret.where", here, false) != here;
    ui.latch(id, "caret.where", here, true);
    if (moved) {
        const Rect viewport = input.frameOf(bodyId);
        const Rect block =
            input.frameOf(std::string(id) + ".block." + std::to_string(state.block));
        if (!viewport.empty() && !block.empty()) {
            // Back into content space: the frames are in window coordinates and
            // the offset is how far the content has already been moved.
            const float top = block.y - viewport.y + state.view.offset;
            const float bottom = top + block.height;
            if (top < state.view.offset) {
                state.view.offset = std::max(0.0f, top);
            } else if (bottom > state.view.offset + viewport.height) {
                state.view.offset = bottom - viewport.height;
            }
        }
    }
    (void)scope;

    return result;
}

}  // namespace gbui
