---
title: Components
description: Every component the library declares, with a live example and its options.
pageClass: demos-page
aside: false
---

# Components

Fifty of them, and everything on this page is read out of the headers rather
than written beside them: the summaries, the signatures and the options tables
come from `gbui::meta`, which `tools/generate_meta.py` produces from
`include/gbui/widgets/*.hpp`. A component that gained an option this morning has
it here this afternoon, and one that never existed cannot appear at all.

Press **Run** on any of them to see it working — the same WebAssembly module
the [demos](/demos) use, downloaded once for the page and not before you ask.

The examples themselves are the one part written by hand, in
[`demos/src/catalog/`](https://github.com/gitgusilva/gbui/tree/main/demos/src/catalog),
because C++ has no reflection and a table of options cannot be turned back into
a call. `gbui_demo --coverage` fails the build when a component in the metadata
has no example, which is what keeps the two halves honest.

<GbuiComponents />

## What a component is

A function that takes a `Ui&` and an options struct. There is no base class,
nothing to inherit and nothing to register:

```cpp
NodeId    thing(Ui&, args…);        // a leaf: builds and returns
Ui::Scope beginThing(Ui&, args…);   // a container: the caller fills it
```

The interactive ones take an `Interaction` as well, and all of them share one
shape — the component draws the value it is handed and reports what the user
did with it:

```cpp
if (checkbox(ui, input, "settings.tags", value, {.label = "Show tags"}))
    value = !value;
```

It never writes to your model. That is what makes undo, validation and "are you
sure?" possible without the toolkit knowing about any of them, and it is why
every one of these examples has a variable behind it rather than a component
that remembers.

[Writing a component](/guide/writing-a-component) walks through building one of
your own; it is the same exercise as the fifty above.
