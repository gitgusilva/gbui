#include "gbui/input/interaction.hpp"

#include <algorithm>

#include "gbui/layout/layout.hpp"

namespace gbui {
namespace {

/** The node carrying `tag`, or an invalid id. Used to follow a gesture back to
 *  the thing that owns it once the pointer has left it. */
NodeId nodeWithTag(const Arena& arena, NodeId root, std::string_view tag) {
    if (!root.valid() || tag.empty()) return {};
    if (arena[root].id == tag) return root;
    for (NodeId child = arena[root].firstChild; child.valid();
         child = arena[child].nextSibling) {
        if (const NodeId found = nodeWithTag(arena, child, tag); found.valid()) return found;
    }
    return {};
}

}  // namespace

std::string_view Interaction::tagAt(const Arena& arena, NodeId root, Vec2 point) {
    NodeId node = hitTest(arena, root, point);
    // The hit is the deepest node, which is usually a label inside a row. The
    // control is the nearest ancestor that named itself.
    while (node.valid()) {
        if (!arena[node].id.empty() && !arena[node].ignoresPointer) return arena[node].id;
        node = arena[node].parent;
    }
    return {};
}

void Interaction::update(const Arena& arena, NodeId root, const InputFrame& frame) {
    previousPointer_ = pointer_;
    pointer_ = frame.pointer;
    wheel_ = frame.wheel;
    modifiers_ = frame.modifiers;
    clicked_.clear();
    pressStarted_.clear();

    const bool haveTree = root.valid() && !arena.empty();
    const std::string_view under = haveTree ? tagAt(arena, root, frame.pointer) : std::string_view{};

    // The innermost thing under the pointer that said it takes the wheel. Found
    // by walking *up* from the deepest hit, so the first one met is the closest
    // one — the list rather than the page it sits on.
    wheelTarget_.clear();
    wheelTargetX_.clear();
    if (haveTree) {
        for (NodeId node = hitTest(arena, root, frame.pointer); node.valid();
             node = arena[node].parent) {
            const Node& candidate = arena[node];
            if (candidate.id.empty()) continue;
            if (candidate.style.overflow == Overflow::Scroll && wheelTarget_.empty()) {
                wheelTarget_ = candidate.id;
            } else if (candidate.style.overflow == Overflow::ScrollX && wheelTargetX_.empty()) {
                wheelTargetX_ = candidate.id;
            }
            if (!wheelTarget_.empty() && !wheelTargetX_.empty()) break;
        }
    }

    // Hover follows the pointer, except while a button is held: a control being
    // dragged keeps the pointer even when it wanders off, which is what makes a
    // slider usable.
    if (!frame.pointerDown) hovered_ = std::string(under);

    // The modality is remembered across frames, not just across this one: it is
    // what a later programmatic focus inherits. Typing does not move focus, so
    // it never lights a ring on its own — it only says which way the user is
    // working right now.
    if (!frame.keys.empty() || !frame.text.empty()) keyboardModality_ = true;
    if (frame.pointerDown && !wasDown_) keyboardModality_ = false;

    if (frame.pointerDown && !wasDown_) {
        pressed_ = std::string(under);
        pressStarted_ = pressed_;
        // Clicking anything moves the keyboard with it, and clicking nothing
        // takes focus away — the same rule every desktop toolkit uses. The ring
        // stays off either way: the pointer already showed where focus went.
        //
        // **The nearest focusable ancestor**, not whatever the pointer landed
        // on. A control is rarely one node: a textarea is a box around a scroll
        // view around a column of runs, and a press on the text resolves to a
        // tag the scroll view invented. Focusing that leaves the keyboard on
        // something no key handler is listening to — click into the box, type,
        // and nothing happens — which is exactly what a textarea did. The
        // browser walks up for the same reason: clicking the text inside a
        // `<textarea>` focuses the textarea.
        //
        // A press that finds nothing focusable above it still clears focus,
        // because clicking the page background is how a reader says "not here".
        std::string_view target;
        if (haveTree) {
            for (NodeId node = hitTest(arena, root, frame.pointer); node.valid();
                 node = arena[node].parent) {
                if (arena[node].focusable && !arena[node].id.empty()) {
                    target = arena[node].id;
                    break;
                }
            }
        }
        focus(target, FocusSource::Pointer);
    } else if (!frame.pointerDown && wasDown_) {
        if (!pressed_.empty() && pressed_ == under) clicked_ = pressed_;
        pressed_.clear();
        hovered_ = std::string(under);
    }

    wasDown_ = frame.pointerDown;
    pointerDown_ = frame.pointerDown;

    // The cursor comes from the node under the pointer, walking up until one
    // has an opinion — so a label inside a button still shows the button's.
    //
    // Except while something is being dragged. A drag is a gesture, not a
    // position: the pointer routinely leaves the thumb it is pulling, and a
    // cursor that flicks back to an arrow halfway through says the drag has
    // ended when it has not. So the node that owns the gesture keeps the
    // cursor until the button comes up, which is what the web does.
    cursor_ = Cursor::Default;
    if (haveTree) {
        viewport_ = arena[root].frame;
        NodeId from = hitTest(arena, root, frame.pointer);
        if (!pressed_.empty()) {
            if (const NodeId held = nodeWithTag(arena, root, pressed_); held.valid()) from = held;
        }
        for (NodeId node = from; node.valid(); node = arena[node].parent) {
            if (arena[node].cursor != Cursor::Default) {
                cursor_ = arena[node].cursor;
                break;
            }
        }
    }

    keys_ = frame.keys;
    text_ = frame.text;

    // Tab moves focus along the controls in tree order. Collected here rather
    // than by the caller because only the tree knows the order they appear in.
    if (haveTree) {
        // Last frame's ancestry, kept before it is rebuilt: it is the only
        // record of where the keyboard was if the node holding it is gone.
        std::vector<std::string> previousChain;
        previousChain.swap(focusChain_);

        focusables_.clear();
        frames_.clear();
        NodeId focusedNode;
        arena.forEach(root, [&](NodeId id, const Node& node, int) {
            if (node.id.empty()) return;
            frames_.insert_or_assign(std::string(node.id), node.frame);
            if (node.focusable) focusables_.emplace_back(node.id);
            if (node.id == focused_) focusedNode = id;
        });

        // A focused node that stopped being built must not take the keyboard
        // with it.
        //
        // This is not a rare case, it is the *normal* case for a virtualised
        // list: click a row, hold an arrow, and the row you clicked scrolls out
        // of the slice and ceases to exist. Focus would then name a node that
        // is not in the tree, `isFocusedWithin` would answer no to every
        // ancestor, and the container's own keyboard handling would go silent —
        // the list stops responding to the very keys that scrolled it away.
        //
        // So focus falls to the nearest ancestor that survived, which is the
        // list itself. That is also what the web does when a focused element is
        // removed from the document, except the web drops focus to the body and
        // this keeps it where the reader was working.
        if (!focused_.empty() && !focusedNode.valid()) {
            for (const std::string& ancestor : previousChain) {
                if (ancestor == focused_) continue;
                if (frames_.find(ancestor) == frames_.end()) continue;
                focused_ = ancestor;
                arena.forEach(root, [&](NodeId id, const Node& node, int) {
                    if (node.id == focused_) focusedNode = id;
                });
                break;
            }
            // Nothing of that ancestry is left either: the whole view went
            // away, and holding a name nobody answers to would keep the ring
            // off screen for good.
            if (!focusedNode.valid()) focused_.clear();
        }

        // Where the focused node sits, so a container can ask whether the
        // keyboard is somewhere inside it.
        for (NodeId node = focusedNode; node.valid(); node = arena[node].parent) {
            if (!arena[node].id.empty()) focusChain_.emplace_back(arena[node].id);
        }
    }

    if (!focusables_.empty() && take(Key::Tab)) {
        const bool backwards = std::any_of(frame.keys.begin(), frame.keys.end(),
                                           [](const KeyEvent& event) {
                                               return event.key == Key::Tab &&
                                                      event.modifiers.shift;
                                           });
        const auto it = std::find(focusables_.begin(), focusables_.end(), focused_);
        if (it == focusables_.end()) {
            focus(backwards ? focusables_.back() : focusables_.front(), FocusSource::Keyboard);
        } else {
            const auto index = static_cast<std::size_t>(std::distance(focusables_.begin(), it));
            const std::size_t count = focusables_.size();
            focus(focusables_[(index + (backwards ? count - 1 : 1)) % count],
                  FocusSource::Keyboard);
        }
    }
}

bool Interaction::isFocusedWithin(std::string_view tag) const {
    if (tag.empty()) return false;
    if (tag == focused_) return true;
    return std::find(focusChain_.begin(), focusChain_.end(), tag) != focusChain_.end();
}

Rect Interaction::frameOf(std::string_view tag) const {
    const auto it = frames_.find(tag);
    return it == frames_.end() ? Rect{} : it->second;
}

bool Interaction::take(Key key) {
    const auto it = std::find_if(keys_.begin(), keys_.end(),
                                 [key](const KeyEvent& event) { return event.key == key; });
    if (it == keys_.end()) return false;
    keys_.erase(it);
    return true;
}

}  // namespace gbui
