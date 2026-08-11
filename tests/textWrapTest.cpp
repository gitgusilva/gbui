#include "gbui/layout/textWrap.hpp"

#include <limits>
#include <string>

#include "gbui/layout/layout.hpp"
#include "gbui/paint/paint.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/style/theme.hpp"
#include "gbui/widgets/text.hpp"
#include "harness.hpp"

using namespace gbui;

namespace {

/** One unit per character, counted in code points so the UTF-8 cases below
 *  measure what they draw. A real font is not needed to assert where a break
 *  goes — only a width that grows with the text. */
float unitWidth(std::string_view run) {
    std::size_t glyphs = 0;
    for (const char ch : run) {
        if ((static_cast<unsigned char>(ch) & 0xC0U) != 0x80U) ++glyphs;
    }
    return static_cast<float>(glyphs);
}

std::string joined(const WrappedText& wrapped) {
    std::string out;
    for (const TextLine& line : wrapped.lines) {
        if (!out.empty()) out += "|";
        out += std::string(line.text);
    }
    return out;
}

/** The approximate measurer's advance, so a test can say how wide a box has to
 *  be for a given number of characters. */
constexpr float kSize = 10.0f;
constexpr float kAdvance = 0.52f * kSize;
constexpr float kLine = 1.45f * kSize;

TextOptions wrapping(int maxLines = 0) {
    TextOptions options;
    options.size = kSize;
    options.overflow = TextOverflow::Wrap;
    options.maxLines = maxLines;
    return options;
}

LayoutContext contextFor(const Theme& theme) {
    LayoutContext context;
    context.theme = &theme;
    return context;
}

}  // namespace

TEST("wrapping breaks at spaces, greedily") {
    const WrappedText wrapped = wrapText("aaa bbb ccc ddd", 7.0f, 0, unitWidth);
    CHECK_EQ(joined(wrapped), std::string("aaa bbb|ccc ddd"));
    CHECK_NEAR(wrapped.widest, 7.0f);
    CHECK(!wrapped.truncated);
}

TEST("the spaces at a break are not drawn") {
    // Two spaces between the words: neither ends the first line nor starts the
    // second, which is what keeps a centred paragraph looking centred.
    const WrappedText wrapped = wrapText("aaa   bbb", 4.0f, 0, unitWidth);
    CHECK_EQ(joined(wrapped), std::string("aaa|bbb"));
}

TEST("a newline always breaks, and a blank line keeps its height") {
    const WrappedText wrapped = wrapText("one\n\ntwo", 100.0f, 0, unitWidth);
    CHECK_EQ(wrapped.lines.size(), std::size_t{3});
    CHECK_EQ(joined(wrapped), std::string("one||two"));
}

TEST("carriage returns do not become a glyph") {
    const WrappedText wrapped = wrapText("one\r\ntwo", 100.0f, 0, unitWidth);
    CHECK_EQ(joined(wrapped), std::string("one|two"));
}

TEST("an unbounded width breaks only on newlines") {
    const WrappedText wrapped =
        wrapText("a very long line indeed", std::numeric_limits<float>::infinity(), 0, unitWidth);
    CHECK_EQ(wrapped.lines.size(), std::size_t{1});
    CHECK_NEAR(wrapped.widest, 23.0f);
}

TEST("a word wider than the line is broken between characters") {
    const WrappedText wrapped = wrapText("aaaaaaa", 3.0f, 0, unitWidth);
    CHECK_EQ(joined(wrapped), std::string("aaa|aaa|a"));
}

TEST("breaking a long word lands on a character boundary") {
    // Six two-byte code points; a break in the middle of one would produce a
    // replacement character on screen.
    const WrappedText wrapped = wrapText("ééééé", 2.0f, 0, unitWidth);
    CHECK_EQ(wrapped.lines.size(), std::size_t{3});
    for (const TextLine& line : wrapped.lines) {
        CHECK_EQ(line.text.size() % 2, std::size_t{0});
    }
    CHECK_EQ(joined(wrapped), std::string("éé|éé|é"));
}

TEST("a line too narrow for one character still advances") {
    const WrappedText wrapped = wrapText("abc", 0.5f, 0, unitWidth);
    CHECK_EQ(joined(wrapped), std::string("a|b|c"));
}

TEST("maxLines clamps the run and says it was cut") {
    const WrappedText wrapped = wrapText("aaa bbb ccc ddd", 7.0f, 1, unitWidth);
    CHECK_EQ(joined(wrapped), std::string("aaa bbb"));
    CHECK(wrapped.truncated);
}

TEST("a run that ends exactly on the last allowed line is not truncated") {
    const WrappedText wrapped = wrapText("aaa bbb", 7.0f, 1, unitWidth);
    CHECK_EQ(joined(wrapped), std::string("aaa bbb"));
    CHECK(!wrapped.truncated);
}

TEST("a wrapped paragraph is as tall as the lines it needs") {
    Arena arena;
    Ui ui(arena);
    NodeId paragraph;
    {
        auto column = ui.beginColumn({.width = 40.0f});
        paragraph = text(ui, "aaa bbb ccc", wrapping());
        (void)column;
    }

    const Theme theme = Theme::dark();
    layout(arena, ui.root(), Rect{0, 0, 40, 400}, contextFor(theme));

    // "aaa bbb" is 7 glyphs — 36.4 — and fits; the third word does not.
    CHECK_NEAR(arena[paragraph].frame.height, 2.0f * kLine);
}

TEST("narrowing a paragraph makes it taller") {
    const auto heightAt = [](float width) {
        Arena arena;
        Ui ui(arena);
        NodeId paragraph;
        {
            auto column = ui.beginColumn({.width = width});
            paragraph = text(ui, "aaa bbb ccc", wrapping());
            (void)column;
        }
        const Theme theme = Theme::dark();
        layout(arena, ui.root(), Rect{0, 0, width, 400}, contextFor(theme));
        return arena[paragraph].frame.height;
    };

    CHECK_NEAR(heightAt(60.0f), 1.0f * kLine);
    CHECK_NEAR(heightAt(40.0f), 2.0f * kLine);
    CHECK_NEAR(heightAt(20.0f), 3.0f * kLine);
}

TEST("padding comes off the width before the text is wrapped") {
    Arena arena;
    Ui ui(arena);
    NodeId paragraph;
    {
        auto column = ui.beginColumn({.width = 40.0f});
        TextStyle textStyle;
        textStyle.size = kSize;
        textStyle.overflow = TextOverflow::Wrap;
        // 10 px of padding leaves 30 for the text, which no two of these words
        // fit into. Measuring against the frame instead would say two lines.
        paragraph = ui.label("aaa bbb ccc", textStyle, Style{.padding = Edges::all(5.0f)});
        (void)column;
    }

    const Theme theme = Theme::dark();
    layout(arena, ui.root(), Rect{0, 0, 40, 400}, contextFor(theme));

    CHECK_NEAR(arena[paragraph].frame.height, 3.0f * kLine + 10.0f);
}

TEST("a row is as tall as a paragraph at the width that paragraph gets") {
    Arena arena;
    Ui ui(arena);
    NodeId row;
    {
        auto column = ui.beginColumn({.width = 100.0f});
        {
            auto scope = ui.beginRow({});
            row = scope.id();
            ui.add({.width = 40.0f});  // the label beside it
            TextOptions options = wrapping();
            options.grow = 1.0f;
            text(ui, "aaa bbb ccc ddd", options);
        }
        (void)column;
    }

    const Theme theme = Theme::dark();
    layout(arena, ui.root(), Rect{0, 0, 100, 400}, contextFor(theme));

    // The run gets the 60 left over, where it needs two lines. Measured against
    // the row's own 100 it would fit on one, and the row would be half as tall
    // as the text drawn inside it.
    CHECK_NEAR(arena[row].frame.height, 2.0f * kLine);
}

TEST("a wrapped run reports the longest word as its minimum width") {
    Arena arena;
    Ui ui(arena);
    NodeId paragraph;
    {
        auto row = ui.beginRow({});
        paragraph = text(ui, "a longestword b", wrapping());
        (void)row;
    }

    const Theme theme = Theme::dark();
    // Far narrower than the text: without min-content sizing the run would be
    // squeezed to a sliver, and every line would then be a broken word.
    layout(arena, ui.root(), Rect{0, 0, 10, 200}, contextFor(theme));

    CHECK_NEAR(arena[paragraph].frame.width, 11.0f * kAdvance);
}

TEST("painting a wrapped run draws one command per line") {
    Arena arena;
    Ui ui(arena);
    {
        auto column = ui.beginColumn({.width = 40.0f});
        text(ui, "aaa bbb ccc", wrapping());
        (void)column;
    }

    const Theme theme = Theme::dark();
    layout(arena, ui.root(), Rect{0, 0, 40, 400}, contextFor(theme));

    DisplayList list;
    record(arena, ui.root(), theme, list);

    std::vector<const DrawText*> runs;
    for (const DrawCommand& command : list.commands()) {
        if (const auto* draw = std::get_if<DrawText>(&command)) runs.push_back(draw);
    }

    CHECK_EQ(runs.size(), std::size_t{2});
    if (runs.size() < 2) return;
    CHECK_EQ(std::string(runs[0]->text), std::string("aaa bbb"));
    CHECK_EQ(std::string(runs[1]->text), std::string("ccc"));
    // Each line sits in its own box, one line height below the one above it.
    CHECK_NEAR(runs[1]->box.y - runs[0]->box.y, kLine);
    CHECK_NEAR(runs[0]->baseline, runs[1]->baseline);
}

TEST("a clamped run ends with an ellipsis on its last line") {
    Arena arena;
    Ui ui(arena);
    {
        auto column = ui.beginColumn({.width = 40.0f});
        text(ui, "aaa bbb ccc ddd eee", wrapping(2));
        (void)column;
    }

    const Theme theme = Theme::dark();
    layout(arena, ui.root(), Rect{0, 0, 40, 400}, contextFor(theme));

    DisplayList list;
    record(arena, ui.root(), theme, list);

    std::vector<std::string> runs;
    for (const DrawCommand& command : list.commands()) {
        if (const auto* draw = std::get_if<DrawText>(&command)) runs.emplace_back(draw->text);
    }

    CHECK_EQ(runs.size(), std::size_t{2});
    if (runs.size() < 2) return;
    CHECK_EQ(runs[0], std::string("aaa bbb"));
    CHECK(runs[1].find("\xe2\x80\xa6") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Where a line is allowed to end
// ---------------------------------------------------------------------------

/**
 * The gap this closes: only a space used to be a break opportunity, so a path
 * or a URL was never broken *at* a separator. It reached the emergency
 * character-level rule instead and was cut mid-segment.
 */
TEST("a path breaks after its slashes, not through its segments") {
    // Wide enough for the longest segment, so a slash is always a choice.
    const WrappedText wrapped =
        wrapText("src/widgets/richEditor.cpp", 16.0f, 0, unitWidth, WordBreak::Normal);
    CHECK(joined(wrapped) == "src/widgets/|richEditor.cpp");
}

/** The separator rule does not repeal the emergency one: a single segment too
 *  long for the line still has to be cut somewhere, and the only place left is
 *  between two characters. */
TEST("a segment longer than the line is still cut") {
    const WrappedText wrapped =
        wrapText("src/richEditor.cpp", 8.0f, 0, unitWidth, WordBreak::Normal);
    CHECK(wrapped.lines.front().text == "src/");
    CHECK(wrapped.lines.size() > 2);
    CHECK(wrapped.widest <= 8.0f);
}

TEST("a hyphenated word breaks after the hyphen") {
    const WrappedText wrapped =
        wrapText("well-behaved-wrapping", 12.0f, 0, unitWidth, WordBreak::Normal);
    for (std::size_t i = 0; i + 1 < wrapped.lines.size(); ++i) {
        CHECK(wrapped.lines[i].text.back() == '-');
    }
}

/** The separator stays on the line it belongs to. Leading a line with "-word"
 *  reads as a list item or a negation, which is not what was written. */
TEST("a break never starts a line with the separator") {
    const WrappedText wrapped = wrapText("aaa-bbb-ccc", 6.0f, 0, unitWidth, WordBreak::Normal);
    for (const TextLine& line : wrapped.lines) {
        CHECK(line.text.front() != '-');
        CHECK(line.text.front() != '/');
    }
}

TEST("keep-all lets a long word overflow rather than cutting it") {
    const WrappedText kept =
        wrapText("3f9a2c7e1b8d4056", 8.0f, 0, unitWidth, WordBreak::KeepAll);
    CHECK(joined(kept) == "3f9a2c7e1b8d4056");
    CHECK(kept.widest > 8.0f);   // it really did overflow, on purpose

    // The same text under the default gets cut to fit instead.
    const WrappedText cut = wrapText("3f9a2c7e1b8d4056", 8.0f, 0, unitWidth, WordBreak::Normal);
    CHECK(cut.lines.size() > 1);
    CHECK(cut.widest <= 8.0f);
}

TEST("keep-all still wraps at spaces") {
    const WrappedText wrapped =
        wrapText("alpha beta gamma", 6.0f, 0, unitWidth, WordBreak::KeepAll);
    CHECK(joined(wrapped) == "alpha|beta|gamma");
}

TEST("anywhere fills each line to the edge") {
    const WrappedText wrapped =
        wrapText("abcdefghij", 4.0f, 0, unitWidth, WordBreak::Anywhere);
    CHECK(joined(wrapped) == "abcd|efgh|ij");
}

/** Whatever the policy, wrapping must terminate and must not lose or invent
 *  characters — the two ways a greedy wrapper goes wrong. */
TEST("no policy loses a character") {
    const std::string text = "a-b/c ddddddddddd e";
    for (const WordBreak policy : {WordBreak::Normal, WordBreak::KeepAll, WordBreak::Anywhere}) {
        const WrappedText wrapped = wrapText(text, 5.0f, 0, unitWidth, policy);
        std::string seen;
        for (const TextLine& line : wrapped.lines) seen += std::string(line.text);
        // Spaces are dropped at breaks; nothing else may be.
        std::string expected;
        for (const char ch : text) {
            if (ch != ' ') expected += ch;
        }
        std::string got;
        for (const char ch : seen) {
            if (ch != ' ') got += ch;
        }
        CHECK(got == expected);
    }
}
