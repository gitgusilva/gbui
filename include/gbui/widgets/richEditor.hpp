// A rich text editor: blocks of text, marks over ranges, and a toolbar.
#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "gbui/input/interaction.hpp"
#include "gbui/input/textEdit.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/icons.hpp"
#include "gbui/widgets/scroll.hpp"
#include "gbui/widgets/select.hpp"

namespace gbui {

/** What can be true of a run of characters. A bitmask, because they combine —
 *  bold *and* italic is one run, not two nested ones. */
enum class Mark : unsigned {
    None = 0,
    Bold = 1u << 0,
    Italic = 1u << 1,
    Underline = 1u << 2,
    Strike = 1u << 3,
    Code = 1u << 4,
    Hyperlink = 1u << 5,
};

constexpr Mark operator|(Mark a, Mark b) {
    return static_cast<Mark>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}
constexpr Mark operator&(Mark a, Mark b) {
    return static_cast<Mark>(static_cast<unsigned>(a) & static_cast<unsigned>(b));
}
constexpr Mark operator~(Mark a) { return static_cast<Mark>(~static_cast<unsigned>(a)); }
constexpr bool any(Mark a) { return static_cast<unsigned>(a) != 0; }

/**
 * A stretch of one block wearing the same marks.
 *
 * Ranges over the text rather than a tree of nested spans, which is the model
 * every editor that survived contact with users ended up at — Quill's deltas,
 * ProseMirror's marks. Nesting looks natural until a bold run and a link
 * overlap by half, and then it is two trees that cannot both be right.
 * Offsets are in bytes, on character boundaries.
 */
struct MarkRange {
    std::size_t from = 0;
    std::size_t to = 0;
    Mark marks = Mark::None;
    /** Where a `Hyperlink` points. Ignored otherwise. */
    std::string href{};
};

enum class BlockType { Paragraph, Heading1, Heading2, Heading3, Bullet, Numbered, Quote, Code };

struct Block {
    BlockType type = BlockType::Paragraph;
    std::string text{};
    std::vector<MarkRange> marks{};

    /** The marks covering a byte offset. */
    Mark marksAt(std::size_t offset) const;
};

struct RichDocument {
    std::vector<Block> blocks{Block{}};

    /** The whole document as plain text, one block per line. What a caller
     *  saves when it does not want the formatting. */
    std::string plainText() const;
};

/** What the editor remembers. Owned by the application, like every other piece
 *  of state here. */
struct RichEditorState {
    /** Which block has the keyboard. */
    std::size_t block = 0;
    /** The caret and selection *within* that block. */
    TextEditState edit{};
    ScrollState view{};
    /** Marks the next typed character will wear — set by pressing bold with
     *  nothing selected, which is what every editor does. */
    Mark pending = Mark::None;
    /** The `BlockStyle` dropdown's own open/highlight state. */
    SelectState blockStyle{};
};

/** What a toolbar button does. `Custom` runs the caller's own callback. */
enum class EditorAction {
    Bold, Italic, Underline, Strike, Code, Hyperlink,
    Paragraph, Heading1, Heading2, Heading3, Bullet, Numbered, Quote, CodeBlock,
    /**
     * A dropdown that sets the block's type, instead of one button per heading.
     *
     * Which is what every editor with more than three heading levels settles
     * on: the buttons are mutually exclusive — a block is a heading *or* a
     * paragraph, never both — and a row of exclusive buttons is a select
     * wearing the wrong clothes. It also reads the current block back, so the
     * toolbar says what the caret is sitting in.
     *
     * The individual `Heading1`…`Heading3` actions still work for a caller that
     * prefers the buttons.
     */
    BlockStyle,
    Separator,
    Custom,
};

/** One entry of the `BlockStyle` dropdown. */
struct BlockChoice {
    BlockType type = BlockType::Paragraph;
    std::string_view label{};
};

/** What `BlockStyle` offers when a caller does not say. */
std::vector<BlockChoice> defaultBlockChoices();

struct ToolbarItem {
    EditorAction action = EditorAction::Custom;
    Icon icon = Icon::Check;
    /** Shown instead of an icon when it is not empty — "H1", "H2". */
    std::string_view label{};
    std::string_view tooltip{};
    /** For `Custom`: what the button does. It is handed the document and the
     *  state, which is everything there is. */
    std::function<void(RichDocument&, RichEditorState&)> onClick{};
};

/** The set a caller gets when it asks for no particular one. */
std::vector<ToolbarItem> defaultToolbar();

struct RichEditorOptions {
    /** Empty takes `defaultToolbar()`. Anything else is exactly what is drawn,
     *  in that order — which is how a caller adds its own buttons, drops the
     *  ones it does not want, or reorders the rest. */
    std::vector<ToolbarItem> toolbar{};
    /** What the `BlockStyle` dropdown lists. Empty takes
     *  `defaultBlockChoices()`. */
    std::vector<BlockChoice> blockChoices{};
    bool showToolbar = true;
    std::string_view placeholder = "Write something…";
    float minHeight = 220.0f;
    float height = kAuto;
    float grow = 0.0f;
};

struct RichEditorResult {
    bool changed = false;
};

/**
 * Draws the editor and edits `document` in place.
 *
 * **What this is and is not.** It is a block editor: paragraphs, headings,
 * lists, quotes and code blocks, with bold, italic, underline, strikethrough,
 * inline code and links over ranges within a block. Typing, splitting a block
 * with Return and merging with Backspace all work, and the toolbar is the
 * caller's to compose.
 *
 * It is **not** finished, and the gaps are worth naming rather than
 * discovering: no images — the painter cannot decode one yet; no undo; the
 * caret moves by character and by block, not by *visual line*, so a block that
 * wraps is edited by a caret that does not know where the lines are; and there
 * is no nesting of lists. Each of those is a piece of work of its own, and
 * three of them want engine features that do not exist.
 */
RichEditorResult richEditor(Ui& ui, const Interaction& input, std::string_view id,
                            RichDocument& document, RichEditorState& state,
                            const RichEditorOptions& options = {});

}  // namespace gbui
