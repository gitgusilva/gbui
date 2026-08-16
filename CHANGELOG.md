# Changelog

Every released version, newest first, written from the history rather than from
memory. Dates are the day the version was tagged.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
the versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html),
with the caveat every 0.x project carries: **the API is not stable yet**, and a
minor version may change one. What is promised before 1.0 is that a change is
named here and the reason for it is in the commit.

## [Unreleased]

Work towards 0.3 lives on `main` and gets a number when it ships, not commit by
commit.

### Added

- **`splitPane`** — two panes and a divider the reader can drag, which is the
  shape every IDE-shaped application is built from.

  **The share is a percentage basis rather than a `grow` ratio**, and finding
  out why is the useful part: this layout engine computes its free space from
  the *hypothetical* sizes, which are already clamped to each item's minimum, so
  two panes with a 120-pixel floor take their 240 first and split only what is
  left — asking for a quarter of 600 got 208 instead of 148, and with large
  minimums the fraction stopped meaning anything. A basis of `p%` plus `shrink`
  is exact: the overflow the divider causes comes back off each pane in
  proportion to its basis, which lands the leading one at `p × (width −
  divider)`.

  The minimums are the layout's rather than the drag's, so they hold when the
  *window* shrinks under a split nobody touched. The divider is ARIA's window
  splitter — a `Separator` that takes the keyboard and carries a value — because
  a split only draggable with a pointer is a layout most people cannot change.
- **`treeView`** — the expandable hierarchy the component inventory calls the
  single biggest gap for a git client. Expansion, keyboard walking and
  virtualisation, which are each easy and never all three.

  **The data is a flat vector in pre-order with a depth on each row.** Flat is
  what makes virtualisation possible at all — a slice of a tree is only a slice
  if the tree is already a sequence — and it is what a caller usually has from a
  `git ls-tree` walk or a directory listing. Which rows are *visible* is worked
  out here in one pass with a watermark, so a caller that collapses a node
  passes exactly the same vector as before.

  Right opens a closed node and steps into an open one; Left closes an open one
  and steps out of a closed one. That pair is the whole of why a tree feels like
  a tree. The twisty opens without choosing and the row chooses, because "show
  me what is in here" and "I want this one" are two gestures.

  Each row reports its `level` and its position among its **siblings** — new
  `Accessibility::level`, ARIA's `aria-level` — since "item 2 of 5" in a
  hierarchy means whose five and "row 340 of 900" is the size of the repository
  rather than of the directory. Computed in two linear passes with a counter per
  depth, because the obvious version is quadratic on a directory with a thousand
  files in it, which is a directory people have.

  `VirtualListOptions::itemRole` is new with it: `Role::None` hands the slot's
  semantics to the row callback, so a tree's rows can be counted among their
  siblings rather than among the nine hundred the list holds.
- **`select` filters, which is the `combobox` the inventory called the gap that
  bites first** — a branch picker past about thirty branches is unusable without
  type-to-filter. An option rather than a component of its own, for the reason
  `textInput` absorbed two fields: everything that makes a select a select is
  unchanged by typing into it, and the two would be one control described twice.

  **`SelectResult` grew a `focus`**, and it is the caller's half of the deal: a
  filter box has to hold the keyboard to be typed into, so the control cannot
  keep it on the closed box — and a component here never moves focus behind the
  caller's back. Same contract `label` and `field` already have. Not wiring it
  leaves a filter that works only once clicked.

  The highlight stays an index into the caller's list rather than into the
  filtered view of it, which is the invariant this is easiest to get wrong. The
  match is a case-insensitive substring rather than a fuzzy score, because fuzzy
  matching reorders the list under the reader and matches things they cannot see
  the reason for. Escape clears the filter before it closes the list; Space
  types a space instead of committing, since a combobox that cannot have a space
  in its query cannot find `feat/nord tuning`; the arrows walk what is on screen
  rather than stepping into rows that are not.

  The filter box carries `controls` and `activeDescendant` because that is where
  the keyboard is; the match count is a `Status` live region; and each row
  reports its place in what is *shown*, since "3 of 40" in a list narrowed to
  four is three lies in five words. `MenuItemOptions` grew `positionInSet` and
  `setSize` to carry it.
- **`carousel`** — a strip of slides, one screenful at a time, with indicators,
  navigators, looping, a fractional `slidesPerPage` and autoplay. It moves by
  *slides* rather than by pages even when several are showing, which is the
  convention that keeps a four-across gallery usable: "next" is the thing after
  the one you are looking at.

  **An autoplaying carousel always draws a pause button, and there is no option
  to remove it.** WCAG's "pause, stop, hide" is a rule rather than a judgement,
  and an option to remove the button would be a switch labelled "make this
  inaccessible". Hovering the slides pauses it and so does the keyboard being
  inside them — but *not* reaching for a control, because the first attempt
  paused on focus anywhere in the carousel and pressing Play then left focus on
  Play and refused to move.

  Off-screen slides are `hidden` from the accessibility tree rather than left in
  it: eight slides all present at once turns a control into a list a reader has
  to find their way out of. The dots are a `TabList` with `activeDescendant`,
  one of the two patterns ARIA blesses for a carousel.
- **`gallery`** — one picture at a time out of a set, with arrows, a caption and
  a thumbnail strip that keeps the current one in view. Every picture has a
  name: its `alt`, its caption, or "Image 3 of 9", because an unnamed picture in
  a set of nine is "image, image, image".

  **Zoom, rotate, flip, download and fullscreen are absent**, and each for a
  reason written into the header rather than left to be discovered: the first
  two need a transform on a node that the painter has not got, download needs a
  native file dialog and nothing here touches the filesystem, and fullscreen is
  a second window. A rotate button that does not rotate is worse than no button.
- **`compare`** — two things in the same rectangle with a handle saying how much
  of each, which is the shape PrimeVue calls Compare and every before-and-after
  on the web is. Both sides are drawn at the full size of the box and one is
  revealed over the other, because a comparison laid out side by side is asking
  the reader to remember rather than to see.

  **The seam is a percentage, not a measured width**, and that is the whole
  design: a clip sized from last frame's geometry is a frame late and jumps on
  every resize, while a percentage resolves during layout and is right on the
  first frame. The content inside the clip is `100 / position` percent of it,
  which comes back out to the full width — a layout identity rather than an
  arithmetic one. The handle is placed by two flexible spacers for the same
  reason, and that also keeps it wholly inside the box at either end.

  It is a slider and genuinely one — value, range, arrow keys at 2%, Page at
  10%, Home and End — rather than PrimeVue's hidden range input beside a div.
  The value is announced as `"60% Retouched"`, because "60 percent" alone says
  neither how much of what nor revealing what, and both sides stay named in the
  tree whatever the handle is doing.
- **`toast`** — short-lived messages, stacked in a corner and gone on their own.
  The last of the three the component inventory called *blocking*, and the one
  it described as "a queue, a timer and a live region".

  The queue is `ToastState`, owned by the application. That matters more here
  than usual: toasts are raised from anywhere — a network reply, a file watcher,
  a shortcut three screens away — and a component that owned them would be a
  component with a global.

  **The id is the whole of the grouping.** Two entries with the same id are one
  toast with a count on it, and an empty id is derived from the kind, the title
  and the message — so a retry loop reports "still offline ×40" instead of forty
  copies of one sentence, which is the failure every application's first toast
  queue has. `group` is a second and different axis: it routes an entry to an
  *outlet*, so a dialog can report into itself while the application's messages
  go to the corner.

  **Placement is six corners, or anywhere at all.** `ToastPlacement::Anchored`
  puts the stack against a tagged node with the same engine a popover uses, and
  `bounds` says which rectangle the corners are measured from, so a stack can
  live inside a panel. Which way it *grows* is never a decision the caller
  makes. A bottom stack does not measure itself to find its own bottom either —
  the container is the whole column and `justify` puts the toasts at the end of
  it, which is right on the first frame where arithmetic on last frame's height
  is not.

  **The timer stops while it is being read**, which is Toastify's behaviour and
  also what WCAG's "enough time" rule asks for: the stack pauses while the
  pointer is over a toast or the keyboard is inside it. `duration = 0` never
  expires. Only what is on screen ages, so an entry waiting behind `maxVisible`
  has not started its clock. The progress bar is drawn only where there is a
  time to show, and dims while paused.

  Each toast is its own live region — `Status` for info and success, `Alert` for
  warning and error, because the next thing the reader was about to do will not
  work. The stack never takes the keyboard, and each × is named after its
  message, since four buttons called "Dismiss" are four buttons nobody can tell
  apart.
- **An accessibility layer: every control now says what it is.** Until this,
  a `Node` carried a style, a tag and a frame and nothing that said what it
  *was* — a screen reader was handed one blank rectangle where an application
  should have been. `gbui/a11y/role.hpp` and `accessibility.hpp` are stages 1 to
  3 of the plan (stage 4 is below): a `Role` and a name on every control, the
  state and value that
  go with it, and the relations that tie a caption to its field and an error to
  its input. Set with `ui.accessible({…})` beside the `tag` and the `focusable`
  that were already there; `ui.role(…)` and `ui.name(…)` are the shorthands.

  The names are ARIA's, which are also AccessKit's, so the platform bridge in
  stage 5 is a lookup table rather than a translation with opinions in it. Three
  deviate and each says why in the header. There is no role for anything this
  toolkit cannot build.

  **`Unset` is not `False`.** A checkbox that is not checked is announced as "not
  checked"; a button, which has no checked state, is announced as a button — so
  every state is a four-valued `Flag` and a state nobody set stays unsaid. The
  value carries `text` as well as a number, because a slider that announces "70"
  is a slider nobody can use and only the caller knows it means "70 percent".
  `positionInSet`/`setSize` exist for one reason: a virtualised list builds only
  the rows on screen, and without them a reader walking fifty thousand commits is
  told "row 3 of 14" for the rest of their life.

  **The record is not on the `Node`.** Most nodes have nothing to say, so it
  lives in a side table the arena owns and a node names by index — four bytes
  each, the full record only where there is one — exactly as vector art already
  does.

  Two relations point the *other* way, `labels` and `describes`, because the end
  that knows is not the end that carries it: a caption is built before the input
  it names and a `field`'s error after it, and no component reaches into
  another's node. `<label for>` is the same shape.

  New options where a component could not otherwise be named:
  `ButtonOptions::name` (for the icon-only button, the classic failure),
  `TextInputOptions::name`, `TextareaOptions::name`, `SelectOptions::name`,
  `SliderOptions::name` and `valueText`, `ProgressOptions::name`,
  `TableOptions::name`, `VirtualListOptions::name`, `ScrollOptions::name`,
  `MarqueeOptions::name`, `RichEditorOptions::name`, `ColorPickerOptions::name`,
  the chart options' `name`, `MenuItemOptions::role`, `PopoverOptions::role` and
  `name`, and `BoxOptions::role` and `name`.

- **`gbui_demo --a11y`, which audits the call sites.** The other half of
  `tests/accessibilityTest`: that one covers the *library*, and this walks the
  accessibility tree of every demo screen and every catalogue example and fails
  naming any control that has nothing to announce. It exists because the names
  the toolkit cannot invent — an icon-only button, a chart, a table — are the
  application's to supply, and nothing but a walk over the real screens can tell
  whether anyone did. `Host::accessibility()` exposes the tree for it, and CI
  runs it beside `--coverage`.

  **It found sixteen on its first run**, all in the six demo screens: five
  unnamed tables, three charts, six switches and a slider. All fixed — and one
  of them was a library gap rather than a call-site one: `ToggleOptions`,
  `CheckboxOptions` and `RadioOptions` had no way to be named when the words are
  drawn *beside* the control instead of by it, which is exactly how the SCADA
  screen lays its pump switches out. They take a `name` now, defaulting to the
  label.
- **The accessibility tree, and a diff of it.** Stage 4:
  `buildAccessibilityTree(arena, root, interaction)` reads the records above into
  one node per thing a reader can perceive, and `diffAccessibility` says what
  changed. Three jobs. It **prunes** — every box that exists for layout is
  collapsed away and its children re-parented, so a button wrapped in three
  containers is one node and not four. It **resolves** — `labels` and `describes`
  become the `labelledBy` and `describedBy` that belong on the control, a control
  with no name takes its caption's, and one with neither takes the text inside
  it, stopping at anything that is a node of its own so a table is not announced
  as every cell it holds. And it **diffs**, because pushing a whole tree at a
  screen reader sixty times a second is how an application becomes unusable
  *with* accessibility turned on.

  An `AccessibilityId` is a **hash of the tag** — the identity scheme focus, hit
  testing and the animation clock already run on, and the only kind that survives
  a tree being rebuilt. Untagged nodes derive one from their parent and their
  position. The consequence is the one worth having: an unchanged frame diffs to
  nothing, even though every node in the arena is new. Focus is reported
  separately, because it moves between two nodes that are otherwise identical.

  **The relations are resolved in two passes, and it has to be two.** A caption
  is built before the control it names, so a single pass writes the control's
  `labelledBy` and then reaches the control and overwrites it with the nothing
  the control knows.

  **This is a tested data model, not something a screen reader can read yet.**
  Stage 5 pushes it through AccessKit, and that is a decision before it is a task
  because it would be the library's second dependency. [The reference
  page](docs/reference/accessibility.md) lists the rest of what is missing —
  including that `modal` still does not trap focus and the colour picker's
  saturation square has no keyboard at all.

### Changed

- **The header house style is a gate, not a habit.**
  `tools/generate_meta.py` now fails — and CI with it — when a widget header
  opens with no sentence saying what it is, when that opening paragraph runs
  past 240 characters, or when an options member has no doc comment.

  It is in the generator rather than in a linter because the generator is
  already the thing that reads every header, and because the **output** depends
  on both: the first paragraph becomes the gallery card and a member's comment
  becomes its row in the properties table, so a header that skips either ships a
  blank in the documentation. A check that lives beside the harvest cannot drift
  from it.

  The length rule catches one specific failure the roadmap named: a header whose
  first line is a design argument ships that argument as its summary. A blank
  `//` line ends the paragraph, which is all a long preamble has to do.

  **The member rule has an exemption, and it is the interesting half.** The
  words this library uses with one meaning everywhere — `width`, `gap`,
  `disabled`, `padding`, `grow`, and any compound ending in a dimension such as
  `cellPadding` — need no comment, because documenting `float width` in forty
  structs is forty copies to keep in step and is exactly the restating-the-code
  comment the style says to delete. They are listed in `SHARED_NAMES` with the
  reason. Everything else is specific to its component and has to say what it
  is; 35 members did not, and now do. Three of those were a parser artefact
  worth fixing anyway — `columnLabels`, `falling` and the candlestick's
  `categoryAxis` were sharing a comment with the member above them, so the
  generated table had them blank.
- **Accessibility is now a rule, not a milestone.** Rule 7 in `CONTRIBUTING`:
  every component that is added or changed carries working accessibility in the
  same commit, with a case in `tests/accessibilityTest.cpp`. The last case in
  that file is the gate — it walks a form and fails on any Tab stop with no role
  or nothing to announce — and it found three the first time it ran.
- **One `textInput` with an `InputType`, where there were `textField` and
  `numberField`.** The HTML shape: `Text`, `Password` and `Number` are one box
  that draws its content differently and refuses different things, and the two
  files that said so separately drifted exactly as two copies do — `numberField`
  held a `double` and no text, so the control in the set that most wants typing
  was the one that could not be typed into. It cannot be called `input`: every
  call site here names its `Interaction` parameter that, and a local hides a
  namespace-scope function of the same name completely.

  **Number entry is rewritten, not renamed.** The state is a `TextEditState`
  like every other input's, and while the box has the keyboard the text is the
  source of truth — `-`, `1.` and empty are all states a number is typed
  through, and nothing rewrites the text underneath the caret. `result.value` is
  clamped even when the text is not (typing `500` into a box that stops at `60`
  shows `500` and returns `60`), `result.hasValue` is how an empty box says it
  has no value, and blur normalises the text from the clamped value. Anything
  that could still become a number is accepted; anything else is refused.

  Two behaviours went with the rewrite, both on purpose: **Home and End move the
  caret** rather than jumping to the bounds, and `+`/`-` type rather than step —
  a box that takes typing cannot have those keys mean something else. Up, Down,
  the wheel and the step buttons still step. Two bugs went with it too: the
  stacked spin box drew `ChevronDown` for *both* arrows, and a number box sized
  to its digits changed width as they were typed, walking the steppers out from
  under the pointer clicking them — a number now takes 120 px unless told
  otherwise. `invalid` is new on all three types, and is the control's half of
  the state `field` owns the message for.

  **This removes public API**: `textField`, `TextFieldOptions`,
  `TextFieldResult`, `numberField`, `NumberFieldOptions` and `NumberFieldResult`
  are gone, and `StepperPlacement` moved to `gbui/widgets/textInput.hpp`. A
  compiler finds every call site; `{.password = true}` becomes
  `{.type = InputType::Password}`.
- **Every `begin*` container is named after what it makes.** `beginPanel` is
  `panel`, `beginBox` is `box`, and so on through `toolbar`, `listRow`,
  `popover`, `modal` and `modalActions`; `beginScroll` is `scrollArea`, because
  `scroll` is what half the call sites already call their `ScrollState`. On the
  builder, `ui.begin` is `ui.scope`, `ui.beginRow`/`beginColumn` are `ui.row`
  and `ui.column`, and `ui.beginIds` is `ui.ids`. A container and a leaf now
  share one naming rule and differ only in what they return — `Ui::Scope`
  against `NodeId` — which is the distinction that was worth spelling, and
  `begin` was never it. **This renames public API**; the mapping above is the
  whole of it, and a compiler finds every call site.
- Documentation: **every component has its own page**, listed in the sidebar and
  found by search, generated from the same metadata the gallery read. Components
  declared in one header share one page — `colorField` with `colorPicker`,
  `scrollArea` with `scrollbar`, `text` with `strong` and `emphasis` — because
  the file they are in is the toolkit's own statement that they are one idea.
  `/components` is the contents page for them.

### Changed

- **The component set is grouped by four questions instead of one word.**
  "Controls" held a checkbox and a date picker as if they were peers. The groups
  are now **Elements** (22 — leaves with a counterpart in HTML, deciding nothing
  beyond the theme), **Containers** (16 — anything whose job is the content
  inside it), **Overlays** (7 — anything that leaves the flow and floats),
  **Components** (8 — the composed editors) and **Charts** (9). The questions are
  asked in that order and the order *is* the taxonomy: it is why `panel` and
  `toolbar` are containers rather than the composed things they plainly are, and
  why `select` is an element even though its list is a popover — the question is
  asked of the control, not of its menu. `elements.hpp` is the new umbrella;
  `controls.hpp` still compiles and now includes the two that replaced it, so no
  existing include breaks. **`components.hpp` no longer pulls in `text`,
  `button`, `icon`, `image` or `spacing`** — include `elements.hpp` for those.
- **`switchToggle` is `toggle`**, and `SwitchOptions` is `ToggleOptions`, in
  `gbui/widgets/toggle.hpp`. The component is a switch and the documentation
  still calls it one; `switch` is a C++ keyword and cannot be a function name,
  which is the whole of the reason and is now written at the top of the header
  so nobody rediscovers it. Fluent and Carbon landed on the same word for the
  same reason. **This renames public API**; a compiler finds every call site.
- Documentation: **the three pickers share one page.** A date, a time and a
  date-and-time are the same control with different amounts of it, and a reader
  who lands on one wants the other two under it — the reasoning that already put
  `colorField` beside `colorPicker`, except these could not share a header
  without breaking "one component, one header". A page may now gather several
  headers, and it lists all of them rather than the first.

### Added

- **`textarea`**, the multi-line plain text box — the gap between `textInput`
  and `richEditor`, and a wide one: a commit message, a description, a note.
  Return takes a newline and Ctrl+Return (Cmd+Return on macOS) submits, which is
  what every composer does. `rows` is a floor and `maxRows` a ceiling, so a box
  can start small and open up as the writing goes on before it scrolls; the view
  follows the caret and nothing else moves it. Lines are the ones a `\n` makes —
  Up, Down, Home and End move by hard line, not by the line a box happens to
  wrap to, the same limit `richEditor` has and for the same reason. The editing
  model gained a `multiline` mode rather than a second copy of itself, and a
  pasted paragraph now keeps its line breaks instead of arriving as one line.
- **`field`**, the wrapper every form writes by hand: a caption, the control,
  and a line underneath that is either guidance or a complaint — never both, as
  advice and a complaint in the same place is two things asking to be read
  first. It is also where the accessibility relations will attach, since a label
  is only a label because it is *for* something and something has to know both
  ends.
- **A tag now publishes a release.**

### Fixed

- **A modal did not trap focus.** Tab walked straight out of the back of the
  dialog and into the page the backdrop says cannot be used, with nothing on
  screen saying where the keyboard had gone. `Node::trapsFocus` and
  `Ui::trapsFocus()` say a subtree confines Tab; `Interaction` resolves it,
  because Tab is resolved there and nowhere else and because a component cannot
  see the tree it is in. Focus moves inside on the frame the dialog appears and
  **returns to whatever opened it** on the frame it stops being built. Nested
  dialogs work: the innermost tagged trap wins.
- **The colour picker could only be used with a pointer.** The square had no
  Tab stop and no keys at all, which is a worse gap than a missing role — a
  role at least says the control is there. The square and both rails are Tab
  stops now: Left and Right move along, Up and Down are the square's second
  axis, Home and End go to the ends of the axis the key belongs to, and Shift
  is ten times the step. Each draws a focus ring, and the square's description
  says so, because nothing about a `Group` implies it answers the arrows.
- **A button was never a Tab stop.** It had not been since focus was built:
  `activated` gives every control Space and Return once it has the keyboard, and
  nothing could ever give the keyboard to a button — so the most ordinary control
  in the set was reachable by pointer alone. A tagged, enabled button is now
  focusable. An untagged one still is not, which is the contract
  `Node::focusable` states and not a second gap.
- **A press focused whatever node it landed on, rather than the control.** A
  control is rarely one node — a textarea is a box around a scroll view around a
  column of runs — and a click on the text resolved to a tag the scroll view
  invented. The keyboard went there, no key handler was listening, and typing
  did nothing. The press now walks up to the nearest focusable ancestor, which
  is what the browser does when you click the text inside a `<textarea>`.
  Clicking nothing focusable still clears focus. Until now a tag was the whole of a release:
  CI built it and nothing else happened, so every `releases/tag/v…` link in this
  file and the documentation's "Release notes" entry led to an empty page.
  `release.yml` publishes the GitHub release, with that version's section of
  this changelog as its body — read, not rewritten, because notes typed a second
  time start drifting the day they are made. `tools/release_notes.sh` prints the
  same text locally. It also checks the tag against the `VERSION` in
  `CMakeLists.txt` at that tag, which is what `release: 0.3` — tagged, then
  reverted — had no way of failing on. Releases for 0.2 and 0.2.1 have been
  published from the sections already written here.

### Fixed

- **A release cancelled its own CI run.** The concurrency group was the commit,
  and cutting a release pushes one commit twice — once as a tag, once as a
  branch update. The second push cancelled the first while it was still queued,
  and the run it took with it was the tag's: the one that says the released
  artefact builds. The group is the ref and the commit now.
- **A version could be advertised before its tag existed.** `build_docs.sh`
  prints "no tag for 0.2, skipping" and carries on, so a version listed in
  `archived` too early became a live dropdown entry leading nowhere and the
  deploy still went green. CI checks the list against the tags, on the pull
  request rather than after publication.
- An archived version is built from the **newest tag in its line**, so `/v0.2/`
  says what 0.2.1 says rather than what 0.2 said — 0.2.1 is what a reader of
  `/v0.2/` would install. The documentation job also runs the node that can read
  `versions.ts`; on the older one the archived list came back empty instead of
  failing.

## [0.2.1] — 2026-08-13

A packaging fix, released from `0.2.x` rather than from `main`: the branch is
0.2 and this, and nothing else.

### Fixed

- **A shared build could not run its own executables on Windows.** Windows has
  no rpath — a program finds a DLL beside itself or on `PATH` — while CMake's
  multi-config generator puts the library in `build/Release/` and every
  executable in `build/<dir>/Release/`. The build and the link both succeed and
  the *loader* fails at startup, which on a machine with no desktop is a dialog
  nobody can dismiss: the process never returns and never says why. The CI job
  that builds shared and runs the tests sat for six hours before it was
  cancelled. The DLLs are copied beside each executable now, on Windows and only
  when the build is shared.
- CI grew the ceilings whose absence turned that bug into six hours:
  `ctest --timeout 180`, so a hung test fails with its own name, and
  `timeout-minutes` on the job that found it.

## [0.2] — 2026-08-12

The first version with a published site, a CI pipeline, an installable library
and a set of application screens to look at. It is the version the documentation
described from the start; it had never been tagged, which is what this tag fixes
— an archived version with no tag is a dropdown entry that 404s.

### The toolkit

- **Build → layout → paint**, three stages in one direction: layout is
  arithmetic that can be asserted without a window, and painting is a display
  list that can be inspected without a GPU.
- **CSS flexbox** with wrapping, percentages, min-content sizing and out-of-flow
  positioning, in logical pixels — a 200% display is a property of the output
  rather than something a component knows about.
- **Theming as data**: 24 semantic tokens read from the `gitbox-themes`
  registry's JSON, with Material 3, Cupertino and Fluent palettes built in, and
  a `Design` beside them for shape, sizing and motion.
- **A software rasteriser** with antialiasing, gradients and clipping, and an
  **SVG writer** for review and golden images.
- **The interaction layer** in full: hover, press, click, focus,
  `:focus-visible`, focus-within, Tab traversal, wheel routing and per-node
  cursors.
- **An animation clock** on CSS's `transition` model — a component says where a
  value should be, not how to get there.
- **Around fifty components**, from a button to a table, a rich-text editor and
  eight kinds of chart, all stateless functions themed by token.
- **One allocation per frame**: nodes live in an arena addressed by index, so
  building a tree is a `push_back` and releasing one is a reset.

### The component set as data

- `gbui::meta` describes every component — its group, its documentation, its
  signature, and each option with type, default and doc — **generated from the
  headers** by `tools/generate_meta.py`, so a table nobody maintains cannot fall
  behind the code it describes. CI regenerates it and fails on a difference.

### Demos and documentation

- **Six application screens** built from the public headers alone: a revenue
  dashboard, a weather desk, a plant supervisory HMI, a production line monitor,
  a grid control desk and a logistics control tower. They link `gbui::gbui` and
  nothing else, so a change that makes them awkward has made the library
  awkward.
- The demos **run in the browser** through WebAssembly, rasterised on the CPU
  into a `<canvas>` — the same source, with no DOM inside the rectangle.
- A documentation site on VitePress, **published per version**: the current
  release at the root and every archived one at its own address, built from its
  own tag.
- The source is shown first and the screen runs when the reader asks, so nothing
  downloads until they press Run.

### Build and packaging

- Installable with CMake, as a **static or shared library**, and consumed from
  outside the tree in CI to prove the install actually works.
- SDL2 is optional: without it everything still builds and every test still
  passes, and only `Window::create` changes.
- CI builds, tests, sanitises and lints on Linux, macOS and Windows, with
  warnings as errors.

### Fixed

- Every `std::optional` in an options struct has a default member initialiser,
  so a designated-initialiser call site cannot leave one indeterminate.
- Four things that only writing six screens against the library could find — see
  `acf2897`.
- The runner works on a fresh machine and on MSVC; the gallery example the
  gitignore was hiding is committed.

### Known at the time

- No accessibility tree, and no bridge for a screen reader.
- No text shaping, so Arabic, Devanagari and emoji are wrong.
- No GPU painter — the `Painter` interface is six methods precisely so one can
  be written.
- No image decoding.

[Unreleased]: https://github.com/gitgusilva/gbui/compare/v0.2.1...HEAD
[0.2.1]: https://github.com/gitgusilva/gbui/releases/tag/v0.2.1
[0.2]: https://github.com/gitgusilva/gbui/releases/tag/v0.2
