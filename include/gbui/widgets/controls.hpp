// The controls, as one include — **superseded**, and kept so nothing breaks.
//
// "Controls" was one group holding two different kinds of thing: a checkbox and
// a date picker are not peers, and putting them under one word made the sidebar
// and the designer's palette both harder to scan than the set actually is. The
// split is now `elements.hpp` — the primitives, the ones with an HTML
// counterpart — and `components.hpp` — the composed ones that have already made
// a design decision. Each of those headers argues its own half.
//
// This header includes both, so an existing `#include "gbui/widgets/controls.hpp"`
// still compiles and still brings in everything it used to. Prefer the one you
// mean in new code: it is shorter, and it says which half you are reaching for.
//
// It contributes no group of its own to the component metadata, which is why
// the split shows up in the documentation without any list being rewritten.
#pragma once

#include "gbui/widgets/components.hpp"
#include "gbui/widgets/elements.hpp"
