// What a node is, to something that cannot see it.
//
// A screen reader is handed a tree of *things* — a button, a list, a row — and
// today it is handed one blank rectangle, because a `Node` says how it is drawn
// and never what it is. This enumeration is the missing half, and it is the
// first of the ten stages in the accessibility plan because nothing else in
// that plan can start without it.
//
// ---- why these names and not our own -----------------------------------
//
// They are ARIA's, and ARIA's are also the ones AccessKit's tree model uses.
// That is deliberate: AccessKit is the intended bridge to UIA, AT-SPI and
// NSAccessibility, and picking the same vocabulary now makes that bridge a
// lookup table rather than a translation with opinions in it. Every role below
// exists in ARIA, so a reader who knows the web knows this file.
//
// Where the two disagree the comment says so. There are exactly three:
// `Label` is what a run of static text is called here and in AccessKit, where
// ARIA leaves it roleless; `TextInput` is ARIA's `textbox`, spelled to match
// the component it comes from; and `ScrollView` is AccessKit's, because ARIA
// has no word for "this region moves" and a reader needs one.
//
// ---- what is deliberately not here ---------------------------------------
//
// Roles for things this toolkit cannot build. There is no `article`, no
// `banner`, no `document`: a role that no component ever sets is a role that
// cannot be tested, and the first person to need one can add it in the same
// commit as the component that needs it.
#pragma once

#include <cstdint>
#include <string_view>

namespace gbui {

enum class Role : std::uint8_t {
    /**
     * Presentational: this node exists for layout and holds nothing a reader
     * needs to be told about.
     *
     * The default, and the right answer for most nodes — a row that spaces two
     * things out is not a thing. The accessibility tree collapses these away.
     */
    None,

    // ---- text and content ------------------------------------------------
    /** A run of static text. ARIA leaves this roleless; AccessKit calls it
     *  `Label`, and so does this. */
    Label,
    Heading,
    Paragraph,
    Image,
    Link,
    Figure,   ///< A chart, a diagram, a drawing — content with a caption.

    // ---- controls ----------------------------------------------------------
    Button,
    Checkbox,
    Radio,
    RadioGroup,
    Switch,
    Slider,
    /** A number box with steppers — ARIA's `spinbutton`. */
    SpinButton,
    /** ARIA's `textbox`, spelled after the component it comes from. */
    TextInput,
    ComboBox,
    ListBox,
    Option,
    ProgressBar,

    // ---- structure ---------------------------------------------------------
    /** A container worth announcing as one, because it has a name. An unnamed
     *  container should be `None`. */
    Group,
    Form,
    Toolbar,
    Separator,
    /** A region that moves under its own frame. AccessKit's word; ARIA has
     *  none, and a reader who cannot see a scrollbar has no other way to be
     *  told there is more below. */
    ScrollView,
    List,
    ListItem,
    Table,
    Row,
    Cell,
    ColumnHeader,
    Tree,
    TreeItem,
    TabList,
    Tab,
    TabPanel,

    // ---- overlays ----------------------------------------------------------
    Menu,
    MenuBar,
    MenuItem,
    MenuItemCheckbox,
    MenuItemRadio,
    Dialog,
    /** A dialog that interrupts, rather than one the reader asked for. */
    AlertDialog,
    Tooltip,

    // ---- live regions ------------------------------------------------------
    /** Something changed and is worth mentioning when there is a pause. */
    Status,
    /** Something changed and should interrupt. */
    Alert,
};

/** ARIA's spelling of a role — `"button"`, `"gridcell"`, `"textbox"`.
 *
 *  What the platform bridge will send and what a test asserts against, so the
 *  two cannot drift into two different names for the same thing. */
std::string_view roleName(Role role);

}  // namespace gbui
