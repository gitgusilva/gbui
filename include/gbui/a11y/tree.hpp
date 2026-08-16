// The accessibility tree: one node per thing a reader can perceive.
//
// Stage 4 of the plan, and the piece that turns the records on the arena into
// something a platform can be handed. Three jobs, and each is the reason the
// stage exists:
//
//   1. **Prune.** A frame is hundreds of nodes and a dozen things. Every box
//      that exists for layout is collapsed away, and what is left is the list a
//      reader would recognise.
//   2. **Resolve.** `labels` and `describes` are stated by the caption and the
//      error message, because those are the ends that know; here they are turned
//      into the `labelledBy` and `describedBy` that belong on the control. A
//      control with no name of its own takes the one that names it.
//   3. **Diff.** The tree is rebuilt every frame and almost none of it changes.
//      Pushing the whole thing at a screen reader sixty times a second is how an
//      application becomes unusable *with* accessibility turned on, so only what
//      changed is reported.
//
// This is the same discipline as the display list, for the same reason: build a
// flat, comparable description once, and let whatever consumes it do so without
// walking the scene.
//
// ---- identity, which is the part that could have gone wrong ---------------
//
// A screen reader holds on to a node between frames, and the arena does not: a
// `NodeId` names a different node next frame, or none. So an id here is a
// **hash of the tag**, which is the identity scheme the whole toolkit already
// runs on — focus, hit testing and the animation clock are all keyed by it.
//
// Untagged nodes still get one, from their parent's id and their position among
// its accessible children. That is stable while the shape of the tree is, which
// is the same guarantee the tag scheme gives and no weaker: a component that
// reorders its children reorders them for the pointer too.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "gbui/a11y/accessibility.hpp"
#include "gbui/core/geometry.hpp"
#include "gbui/scene/tree.hpp"

namespace gbui {

class Interaction;

/** An id that survives the frame. Zero is "no node", never a real one. */
using AccessibilityId = std::uint32_t;

/** One thing a reader can perceive. */
struct AccessibilityNode {
    AccessibilityId id = 0;
    /** The tag it came from, empty when the node had none. Handy in a test and
     *  in a bridge that wants to talk back. */
    std::string_view tag{};
    Role role = Role::None;
    /**
     * What it is called, after the name computation.
     *
     * The component's own `name` when it set one; otherwise the name of
     * whatever `labelledBy` points at; otherwise the text inside it, which is
     * what makes a `listRow` announce its contents without every call site
     * repeating them.
     */
    std::string name{};
    std::string description{};
    AccessibilityState state{};
    AccessibilityValue value{};
    std::size_t positionInSet = 0;
    std::size_t setSize = 0;
    /** Where it is on screen, in window coordinates. A screen reader draws a
     *  highlight here and a magnifier scrolls to it. */
    Rect bounds{};
    /** This is where the keyboard is. Exactly one node in a tree has it, or
     *  none. */
    bool focused = false;

    AccessibilityId parent = 0;
    std::vector<AccessibilityId> children{};

    // Relations, resolved to ids. Zero where there is none.
    AccessibilityId labelledBy = 0;
    AccessibilityId describedBy = 0;
    AccessibilityId controls = 0;
    AccessibilityId activeDescendant = 0;
};

/** A whole frame's worth, parents before children. */
struct AccessibilityTree {
    std::vector<AccessibilityNode> nodes;
    AccessibilityId root = 0;

    const AccessibilityNode* find(AccessibilityId id) const;
    /** By the tag the node was built with — what a test asks. */
    const AccessibilityNode* find(std::string_view tag) const;
    bool empty() const { return nodes.empty(); }
};

/**
 * Builds the tree from a **laid-out** arena.
 *
 * After layout, because a node's bounds are part of what it says and layout is
 * what writes them. The `Interaction` supplies one thing: which tag has the
 * keyboard.
 */
AccessibilityTree buildAccessibilityTree(const Arena& arena, NodeId root,
                                         const Interaction& input);

/**
 * What changed since the last frame.
 *
 * `changed` holds every node that is new or different, whole — a screen reader
 * takes a node at a time, and half of one is not a thing that can be sent.
 * `removed` is the ids that were there and are not.
 */
struct AccessibilityUpdate {
    std::vector<AccessibilityNode> changed;
    std::vector<AccessibilityId> removed;
    /** Where the keyboard is now, or zero. Reported separately because it moves
     *  without anything about either node changing. */
    AccessibilityId focus = 0;
    bool focusMoved = false;

    bool empty() const { return changed.empty() && removed.empty() && !focusMoved; }
};

AccessibilityUpdate diffAccessibility(const AccessibilityTree& previous,
                                      const AccessibilityTree& next);

}  // namespace gbui
