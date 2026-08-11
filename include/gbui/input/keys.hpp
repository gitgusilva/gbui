// The keyboard vocabulary.
//
// It lives in `input` rather than in `platform` because it is the input module
// that routes keys and the components that interpret them; a backend only
// produces them. That direction — the toolkit names what it wants, the platform
// supplies it — is what keeps a second backend a translation table rather than
// a redesign.
#pragma once

#include <cstdint>

namespace gbui {

enum class Key {
    Unknown,
    // Navigation
    Left, Right, Up, Down, Home, End, PageUp, PageDown,
    // Editing
    Backspace, Delete, Return, Tab, Escape, Space,
    // Letters that carry shortcuts. Anything typed arrives as text instead, so
    // this list is only what a component reacts to as a *key*.
    A, C, V, X, Z, Y, T, D,
    // Numbers and symbols the controls use
    Plus, Minus,
};

/** Modifier state at the moment a key was pressed. */
struct Modifiers {
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
    bool super = false;

    /** The platform's "the usual shortcut modifier" — Ctrl, or Cmd on macOS. */
    bool command() const {
#if defined(__APPLE__)
        return super;
#else
        return ctrl;
#endif
    }
};

struct KeyEvent {
    Key key = Key::Unknown;
    Modifiers modifiers{};
    /** True when the event came from the platform's auto-repeat. */
    bool repeat = false;
};

}  // namespace gbui
