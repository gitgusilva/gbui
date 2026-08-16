#include "gbui/scene/tree.hpp"

#include <iterator>

#include <algorithm>
#include <cassert>
#include <cstring>

namespace gbui {

void Arena::addChild(NodeId parent, NodeId child) {
    assert(parent.valid() && child.valid());
    Node& p = nodes_[parent.index()];
    Node& c = nodes_[child.index()];
    c.parent = parent;
    if (p.lastChild.valid()) {
        nodes_[p.lastChild.index()].nextSibling = child;
    } else {
        p.firstChild = child;
    }
    p.lastChild = child;
}

std::string_view Arena::intern(std::string_view text) {
    if (text.empty()) return {};

    // Anything longer than a block gets a block of its own rather than being
    // split: a view has to be contiguous.
    if (text.size() > kStringBlockSize) {
        stringBlocks_.emplace_back(text.begin(), text.end());
        return {stringBlocks_.back().data(), text.size()};
    }

    if (stringBlocks_.empty() || blockUsed_ + text.size() > kStringBlockSize) {
        stringBlocks_.emplace_back();
        stringBlocks_.back().resize(kStringBlockSize);
        blockUsed_ = 0;
    }

    char* destination = stringBlocks_.back().data() + blockUsed_;
    std::memcpy(destination, text.data(), text.size());
    blockUsed_ += text.size();
    return {destination, text.size()};
}

std::uint32_t Arena::addShapes(std::vector<Shape> shapes) {
    const auto first = static_cast<std::uint32_t>(shapes_.size());
    shapes_.insert(shapes_.end(), std::make_move_iterator(shapes.begin()),
                   std::make_move_iterator(shapes.end()));
    return first;
}

Accessibility& Arena::accessibilityFor(NodeId id) {
    Node& node = nodes_[id.index()];
    if (node.accessibility == Node::kNoAccessibility) {
        node.accessibility = static_cast<std::uint32_t>(accessibility_.size());
        accessibility_.emplace_back();
    }
    return accessibility_[node.accessibility];
}

void Arena::reset() {
    // clear() keeps capacity: the next frame writes over the same pages instead
    // of asking the allocator for them again.
    nodes_.clear();
    shapes_.clear();
    accessibility_.clear();
    if (stringBlocks_.size() > 1) {
        // Keep one block hot and release the rest, so a single huge frame does
        // not pin its memory for the life of the process.
        stringBlocks_.erase(stringBlocks_.begin() + 1, stringBlocks_.end());
    }
    blockUsed_ = 0;
}

std::size_t Arena::bytesUsed() const {
    std::size_t total = nodes_.capacity() * sizeof(Node);
    total += accessibility_.capacity() * sizeof(Accessibility);
    for (const auto& block : stringBlocks_) total += block.capacity();
    return total;
}

}  // namespace gbui
