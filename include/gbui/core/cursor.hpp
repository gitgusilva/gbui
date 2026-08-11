// What the pointer looks like.
//
// It lives in `core` because `style` names one and `input` reports one, and a
// vocabulary type shared by two modules belongs below both.
//
// A cursor is feedback, not decoration: an arrow over a resize handle tells the
// user the wrong thing, and a hand over plain text tells them something is
// clickable when it is not. So it is a property of a node — the node under the
// pointer decides — rather than something an application sets by hand.
#pragma once

namespace gbui {

enum class Cursor {
    Default,
    /** Anything clickable that is not a link: a button, a row, a menu item. */
    Pointer,
    /** Over editable or selectable text. */
    Text,
    /** A link, when the platform draws it differently from Pointer. */
    Hand,
    /** Something being dragged, and something that can be. */
    Grab,
    Grabbing,
    ResizeHorizontal,
    ResizeVertical,
    ResizeDiagonalUp,    ///< north-east / south-west
    ResizeDiagonalDown,  ///< north-west / south-east
    /** The action is not available here. */
    NotAllowed,
    /** Work in progress; the UI still responds. */
    Progress,
    Wait,
    Crosshair,
    Help,
};

}  // namespace gbui
