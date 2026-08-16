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
  beyond the theme), **Containers** (11 — anything whose job is the content
  inside it), **Overlays** (6 — anything that leaves the flow and floats),
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
