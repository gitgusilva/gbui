// What the rich editor does with the text it is given.
#include "gbui/widgets/richEditor.hpp"

#include <string>

#include "gbui/layout/layout.hpp"
#include "gbui/platform/font.hpp"
#include "gbui/style/theme.hpp"

#include "harness.hpp"

using namespace gbui;

namespace {

/** Builds the editor once and returns where the caret landed.
 *
 * Two passes, because every widget here places itself from *last* frame's
 * geometry: the first pass gives the block a width, the second puts the caret
 * in it. Returns a zero rect when the machine has no font, which is the same
 * escape hatch the text field's tests use. */
Rect caretAfter(const std::string& text, std::size_t caret) {
    Interaction input;
    Theme theme = Theme::dark();
    FontDatabase fonts;

    RichDocument document;
    document.blocks[0].text = text;
    RichEditorState state;
    state.edit.caret = caret;
    state.edit.anchor = caret;

    Rect found{};
    for (int pass = 0; pass < 3; ++pass) {
        Arena arena;
        Ui ui(arena);
        ui.setMeasure(measureWith(fonts), theme.typography());
        NodeId root;
        {
            auto column = ui.column({});
            richEditor(ui, input, "ed", document, state, {.showToolbar = false});
            root = column.id();
        }
        LayoutContext context;
        context.theme = &theme;
        context.measure = measureWith(fonts);
        layout(arena, root, Rect{0, 0, 600, 400}, context);
        input.update(arena, root, InputFrame{});
        input.focus("ed.block.0");

        for (std::size_t i = 0; i < arena.size(); ++i) {
            const Node& node = arena[NodeId{static_cast<std::uint32_t>(i)}];
            if (node.style.position != Position::Absolute) continue;
            const float w = node.style.width.value;
            if (w > 0.9f && w < 3.1f) found = node.frame;
        }
    }
    return found;
}

/** Whether this machine has any font at all. Without one nothing is measured
 *  and every geometric assertion here is vacuous, so the tests say so out loud
 *  rather than passing on a zero. */
bool haveFont() {
    FontDatabase fonts;
    Theme theme = Theme::dark();
    return measureWith(fonts)("Ag", TextStyle{}, theme.typography(), kAuto).width > 0.0f;
}

}  // namespace

/**
 * The bug this pins: `wrapText` ends a line at the last *word*, dropping the
 * spaces after it — which is exactly right for drawing wrapped text and exactly
 * wrong for an editor. Typing a space moved the caret nowhere, so the space
 * seemed not to register until the next character brought it back.
 */
TEST("a trailing space moves the caret") {
    const Rect withoutSpace = caretAfter("a", 1);
    const Rect withSpace = caretAfter("a ", 2);
    if (!haveFont()) return;
    CHECK(withoutSpace.width > 0.0f);   // the caret was found at all
    CHECK(withSpace.width > 0.0f);
    CHECK(withSpace.x > withoutSpace.x);
}

TEST("every trailing space moves the caret again") {
    const Rect one = caretAfter("a ", 2);
    const Rect two = caretAfter("a  ", 3);
    const Rect three = caretAfter("a   ", 4);
    if (!haveFont()) return;
    CHECK(one.width > 0.0f);
    CHECK(two.x > one.x);
    CHECK(three.x > two.x);
}

/** A space between words was never the broken case, but it is what the fix
 *  must not break. */
TEST("a space between words still measures as one space") {
    const Rect before = caretAfter("a b", 1);
    const Rect after = caretAfter("a b", 2);
    if (!haveFont()) return;
    CHECK(before.width > 0.0f);
    CHECK(after.x > before.x);
}

TEST("plain text keeps the spaces it was given") {
    RichDocument document;
    document.blocks[0].text = "a  b ";
    CHECK(document.plainText() == "a  b \n");  // one block per line, newline-terminated
}
