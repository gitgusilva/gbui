// The node tree and the arena that owns it.
//
// Memory model, and why it is not shared_ptr:
//
// A UI tree is built often — every time state changes — walked twice per frame
// (layout, then paint) and thrown away whole. That access pattern rewards
// contiguous storage and punishes per-node allocation, so nodes live in one
// growing vector inside an Arena and are addressed by index (NodeId) rather
// than by pointer. The consequences are the point:
//
//   * building a tree is a push_back, not a new — no allocator round trip and
//     no refcount traffic per node;
//   * layout and paint walk memory in the order it was written, which is what
//     the prefetcher wants;
//   * releasing a tree is `clear()`, one operation regardless of node count,
//     with no destructor cascade and no chance of a cycle leaking;
//   * a NodeId survives a reallocation, where a Node* would not.
//
// Children are an intrusive first-child/next-sibling list, so a node costs no
// std::vector of its own — a container with three children allocates nothing.
//
// Text is interned into fixed 4 KiB blocks and referenced as string_view. The
// blocks are never reallocated (a std::deque of arrays, not a vector<char>),
// because a view into a buffer that later grows is a dangling read.
#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

#include "gbui/core/cursor.hpp"
#include "gbui/core/image.hpp"
#include "gbui/core/path.hpp"
#include "gbui/style/style.hpp"

namespace gbui {

/** Index into an Arena. Invalid() is the null of this world; it is checked
 *  rather than dereferenced, so a stale id cannot corrupt memory. */
class NodeId {
public:
    using Index = std::uint32_t;
    static constexpr Index kInvalid = static_cast<Index>(-1);

    constexpr NodeId() = default;
    constexpr explicit NodeId(Index index) : index_(index) {}

    constexpr bool valid() const { return index_ != kInvalid; }
    constexpr Index index() const { return index_; }
    constexpr explicit operator bool() const { return valid(); }
    friend constexpr bool operator==(NodeId, NodeId) = default;

private:
    Index index_ = kInvalid;
};

/** What a node is made of. Kept an aggregate of trivial members plus two views
 *  so that clearing the arena is a size reset, not a walk. */
/** Vector content on a node: path data, its stroke width on the source grid,
 *  and the token it is drawn in. The data is a view into static storage — the
 *  generated icon table — so the node owns nothing. */
struct IconContent {
    std::string_view path{};
    float stroke = 2.0f;
    Fill color{Token::Text};
};

/** A picture on a node: the caller's pixels, and how they meet the node's box.
 *  Borrowed for the frame like the text is — see `Bitmap`. */
struct ImageContent {
    Bitmap source{};
    ImageFit fit = ImageFit::Contain;
    float opacity = 1.0f;
    bool valid() const { return source.valid(); }
};

/**
 * One piece of vector art on a node, in the node's **own** coordinates.
 *
 * Local rather than absolute so the code that builds a chart, a graph or a
 * sparkline works in the box it was given and never has to know where that box
 * ended up. The translation happens once, when the frame is recorded.
 */
struct Shape {
    Path path{};
    Fill color{Token::Text};
    /** Zero fills the contours; anything else strokes them at that width. */
    float stroke = 0.0f;
    /**
     * Painted instead of `color` when it has two or more stops, exactly as
     * `Style::backgroundGradient` is for a box.
     *
     * Measured across the path's own bounding box, so a stroked arc fades
     * along the arc rather than along the node it sits in. Before this existed
     * the only way to fade a dial was to emit a dozen segments at stepped
     * alphas — a dozen paths where one will do.
     *
     * Last in the struct rather than beside `color`, where it belongs by
     * meaning, because `Shape{path, colour, stroke}` is written positionally
     * all over this library and in anyone else's charts. Field order is not a
     * statement about anything; breaking every one of those call sites to make
     * it read better would be.
     */
    Gradient gradient{};
};

struct Node {
    Style style{};
    TextStyle textStyle{};
    IconContent icon{};
    ImageContent image{};
    /** Leaf content. Points into the arena's string blocks, never owned here. */
    std::string_view text{};
    /** Free-form tag for hit testing and tests — "sidebar.item.main". */
    std::string_view id{};
    /** Whether Tab can land here. Only tagged nodes qualify, because focus has
     *  to survive the tree being rebuilt and a tag is the only thing that does. */
    bool focusable = false;
    /** A tag that names geometry rather than a control. Hit testing walks past
     *  it to the tagged ancestor, so a text field can name the box its caret is
     *  measured against without that box swallowing the click meant for the
     *  field. `frameOf` still finds it — naming and targeting are two jobs. */
    bool ignoresPointer = false;
    /** What the pointer looks like over this node. Default means "ask my
     *  parent", so a label inside a button inherits the button's. */
    Cursor cursor = Cursor::Default;

    /** A slice of the arena's shape store, drawn after this node's background
     *  and before its children. A range rather than a vector so a node stays
     *  trivial to clear — the arena owns the geometry, as it owns the text. */
    std::uint32_t firstShape = 0;
    std::uint32_t shapeCount = 0;

    NodeId parent{};
    NodeId firstChild{};
    NodeId lastChild{};
    NodeId nextSibling{};

    /** Written by layout, in absolute window coordinates. */
    Rect frame{};
    /** Content box: frame minus padding and border. */
    Rect content{};
};

class Arena {
public:
    Arena() = default;
    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;
    Arena(Arena&&) noexcept = default;
    Arena& operator=(Arena&&) noexcept = default;

    /** Reserving up front turns a whole frame's building into zero reallocs. */
    void reserve(std::size_t nodes) { nodes_.reserve(nodes); }

    NodeId create(const Style& style = {}) {
        nodes_.push_back(Node{});
        nodes_.back().style = style;
        return NodeId(static_cast<NodeId::Index>(nodes_.size() - 1));
    }

    Node& operator[](NodeId id) { return nodes_[id.index()]; }
    const Node& operator[](NodeId id) const { return nodes_[id.index()]; }

    std::size_t size() const { return nodes_.size(); }
    bool empty() const { return nodes_.empty(); }

    /** Appends a child. O(1): the parent keeps a tail pointer, so building a
     *  long list of commits does not degrade into a quadratic walk. */
    void addChild(NodeId parent, NodeId child);

    /** Copies text into the arena and returns a view valid until reset(). */
    std::string_view intern(std::string_view text);

    /** Takes ownership of vector art and returns where it landed. Contiguous,
     *  so a node names a range rather than holding a container of its own. */
    std::uint32_t addShapes(std::vector<Shape> shapes);
    const Shape& shape(std::uint32_t index) const { return shapes_[index]; }
    std::size_t shapeCount() const { return shapes_.size(); }

    /** Drops every node and every interned string, keeping the capacity so the
     *  next frame reuses the same memory. */
    void reset();

    /** Depth-first walk, parents before children, in insertion order. */
    template <typename Fn>
    void forEach(NodeId root, Fn&& fn) const {
        if (!root.valid()) return;
        walk(root, 0, fn);
    }

    /** Total bytes held, for the diagnostics the app already shows elsewhere. */
    std::size_t bytesUsed() const;

private:
    std::vector<Shape> shapes_;

    static constexpr std::size_t kStringBlockSize = 4096;

    template <typename Fn>
    void walk(NodeId id, int depth, Fn& fn) const {
        fn(id, nodes_[id.index()], depth);
        for (NodeId child = nodes_[id.index()].firstChild; child.valid();
             child = nodes_[child.index()].nextSibling) {
            walk(child, depth + 1, fn);
        }
    }

    std::vector<Node> nodes_;
    // A deque of fixed blocks: appending never moves what is already interned.
    std::deque<std::vector<char>> stringBlocks_;
    std::size_t blockUsed_ = 0;
};

}  // namespace gbui
