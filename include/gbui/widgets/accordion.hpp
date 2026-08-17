// Sections that open one at a time, or several.
//
// A stack of headers, each with a body that is drawn only while it is open. The
// shape a settings page, a sidebar of collapsible groups and a list of long
// answers all have, and the reason it is a component rather than a `panel` with
// a `bool` is the keyboard: the arrows have to move between headers, Home and
// End have to reach the ends, and each header has to say whether it is open
// before a reader presses it.
//
// ---- what it is not ---------------------------------------------------------
//
// Not `tabs`, though both hide all but one thing. Tabs are *one* of a set and the
// set is always visible; an accordion can have **none** open, or all of them, and
// the sections it is not showing take no room. Use tabs when the reader is
// switching between views of the same size, and this when they are opening a
// long thing to read it.
//
// Not `treeView` either: a tree's rows are the same *kind* of thing at different
// depths, and these are a fixed handful of sections that happen to fold.
#pragma once

#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/icons.hpp"

namespace gbui {

struct AccordionSection {
    /** Identifies the section in `AccordionState`, so a reordered list does not
     *  change which one is open. Indices would. */
    std::string_view id{};
    std::string_view title{};
    /** A second line under the title, for what the title had no room for. */
    std::string_view detail{};
    std::optional<Icon> icon{};
    /** Drawn at the trailing edge before the chevron — a count, a status. */
    std::string_view badge{};
    bool disabled = false;
    /**
     * What is inside it, called only while the section is open.
     *
     * A callback for the reason `splitPane` takes them: the content belongs
     * *inside* something this component builds, and a closed section should
     * cost nothing rather than be built and hidden.
     */
    std::function<void(Ui&)> body{};
};

/** Which sections are open, and where the keyboard is. The application's, like
 *  every other piece of state the toolkit reads. */
struct AccordionState {
    /** Open sections, by `AccordionSection::id`. `std::less<>` so a lookup can
     *  be done with a `string_view` without building a `string` for it. */
    std::set<std::string, std::less<>> open{};
    /** Which header the arrows are on. Empty until the reader arrives. */
    std::string focused{};

    bool isOpen(std::string_view id) const { return open.find(id) != open.end(); }
    void toggle(std::string_view id) {
        if (const auto at = open.find(id); at != open.end())
            open.erase(at);
        else
            open.emplace(id);
    }
};

struct AccordionOptions {
    /**
     * Opening one closes the others.
     *
     * Off by default, and that is the less obvious choice: an accordion whose
     * sections close each other cannot be used to *compare* two of them, and a
     * reader who opens the second and loses the first will open both again one
     * at a time. Turn it on when the sections are long enough that two of them
     * on screen would be worse.
     */
    bool exclusive = false;
    /** What the whole stack is, for a reader arriving at it. */
    std::string_view name{};
    float gap = 6.0f;
};

struct AccordionResult {
    /** The section whose header was pressed this frame, if any. Already applied
     *  to `state`; handed back for a caller that wants to fetch on open. */
    std::optional<std::string_view> toggled{};
    /**
     * Focus this, if anything — the header the arrows just moved to.
     *
     * Handed back rather than acted on, the same contract `select`, `label` and
     * `field` have: a component here never moves the keyboard behind the
     * application's back.
     *
     *     if (const auto target = result.focus) interaction.focus(*target);
     */
    std::optional<std::string_view> focus{};
};

/**
 * A stack of collapsible sections.
 *
 * Enter or Space on a header opens it, the arrows move between headers, and Home
 * and End reach the ends. Every header carries `expanded`, so a reader is told
 * what a press will do before they make it.
 */
AccordionResult accordion(Ui& ui, const Interaction& input, std::string_view id,
                          const std::vector<AccordionSection>& sections, AccordionState& state,
                          const AccordionOptions& options = {});

}  // namespace gbui
