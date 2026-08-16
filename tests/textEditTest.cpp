#include "gbui/input/textEdit.hpp"

#include <string>

#include "harness.hpp"

using namespace gbui;

namespace {

KeyEvent key(Key which, bool shift = false, bool ctrl = false) {
    KeyEvent event;
    event.key = which;
    event.modifiers.shift = shift;
    event.modifiers.ctrl = ctrl;
    return event;
}

/** A shortcut on the modifier the *platform* uses for one — Ctrl, or Cmd on
 *  macOS, which is what `Modifiers::command()` answers.
 *
 *  Word movement is a different question and stays on Ctrl: `applyKey` accepts
 *  Ctrl or Alt for it everywhere. Select-all and the clipboard go through
 *  `command()`, so a test that hardcoded Ctrl passed on Linux and failed on a
 *  Mac — which is exactly the kind of thing the macOS job exists to say. */
KeyEvent commandKey(Key which, bool shift = false) {
    KeyEvent event;
    event.key = which;
    event.modifiers.shift = shift;
#if defined(__APPLE__)
    event.modifiers.super = true;
#else
    event.modifiers.ctrl = true;
#endif
    return event;
}

TextEditState fieldWith(std::string text, std::size_t caret) {
    TextEditState state;
    state.text = std::move(text);
    state.caret = state.anchor = caret;
    return state;
}

}  // namespace

TEST("typing inserts at the caret") {
    TextEditState state = fieldWith("mai", 3);
    const auto result = insertText(state, "n");

    CHECK(result.changed);
    CHECK_EQ(state.text, std::string("main"));
    CHECK_EQ(state.caret, std::size_t{4});
    CHECK(!state.hasSelection());
}

TEST("typing over a selection replaces it") {
    TextEditState state = fieldWith("feature/login", 0);
    state.anchor = 0;
    state.caret = 7;  // "feature"

    insertText(state, "fix");

    CHECK_EQ(state.text, std::string("fix/login"));
    CHECK_EQ(state.caret, std::size_t{3});
    CHECK(!state.hasSelection());
}

TEST("the caret moves over characters, not bytes") {
    // "café" — the é is two bytes, so a caret that counts bytes lands inside it.
    TextEditState state = fieldWith("caf\xc3\xa9", 5);

    applyKey(state, key(Key::Left));
    CHECK_EQ(state.caret, std::size_t{3});  // before the é, not inside it

    applyKey(state, key(Key::Right));
    CHECK_EQ(state.caret, std::size_t{5});
}

TEST("backspace deletes a whole character") {
    TextEditState state = fieldWith("caf\xc3\xa9", 5);
    const auto result = applyKey(state, key(Key::Backspace));

    CHECK(result.changed);
    CHECK_EQ(state.text, std::string("caf"));
}

TEST("shift extends the selection and a bare arrow collapses it") {
    TextEditState state = fieldWith("main", 0);

    applyKey(state, key(Key::Right, /*shift=*/true));
    applyKey(state, key(Key::Right, /*shift=*/true));
    CHECK(state.hasSelection());
    CHECK_EQ(std::string(state.selectedText()), std::string("ma"));

    // Left with no shift puts the caret at the start of the selection rather
    // than one character further left — the rule every editor follows.
    applyKey(state, key(Key::Left));
    CHECK(!state.hasSelection());
    CHECK_EQ(state.caret, std::size_t{0});
}

TEST("word movement stops at the start of a word") {
    const std::string text = "feature/nord-tuning fix";
    TextEditState state = fieldWith(text, text.size());

    applyKey(state, key(Key::Left, false, /*ctrl=*/true));
    CHECK_EQ(state.text.substr(state.caret), std::string("fix"));

    applyKey(state, key(Key::Left, false, true));
    CHECK_EQ(state.text.substr(state.caret, 6), std::string("tuning"));
}

TEST("ctrl+backspace deletes the word before the caret") {
    TextEditState state = fieldWith("origin/main", 11);
    const auto result = applyKey(state, key(Key::Backspace, false, /*ctrl=*/true));

    CHECK(result.changed);
    CHECK_EQ(state.text, std::string("origin/"));
}

TEST("home and end reach the ends, and select-all covers everything") {
    TextEditState state = fieldWith("themes/nord/theme.json", 5);

    applyKey(state, key(Key::End));
    CHECK_EQ(state.caret, state.text.size());

    applyKey(state, key(Key::Home));
    CHECK_EQ(state.caret, std::size_t{0});

    applyKey(state, commandKey(Key::A));
    CHECK(state.hasSelection());
    CHECK_EQ(std::string(state.selectedText()), state.text);
}

TEST("return and escape are reported, not swallowed") {
    TextEditState state = fieldWith("main", 4);

    CHECK(applyKey(state, key(Key::Return)).submitted);
    CHECK(applyKey(state, key(Key::Escape)).cancelled);
    CHECK_EQ(state.text, std::string("main"));  // neither changes the text
}

TEST("control characters never reach the text") {
    TextEditState state = fieldWith("", 0);
    // A frame that carries a tab and a newline alongside real typing: only the
    // printable part belongs in a single-line field.
    applyInput(state, {}, "a\tb\nc");

    CHECK_EQ(state.text, std::string("abc"));
}

TEST("a caret past the end is pulled back rather than read out of bounds") {
    TextEditState state = fieldWith("main", 99);
    state.clampToText();
    CHECK_EQ(state.caret, std::size_t{4});

    // And one left inside a multi-byte character moves to its boundary.
    TextEditState accented = fieldWith("caf\xc3\xa9", 4);
    accented.clampToText();
    CHECK_EQ(accented.caret, std::size_t{3});
}

TEST("a whole frame of input applies keys before the text it typed") {
    TextEditState state = fieldWith("main", 4);
    // Backspace then a character: the order matters, and applying the text
    // first would leave "maiX" instead of "maiX" after deleting the n.
    const auto result = applyInput(state, {key(Key::Backspace)}, "X");

    CHECK(result.changed);
    CHECK_EQ(state.text, std::string("maiX"));
}

// ---- more than one line ----------------------------------------------------

namespace {

/** Three lines, so a caret has somewhere above it and somewhere below. */
TextEditState paragraph(std::size_t caret) {
    TextEditState state;
    state.text = "first\nsecond line\nthird";
    state.caret = state.anchor = caret;
    return state;
}

constexpr TextEditOptions kMultiline{.multiline = true};

}  // namespace

TEST("a line knows where it starts and ends") {
    const std::string_view text = "first\nsecond line\nthird";
    // Inside the middle line, from anywhere in it.
    CHECK_EQ(lineStart(text, 9), std::size_t{6});
    CHECK_EQ(lineEnd(text, 9), std::size_t{17});
    // At the very start, and at the very end, where there is no newline to find.
    CHECK_EQ(lineStart(text, 0), std::size_t{0});
    CHECK_EQ(lineEnd(text, text.size()), text.size());
    // On the newline itself: it belongs to the line it terminates.
    CHECK_EQ(lineStart(text, 5), std::size_t{0});
    CHECK_EQ(lineEnd(text, 6), std::size_t{17});
}

TEST("Home and End reach the ends of the line, not of the text") {
    TextEditState state = paragraph(9);   // inside "second line"
    applyKey(state, key(Key::Home), kMultiline);
    CHECK_EQ(state.caret, std::size_t{6});
    applyKey(state, key(Key::End), kMultiline);
    CHECK_EQ(state.caret, std::size_t{17});

    // A single-line field is unchanged by any of this.
    TextEditState single = paragraph(9);
    applyKey(single, key(Key::Home));
    CHECK_EQ(single.caret, std::size_t{0});
}

TEST("Up and Down keep the column, and stop at a short line's end") {
    // Column 8 of "second line" — past the end of "first", which is 5 long.
    TextEditState state = paragraph(14);
    applyKey(state, key(Key::Up), kMultiline);
    // Clamped to the end of the shorter line rather than spilling into it.
    CHECK_EQ(state.caret, std::size_t{5});

    // Back down, from column 5 of the first line into the second.
    applyKey(state, key(Key::Down), kMultiline);
    CHECK_EQ(state.caret, std::size_t{11});

    // Off the bottom goes to the very end, and off the top to the very start —
    // a caret that refuses to move reads as the key not working.
    TextEditState last = paragraph(20);
    applyKey(last, key(Key::Down), kMultiline);
    CHECK_EQ(last.caret, last.text.size());
    TextEditState top = paragraph(3);
    applyKey(top, key(Key::Up), kMultiline);
    CHECK_EQ(top.caret, std::size_t{0});
}

TEST("Shift with Up and Down extends the selection over whole lines") {
    TextEditState state = paragraph(0);
    applyKey(state, key(Key::Down, /*shift=*/true), kMultiline);

    CHECK(state.hasSelection());
    CHECK_EQ(state.anchor, std::size_t{0});
    CHECK_EQ(state.caret, std::size_t{6});
    CHECK_EQ(std::string(state.selectedText()), std::string("first\n"));
}

TEST("Return writes a newline, and the modifier is what submits") {
    TextEditState state = paragraph(5);   // end of "first"
    const auto typed = applyKey(state, key(Key::Return), kMultiline);
    CHECK(typed.changed);
    CHECK(!typed.submitted);
    CHECK_EQ(state.text, std::string("first\n\nsecond line\nthird"));

    TextEditState done = paragraph(5);
    const auto sent = applyKey(done, commandKey(Key::Return), kMultiline);
    CHECK(sent.submitted);
    CHECK(!sent.changed);
    CHECK_EQ(done.text, std::string("first\nsecond line\nthird"));

    // And a single-line field still submits on a bare Return, as it always did.
    TextEditState single = paragraph(5);
    CHECK(applyKey(single, key(Key::Return)).submitted);
}

TEST("a pasted paragraph keeps its line breaks") {
    // The newlines arrive as *typed text*, not as keys — which is how a paste
    // reaches the model, and where they were being filtered out.
    TextEditState state;
    applyInput(state, {}, "one\ntwo\nthree", kMultiline);
    CHECK_EQ(state.text, std::string("one\ntwo\nthree"));

    // A single-line field still refuses them, which is the whole reason the
    // filter is there.
    TextEditState single;
    applyInput(single, {}, "one\ntwo");
    CHECK_EQ(single.text, std::string("onetwo"));
}
