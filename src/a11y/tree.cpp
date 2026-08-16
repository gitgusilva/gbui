#include "gbui/a11y/tree.hpp"

#include <algorithm>
#include <map>

#include "gbui/input/interaction.hpp"

namespace gbui {
namespace {

constexpr AccessibilityId kFnvOffset = 2166136261u;

/** FNV-1a. An id has to be a pure function of the identity it comes from, so
 *  the same control is the same node next frame — a counter would renumber the
 *  whole tree the moment a row was inserted. */
AccessibilityId hashOf(std::string_view text, AccessibilityId seed) {
    AccessibilityId hash = seed;
    for (const char c : text) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 16777619u;
    }
    // Zero means "no node" everywhere in this file, so it is not available as a
    // real id. One collision in four billion, moved rather than mishandled.
    return hash == 0 ? 1u : hash;
}

/** An untagged node's id: its parent's, and where it sits among that parent's
 *  accessible children. */
AccessibilityId derivedId(AccessibilityId parent, std::size_t index) {
    AccessibilityId hash = hashOf("#", parent);
    hash ^= static_cast<AccessibilityId>(index + 1);
    hash *= 16777619u;
    return hash == 0 ? 1u : hash;
}

/**
 * Whether two nodes would be announced identically.
 *
 * Written out rather than defaulted, because `children` is deliberately part of
 * it — a row inserted above changes nothing about a node except its place, and
 * a reader walking the list has to be told.
 */
bool sameAs(const AccessibilityNode& a, const AccessibilityNode& b) {
    const auto sameRect = [](const Rect& x, const Rect& y) {
        return x.x == y.x && x.y == y.y && x.width == y.width && x.height == y.height;
    };
    const auto sameState = [](const AccessibilityState& x, const AccessibilityState& y) {
        return x.checked == y.checked && x.expanded == y.expanded && x.selected == y.selected &&
               x.pressed == y.pressed && x.disabled == y.disabled && x.readOnly == y.readOnly &&
               x.invalid == y.invalid && x.busy == y.busy && x.required == y.required &&
               x.sorted == y.sorted;
    };
    const auto sameValue = [](const AccessibilityValue& x, const AccessibilityValue& y) {
        return x.present == y.present && x.now == y.now && x.minimum == y.minimum &&
               x.maximum == y.maximum && x.text == y.text;
    };
    return a.role == b.role && a.name == b.name && a.description == b.description &&
           sameState(a.state, b.state) && sameValue(a.value, b.value) &&
           a.positionInSet == b.positionInSet && a.setSize == b.setSize &&
           a.level == b.level &&
           sameRect(a.bounds, b.bounds) && a.parent == b.parent && a.children == b.children &&
           a.labelledBy == b.labelledBy && a.describedBy == b.describedBy &&
           a.controls == b.controls && a.activeDescendant == b.activeDescendant;
}

/** Builds one tree. A struct rather than a pile of parameters: the walk needs
 *  the arena, the focused tag and somewhere to put things. */
struct Builder {
    const Arena& arena;
    std::string_view focused;
    AccessibilityTree out;
    /** Where a node landed, by id, so a relation can be resolved once every
     *  node exists. */
    std::map<AccessibilityId, std::size_t, std::less<>> byId;
    /** Tag to id, for the relations, which are spelled as tags. */
    std::map<std::string_view, AccessibilityId, std::less<>> byTag;

    AccessibilityNode& at(AccessibilityId id) { return out.nodes[byId[id]]; }

    /**
     * Emits the accessible nodes under `node`, attaching them to `parent`.
     *
     * `siblings` counts what has already been attached to that parent, which is
     * what an untagged node's identity is derived from.
     */
    void walk(NodeId node, AccessibilityId parent, std::size_t& siblings) {
        const Node& current = arena[node];
        const Accessibility* info = arena.accessibility(node);

        // `hidden` takes the whole subtree with it. That is what separates it
        // from `Role::None`, which only says *this* node is not a thing.
        if (info != nullptr && info->hidden) return;

        // A node with nothing to say is not a node here; its children are
        // attached to its parent instead. That is the pruning, and it is why a
        // button wrapped in three layout boxes is one node and not four.
        const bool announces = info != nullptr && info->role != Role::None;
        if (!announces) {
            for (NodeId child = current.firstChild; child.valid();
                 child = arena[child].nextSibling) {
                walk(child, parent, siblings);
            }
            return;
        }

        const AccessibilityId id = current.id.empty() ? derivedId(parent, siblings)
                                                      : hashOf(current.id, kFnvOffset);
        ++siblings;

        AccessibilityNode entry;
        entry.id = id;
        entry.tag = current.id;
        entry.role = info->role;
        entry.name = std::string(info->name);
        // A control with no name of its own is named by the text inside it —
        // which is what makes a `listRow` announce its contents and a table cell
        // announce its value without every call site repeating them. Skipped
        // when something else names it: that edge is resolved below and wins,
        // as `aria-labelledby` wins over `aria-label`.
        if (entry.name.empty() && info->relations.labelledBy.empty()) {
            gatherText(node, entry.name, true);
        }
        entry.description = std::string(info->description);
        entry.state = info->state;
        entry.value = info->value;
        entry.positionInSet = info->positionInSet;
        entry.setSize = info->setSize;
        entry.level = info->level;
        entry.bounds = current.frame;
        entry.focused = !current.id.empty() && current.id == focused;
        entry.parent = parent;

        byId[id] = out.nodes.size();
        if (!current.id.empty()) byTag[current.id] = id;
        out.nodes.push_back(std::move(entry));
        if (parent != 0) at(parent).children.push_back(id);

        std::size_t below = 0;
        for (NodeId child = current.firstChild; child.valid(); child = arena[child].nextSibling) {
            walk(child, id, below);
        }
    }

    /**
     * The text inside `node`, stopping at anything that became a node of its
     * own.
     *
     * The simplified accessible-name computation, and the "stopping" is the
     * whole of it: a `listRow` should be announced as the text in it, and a
     * `table` should not be announced as every cell it contains.
     */
    void gatherText(NodeId node, std::string& into, bool top) const {
        const Node& current = arena[node];
        const Accessibility* info = arena.accessibility(node);
        if (info != nullptr && info->hidden) return;
        if (!top && info != nullptr && info->role != Role::None) return;

        if (!current.text.empty()) {
            if (!into.empty()) into += ' ';
            into += current.text;
        }
        for (NodeId child = current.firstChild; child.valid(); child = arena[child].nextSibling) {
            gatherText(child, into, false);
        }
    }

    AccessibilityId idOf(std::string_view tag) const {
        if (tag.empty()) return 0;
        const auto found = byTag.find(tag);
        return found == byTag.end() ? 0 : found->second;
    }

    /**
     * Relations, from tags to ids, in two passes over the arena.
     *
     * **Two, and not one.** The forward direction writes what a node says about
     * itself and the reverse writes what another node says about it — and a
     * caption is built *before* the control it names, so a single pass sets the
     * control's `labelledBy` and then reaches the control itself and overwrites
     * it with the nothing the control knows. That is the bug this shape exists
     * to avoid, and it presented as a caption that was in the tree, correct,
     * and attached to nothing.
     */
    void resolveForward(NodeId node) {
        const Node& current = arena[node];
        const Accessibility* info = arena.accessibility(node);
        if (info != nullptr && info->hidden) return;

        if (info != nullptr && info->role != Role::None && !current.id.empty()) {
            if (const AccessibilityId self = idOf(current.id); self != 0) {
                AccessibilityNode& entry = at(self);
                entry.labelledBy = idOf(info->relations.labelledBy);
                entry.describedBy = idOf(info->relations.describedBy);
                entry.controls = idOf(info->relations.controls);
                entry.activeDescendant = idOf(info->relations.activeDescendant);
            }
        }
        for (NodeId child = current.firstChild; child.valid(); child = arena[child].nextSibling) {
            resolveForward(child);
        }
    }

    void resolveReverse(NodeId node) {
        const Node& current = arena[node];
        const Accessibility* info = arena.accessibility(node);
        if (info != nullptr && info->hidden) return;

        if (info != nullptr) {
            const AccessibilityId self = idOf(current.id);
            if (const AccessibilityId target = idOf(info->relations.labels); target != 0) {
                AccessibilityNode& other = at(target);
                other.labelledBy = self;
                // A control with no name of its own takes the caption's, which is
                // the whole point of the relation and not a shortcut.
                if (other.name.empty() && self != 0) other.name = at(self).name;
                // `required` rides the same edge: the asterisk is drawn on the
                // caption because that is where there is room for it, and it is a
                // statement about the control.
                if (other.state.required == Flag::Unset && info->state.required != Flag::Unset) {
                    other.state.required = info->state.required;
                }
            }
            if (const AccessibilityId target = idOf(info->relations.describes); target != 0) {
                AccessibilityNode& other = at(target);
                other.describedBy = self;
                if (other.description.empty() && self != 0) other.description = at(self).name;
            }
        }
        for (NodeId child = current.firstChild; child.valid(); child = arena[child].nextSibling) {
            resolveReverse(child);
        }
    }
};

}  // namespace

const AccessibilityNode* AccessibilityTree::find(AccessibilityId id) const {
    for (const AccessibilityNode& node : nodes) {
        if (node.id == id) return &node;
    }
    return nullptr;
}

const AccessibilityNode* AccessibilityTree::find(std::string_view tag) const {
    if (tag.empty()) return nullptr;
    for (const AccessibilityNode& node : nodes) {
        if (node.tag == tag) return &node;
    }
    return nullptr;
}

AccessibilityTree buildAccessibilityTree(const Arena& arena, NodeId root,
                                         const Interaction& input) {
    AccessibilityTree tree;
    if (!root.valid() || arena.empty()) return tree;

    Builder builder{arena, input.focused(), {}, {}, {}};

    // A root every tree has, whatever the application built. A screen reader
    // needs somewhere to start, and the outermost box is almost always a layout
    // node that the pruning would otherwise throw away.
    AccessibilityNode window;
    window.id = hashOf("gbui.window", kFnvOffset);
    window.role = Role::Group;
    window.bounds = arena[root].frame;
    builder.byId[window.id] = 0;
    builder.out.nodes.push_back(window);
    builder.out.root = window.id;

    std::size_t siblings = 0;
    builder.walk(root, window.id, siblings);
    builder.resolveForward(root);
    builder.resolveReverse(root);

    return std::move(builder.out);
}

AccessibilityUpdate diffAccessibility(const AccessibilityTree& previous,
                                      const AccessibilityTree& next) {
    AccessibilityUpdate update;

    std::map<AccessibilityId, const AccessibilityNode*, std::less<>> before;
    for (const AccessibilityNode& node : previous.nodes) before[node.id] = &node;

    for (const AccessibilityNode& node : next.nodes) {
        const auto found = before.find(node.id);
        if (found == before.end() || !sameAs(*found->second, node)) {
            update.changed.push_back(node);
        }
        if (node.focused) update.focus = node.id;
    }

    std::map<AccessibilityId, bool, std::less<>> present;
    for (const AccessibilityNode& node : next.nodes) present[node.id] = true;
    for (const AccessibilityNode& node : previous.nodes) {
        if (present.find(node.id) == present.end()) update.removed.push_back(node.id);
    }

    AccessibilityId was = 0;
    for (const AccessibilityNode& node : previous.nodes) {
        if (node.focused) was = node.id;
    }
    // Reported on its own, because focus moves between two nodes that are both
    // otherwise unchanged — and "the keyboard is here now" is the one message a
    // screen reader must never miss.
    update.focusMoved = was != update.focus;

    return update;
}

}  // namespace gbui
