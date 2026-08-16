// A control with its label, its help and its error, laid out as one thing.
//
// The part of a form that every application writes and rewrites: a caption
// above, the control, and a line underneath that is either guidance or a
// complaint. Written by hand it comes out slightly differently on every screen
// — a different gap, a different colour for the error, the asterisk on the
// wrong side — and none of those differences mean anything.
//
// It is also where the accessibility relations will attach. A label is only a
// label because it is *for* a control, and an error message is only an error
// message because it *describes* one; both are relations between two nodes and
// something has to know about both ends. This is the something.
//
// The control is a callback rather than the contents of a scope, because a
// field has parts on both sides of it. A scope can open before its children and
// close after them, but it cannot write a line of text underneath — so the
// shape that works is the one `marquee` already uses.
//
//     field(ui, input, "f.name", {.label = "Repository", .required = true,
//                                 .help = "Lowercase, no spaces.", .forId = "name"},
//           [&](Ui& ui) { textField(ui, input, "name", state); });
#pragma once

#include <functional>
#include <optional>
#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"

namespace gbui {

struct FieldOptions {
    /** The caption above the control. Empty draws none, which is what a field
     *  inside a table cell or beside its own heading wants. */
    std::string_view label{};
    /**
     * The control this labels, so clicking the caption focuses it.
     *
     * The id of the control built inside the callback — this cannot be worked
     * out from the tree, because the field is built before the control is and
     * a component never reaches into what a caller wrote.
     */
    std::string_view forId{};
    /** Guidance under the control. Replaced by `error` while there is one:
     *  advice and a complaint in the same place, at the same time, is two
     *  things asking to be read first. */
    std::string_view help{};
    /**
     * What is wrong. Non-empty puts the field in its invalid state — the
     * message is drawn in the error colour and the caption goes with it.
     *
     * It does **not** restyle the control. A component does not reach into
     * another component's options, and a border that changed colour by remote
     * control would be exactly that; pass `invalid` to the control as well
     * where it has one. What this owns is the message and the caption.
     */
    std::string_view error{};
    /** Marks the caption with the usual asterisk. */
    bool required = false;
    bool disabled = false;
    /** Between the caption, the control and the message. */
    float gap = 5.0f;
    float width = kAuto;
    float grow = 0.0f;
};

struct FieldResult {
    /**
     * The caption was clicked: focus this.
     *
     * Handed back rather than acted on, the same contract `label` has — the
     * toolkit does not mutate interaction state behind a component's back.
     *
     *     if (const auto target = field(...).focus) interaction.focus(*target);
     */
    std::optional<std::string_view> focus{};
};

/** Draws the caption, then `control`, then the help or the error. */
FieldResult field(Ui& ui, const Interaction& input, std::string_view id,
                  const FieldOptions& options, const std::function<void(Ui&)>& control);

}  // namespace gbui
