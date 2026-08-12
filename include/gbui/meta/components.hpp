// What the component set *is*, as data.
//
// Five separate things need to know that `button` takes a `ButtonOptions` whose
// `variant` is one of four values: the designer's properties panel, the code
// generator that writes C++ out of a scene, the importer that maps a
// `QPushButton` onto ours, the documentation's component gallery, and a
// conformance test that every option is actually reachable. Written by hand
// that table rots on the first new option; generated from the headers it
// cannot, which is why `src/meta/components.cpp` is produced by
// `tools/generate_meta.py` and never edited.
//
// **This module links nothing.** It is `std::string_view` and `std::vector`
// over static storage, in a target of its own — a tool that wants to know the
// shape of the component set should not have to link a rasteriser to find out.
//
//     for (const meta::ComponentInfo& c : meta::components()) {
//         if (c.group == "Controls") …
//     }
#pragma once

#include <string_view>
#include <vector>

namespace gbui::meta {

/**
 * What kind of value a property holds, coarse enough to build an editor from.
 *
 * Coarser than the C++ type on purpose: a properties panel needs to know
 * whether to draw a checkbox, a number field or a menu, and `float` and
 * `double` are the same question. `Enum` carries its choices; everything else
 * is described well enough by `PropertyInfo::type`.
 */
enum class PropertyKind {
    /** `bool`. */
    Bool,
    /** `float`, `double`, `int`, `std::size_t`. */
    Number,
    /** `Length` — a number that may also be a percentage. */
    Length,
    /** `std::string_view`, `std::string`. */
    Text,
    /** `Token` — a colour from the theme, never a literal. */
    Token,
    /** A scoped enumeration; see `PropertyInfo::choices`. */
    Enum,
    /** `Icon`. */
    Icon,
    /** `Edges` — four numbers in the CSS order. */
    Edges,
    /** Anything the generator recognised but has no editor for: a callback, a
     *  nested options struct, a container. Listed so the table stays complete
     *  and an editor can grey it out rather than pretend it is absent. */
    Opaque,
};

/** One member of a component's options struct. */
struct PropertyInfo {
    /** As written: `variant`, `leading`, `fitSample`. */
    std::string_view name;
    PropertyKind kind = PropertyKind::Opaque;
    /** The C++ type, verbatim — `ButtonVariant`, `std::optional<Icon>`. */
    std::string_view type;
    /** The default member initialiser, verbatim, or empty when there is none.
     *  Verbatim rather than parsed: an editor shows it, a generator emits it,
     *  and neither needs it evaluated. */
    std::string_view defaultText;
    /** The values of a scoped enumeration, in declaration order. Empty unless
     *  `kind` is `Enum`. */
    std::vector<std::string_view> choices;
    /** The documentation comment above the member, joined into one line and
     *  stripped of its markers. Empty when it had none. */
    std::string_view doc;
    /** True when the member is `std::optional<…>`, which an editor draws as a
     *  value plus a "set" toggle rather than as a value. */
    bool optional = false;
};

/** One component: a function that writes nodes through a `Ui`. */
struct ComponentInfo {
    /** The function's name: `button`, `beginPanel`, `lineChart`. */
    std::string_view name;
    /** Which umbrella header gathers it — Components, Controls, Containers,
     *  Overlays, Charts — or "Other" for one that no umbrella includes. */
    std::string_view group;
    /** The header that declares it, as it is included. */
    std::string_view header;
    /** The doc comment above the declaration, joined into one line. */
    std::string_view summary;
    /** The options struct's name, or empty for a component that takes none. */
    std::string_view optionsType;
    std::vector<PropertyInfo> properties;
    /**
     * True when it opens a container — it returns a `Ui::Scope` and the caller
     * fills it — rather than building a leaf and returning.
     *
     * The distinction a designer needs before anything else: a container can be
     * dropped *into*, and a leaf cannot.
     */
    bool container = false;
    /** True when it takes an `Interaction`, which is what "this one reacts to
     *  the pointer and the keyboard" means here. */
    bool interactive = false;
    /** The declaration, verbatim and on one line — the signature a reader
     *  wants next to the description. */
    std::string_view signature;
};

/**
 * Every component the library declares, in header order within each group.
 *
 * Built once on first use. The storage is static and the views point into it,
 * so an entry outlives any caller.
 */
const std::vector<ComponentInfo>& components();

/** One by name, or nullptr. */
const ComponentInfo* find(std::string_view name);

/** The groups present, in the order a gallery should show them. */
const std::vector<std::string_view>& groups();

}  // namespace gbui::meta
