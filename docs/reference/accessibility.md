# a11y

`#include "gbui/a11y/role.hpp"`, `accessibility.hpp`

What a node is, to somebody who cannot see it. A `Style` says how a node is
drawn and a `Role` says what it *is* — and until this existed, a screen reader
was handed one blank rectangle where an application should have been.

**This is stages 1 to 3 of the accessibility plan**: a role and a name on every
control, the state and value that go with it, and the relations that tie a
caption to its field and an error to its input. What it is **not** yet is a
platform bridge — nothing is sent to UIA, AT-SPI or NSAccessibility. That is
stage 5, and it needs the pruned tree of stage 4 in front of it. Everything here
is in the arena and testable today, which is exactly how it should be built.

## The rule

Every component that is added or changed in this repository carries working
accessibility in the same commit. It is [rule 7 in
CONTRIBUTING](https://github.com/gitgusilva/gbui/blob/main/CONTRIBUTING.md), and
`tests/accessibilityTest.cpp` is the gate: its last case walks a form and fails
on any Tab stop with no role, or with nothing to announce.

## Setting it

```cpp
ui.tag(id).focusable(!options.disabled).accessible({
    .role = Role::Checkbox,
    .name = options.label,
    .state = {.checked = flag(checked), .disabled = flag(options.disabled)},
});
```

Two shorthands exist for the nodes that need nothing else — `ui.role(Role::Row)`
and `ui.name("Commits")` — and an overload takes a `NodeId`, for the component
that only learns something after its own subtree is closed.

**Calls merge, they do not replace.** A field that was not given is "I had
nothing to say about this", never "set it back to the default", so a component
can state its role and a wrapper can add the relation that names it. Every
string is interned, so `std::string(id) + ".error"` is safe to pass.

**It is not stored on the node.** A `Node` is a trivial aggregate so clearing the
arena is a size reset, and most nodes have nothing to say — a row that spaces two
things out is not a thing. So the record lives in a side table the arena owns and
a node names by index, exactly as vector art already does: four bytes on every
node, the full record only on the ones that have one. `arena.accessibility(id)`
reads it back, and returns null for the majority that never set one.

## Roles

The names are ARIA's, and ARIA's are also what AccessKit's tree model uses —
picking the same vocabulary now makes stage 5 a lookup table rather than a
translation with opinions in it. `roleName()` gives the ARIA spelling, which is
what a test asserts against and what the bridge will send.

Three deviate, and each says so in the header: **`Label`** is a run of static
text (AccessKit's word; ARIA leaves it roleless), **`TextInput`** is ARIA's
`textbox` spelled after the component it comes from, and **`ScrollView`** is
AccessKit's, because ARIA has no word for "this region moves".

There is no role for anything this toolkit cannot build. A role no component ever
sets is a role nothing can test.

| | |
| --- | --- |
| Content | `Label`, `Heading`, `Paragraph`, `Image`, `Link`, `Figure` |
| Controls | `Button`, `Checkbox`, `Radio`, `RadioGroup`, `Switch`, `Slider`, `SpinButton`, `TextInput`, `ComboBox`, `ListBox`, `Option`, `ProgressBar` |
| Structure | `Group`, `Form`, `Toolbar`, `Separator`, `ScrollView`, `List`, `ListItem`, `Table`, `Row`, `Cell`, `ColumnHeader`, `Tree`, `TreeItem`, `TabList`, `Tab`, `TabPanel` |
| Overlays | `Menu`, `MenuBar`, `MenuItem`, `MenuItemCheckbox`, `MenuItemRadio`, `Dialog`, `AlertDialog`, `Tooltip` |
| Live | `Status`, `Alert` |

`Role::None` is the default and the right answer for most nodes: presentational,
collapsed away by the tree.

## State

```cpp
enum class Flag { Unset, False, True, Mixed };
constexpr Flag flag(bool);
```

**`Unset` is not `False`.** A checkbox that is not checked is announced as "not
checked"; a button, which has no checked state, is announced as a button. A
`bool` defaulting to false would give every button in the tree a state it does
not have. `Mixed` is the third value a tri-state checkbox has, and the reason
this is not `std::optional<bool>`.

The flags are `checked`, `expanded`, `selected`, `pressed`, `disabled`,
`readOnly`, `invalid`, `busy` and `required`, plus `sorted` for a column header —
`Sort::None` there means "sortable, not sorted by", which is what the permanent
arrow says visually.

## Value

```cpp
struct AccessibilityValue {
    bool present;
    double now, minimum, maximum;
    std::string_view text;   // the value in words
};
```

`text` is the whole point of this being more than a number. **A slider that
announces "70" is a slider nobody can use**; "70 percent" is one they can, and
only the caller knows which of the two it is — the number carries no unit and the
toolkit has no locale to invent one from. That is why `SliderOptions::valueText`
exists.

`minimum == maximum` says there is no range, which is the honest answer for a
text box: it has a value and no bounds. A number box sets all three, and
`textInput` reports the **clamped** value even while the text is not — typing
`500` into a box that stops at `60` shows `500` and returns `60`.

A password box reports no value at all. The bullets exist so the string is not on
offer, and a tree that carried it would hand back what the screen refuses to
show.

## Relations

Tags, never `NodeId`s: the tree is rebuilt every frame and only a tag survives
that.

`labelledBy`, `describedBy`, `controls`, `owns`, `activeDescendant` — and then
two more, pointed the other way:

```cpp
std::string_view labels;      // the control this node names
std::string_view describes;   // the control this node describes
```

**They exist because the end that knows is not the end that carries it.** A
caption is built before the input it names; a `field`'s error is built after it;
and a component never reaches into another component's node. So the node that
knows says which way round it goes, and the accessibility tree turns it over when
it is built. This is `<label for>` exactly — the attribute is on the label and the
browser resolves it onto the control.

`activeDescendant` is what makes an open `select` usable: focus stays on the box
— deliberately, so Tab cannot fall into the popup — and this is the only way to
say which row the arrow keys are on.

## Sets

```cpp
std::size_t positionInSet;   // one-based
std::size_t setSize;
```

For one reason, and it is a good one: **a virtualised list builds only the rows
on screen.** Without these, a reader walking fifty thousand commits is told "row
3 of 14" and then told it again for the rest of the list, because fourteen is all
that was ever in the tree. `virtualList` sets them from `count`.

## Hiding

`hidden` is `aria-hidden`: this node and everything under it is not in the tree.
Not the same as `Role::None`, which means "I am not a thing, but my children
might be". It is for content drawn twice on purpose — a marquee draws its text a
second time to hide the seam, and a reader given both would be read the same
sentence twice with nothing to say why.

## What is still missing

Named rather than discovered, which is this project's habit:

- **The tree and the bridge.** Stage 4 prunes this into one node per thing a user
  can perceive and diffs it against last frame; stage 5 pushes that through
  AccessKit. Neither exists. Until then this is a data model with tests, not
  something a screen reader can read.
- **Focus is not trapped in a modal.** `modal` has the right role and Tab still
  walks straight out of the back of it. Stage 7.
- **The colour picker's square has no keyboard**, in any form. A pointer is the
  only way to reach it. Stage 6 is the widget-by-widget audit that finds the rest
  of these.
- **`richEditor` does not report the marks under the caret** — bold, the heading
  level, which list a block is in. That is a property of a *position* rather than
  of a node, and it wants a text-range interface that comes after stage 4.
- **Nothing reads the system's reduced-motion or font-size settings.** Stages 9
  and 10.
