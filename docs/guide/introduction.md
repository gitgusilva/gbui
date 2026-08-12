# Introduction

gbui is a UI toolkit for C++20. It draws application interfaces — the kind with
lists, panels, toolbars, dialogs and tables — without a web engine and without a
dependency beyond the standard library.

It was written for [GitBox](https://github.com/gitgusilva/gitbox), a desktop Git
client built on Electron, as a way off that runtime that keeps the design system
already in place. That origin explains two decisions you will meet early:

- **The layout model is CSS flexbox**, because that is what the interface being
  replaced was written in, and because it is the model most people already have
  in their heads.
- **Themes are the same files** the GitBox theme registry publishes, so a theme
  written for the web application themes the native one unchanged.

## What it is

A **retained-mode** toolkit. You build a tree of nodes, the library measures and
positions them, and a painter draws the result. The tree is cheap enough to
rebuild whenever your state changes, which is usually simpler than mutating one
in place.

![The pipeline: scene builds a tree, layout writes frames, paint records a display list, and a backend draws it.](/pipeline-light.svg){.light-only}
![The pipeline: scene builds a tree, layout writes frames, paint records a display list, and a backend draws it.](/pipeline-dark.svg){.dark-only}

Each stage reads only the one before it. Layout knows nothing about painting,
painting knows nothing about nodes, and a backend knows nothing about themes.
That separation is what makes the interesting half of the library testable
without a screen.

Components are **functions**, not objects: each takes a `Ui&` and an options
struct, draws itself, and reports what the user did. Nothing is inherited,
nothing is registered, and no component owns a piece of your model.

## What it is not

- **Not a browser.** There is no HTML, no DOM and no CSS engine. The flexbox
  vocabulary is borrowed; the machinery is not.
- **Not an application framework.** No networking, no threading model, no file
  dialogs. It draws, and it reports input.
- **Not a game UI.** Retained mode, laid out for text-heavy interfaces, not for
  immediate-mode overlays at 240 Hz.
- **Not a pixel-identical clone of a web application.** Two different layout and
  text stacks cannot agree to the pixel. The target is a UI that is
  unmistakably the same product, in the same design system, at every size.

## Who it is for

People writing desktop applications in C++ who want a UI that is themeable and
testable, and who are willing to trade a mature widget catalogue for a codebase
they can read. The library is about 18 000 lines of headers and sources, and
every one of them is in this repository — there is no vendored toolkit
underneath.

If you need a complete widget set today, use Qt. If you need a declarative
language and tooling, use Slint. gbui is worth your time when you want to own
the whole rendering path and keep it comprehensible.

## Next

- [Installation](/guide/installation) — build it and link against it.
- [Your first window](/guide/first-window) — sixty lines to something on screen.
- [Architecture](/guide/architecture) — why the three stages are separate.
