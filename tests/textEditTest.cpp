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
    state.caret = 7;   // "feature"

    insertText(state, "fix");

    CHECK_EQ(state.text, std::string("fix/login"));
    CHECK_EQ(state.caret, std::size_t{3});
    CHECK(!state.hasSelection());
}

TEST("the caret moves over characters, not bytes") {
    // "café" — the é is two bytes, so a caret that counts bytes lands inside it.
    TextEditState state = fieldWith("caf\xc3\xa9", 5);

    applyKey(state, key(Key::Left));
    CHECK_EQ(state.caret, std::size_t{3});   // before the é, not inside it

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

    applyKey(state, key(Key::A, false, /*ctrl=*/true));
    CHECK(state.hasSelection());
    CHECK_EQ(std::string(state.selectedText()), state.text);
}

TEST("return and escape are reported, not swallowed") {
    TextEditState state = fieldWith("main", 4);

    CHECK(applyKey(state, key(Key::Return)).submitted);
    CHECK(applyKey(state, key(Key::Escape)).cancelled);
    CHECK_EQ(state.text, std::string("main"));   // neither changes the text
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
