#include "gbui/layout/layout.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "gbui/layout/textWrap.hpp"

namespace gbui {
namespace {

bool isRow(Direction d) { return d == Direction::Row; }

/**
 * The container size a percentage is measured against, per axis.
 *
 * Both are `kAuto` during the intrinsic passes, because a container sizing
 * itself to its children cannot also be what their percentages are a share of —
 * that is circular, and CSS resolves it the same way: a percentage against an
 * unknown basis behaves as `auto`.
 */
struct Basis {
    float width = kAuto;
    float height = kAuto;

    float along(bool horizontal) const { return horizontal ? width : height; }
};

/** A length in pixels, given what a percentage would be a share of. */
float sizeOf(const Length& length, float basis) { return length.resolve(basis); }

/** Counting UTF-8 lead bytes rather than bytes keeps accented text from
 *  measuring wider than it draws. */
std::size_t countGlyphs(std::string_view text) {
    std::size_t glyphs = 0;
    for (const char ch : text) {
        if ((static_cast<unsigned char>(ch) & 0xC0U) != 0x80U) ++glyphs;
    }
    return glyphs;
}

float clampTo(float value, float minimum, float maximum) {
    // An auto minimum is resolved by the caller; anything that still holds NaN
    // here would poison the comparison and propagate a NaN frame.
    const float floorValue = isAuto(minimum) ? 0.0f : minimum;
    return std::max(floorValue, std::min(value, maximum));
}

/** The font size a run resolves to: its own, or the theme's for that role. */
float fontSizeOf(const TextStyle& style, const Typography& typography) {
    if (!isAuto(style.size)) return style.size;
    switch (style.role) {
        case FontRole::Editor: return typography.editorFontSize;
        case FontRole::Mono: return typography.uiFontSize;
        case FontRole::Ui: break;
    }
    return typography.uiFontSize;
}

/** Space taken by the box itself — padding plus border on both sides. */
Edges frameEdges(const Style& style) {
    const float b = style.border.visible() ? style.border.width : 0.0f;
    return {
        style.padding.top + b,
        style.padding.right + b,
        style.padding.bottom + b,
        style.padding.left + b,
    };
}

struct Axis {
    bool row;

    float main(float w, float h) const { return row ? w : h; }
    float cross(float w, float h) const { return row ? h : w; }
    float mainEdges(const Edges& e) const { return row ? e.horizontal() : e.vertical(); }
    float crossEdges(const Edges& e) const { return row ? e.vertical() : e.horizontal(); }
    float mainStart(const Edges& e) const { return row ? e.left : e.top; }
    float crossStart(const Edges& e) const { return row ? e.top : e.left; }
};

/** Per-child scratch for one container's pass. Lives on the stack of layoutNode
 *  in a vector reused across siblings — the only allocation the engine makes,
 *  and it is amortised away by the caller's reserve. */
struct Item {
    NodeId id;
    float base = 0.0f;      ///< main size before growing or shrinking
    float target = 0.0f;    ///< main size after distribution
    float minMain = 0.0f;   ///< resolved floor, content-based when auto
    float maxMain = std::numeric_limits<float>::infinity();
    float grow = 0.0f;
    float shrink = 1.0f;
    /** Set once an item has hit a bound; the remaining space is shared out
     *  among the others. See resolveFlexibleLengths. */
    bool frozen = false;
    Edges margin{};
};

class Engine {
public:
    Engine(Arena& arena, const LayoutContext& context)
        : arena_(arena), context_(context),
          typography_(context.theme ? context.theme->typography() : Typography{}) {}

    void run(NodeId root, const Rect& viewport) {
        viewport_ = viewport;
        Node& node = arena_[root];
        const Edges margin = node.style.margin;
        // The root's percentages are a share of the viewport it was handed.
        const Basis basis{viewport.width, viewport.height};
        const float width =
            resolveSize(sizeOf(node.style.width, basis.width), viewport.width - margin.horizontal(),
                        sizeOf(node.style.minWidth, basis.width),
                        sizeOf(node.style.maxWidth, basis.width));
        const float height = resolveSize(sizeOf(node.style.height, basis.height),
                                         viewport.height - margin.vertical(),
                                         sizeOf(node.style.minHeight, basis.height),
                                         sizeOf(node.style.maxHeight, basis.height));
        place(root, Rect{viewport.x + margin.left, viewport.y + margin.top, width, height});
    }

    /**
     * Natural size along one axis. The axis is the caller's, not this node's:
     * a row asks its children for their width even when a child is itself a
     * column, and getting that backwards makes every auto-sized box collapse.
     *
     * The answer is clamped by this node's own minimum and maximum, and that is
     * not a detail — it is the difference between a container that is as tall
     * as its contents and one that is *nearly* as tall. A control that declares
     * a floor rather than a fixed size is the common case, not the exotic one:
     * `button` sets `minHeight` deliberately ("a floor, not a fixed size", so a
     * large label is not cropped) and leaves the height auto. Measured without
     * the floor, a button reports the height of the glyph inside it — around
     * seven pixels short — and every row holding one measures short with it.
     *
     * Seven pixels is enough to matter, because the two things that read this
     * number are the two that cannot afford it: an auto-sized box gives its
     * children less than it drew, and a scroll view computes a content size
     * under the real one and stops short of the end. The calendar in a popover
     * was both at once — the box measured a header's worth too short, so the
     * last week hung out of it and the scroll would not reach.
     */
    float intrinsicAlong(NodeId id, bool horizontal, float available) {
        return clampIntrinsic(id, horizontal, intrinsicContentAlong(id, horizontal, available));
    }

    /** `intrinsicAlong` without the clamp — the content's own measurement. */
    float intrinsicContentAlong(NodeId id, bool horizontal, float available) {
        const Node& node = arena_[id];
        const Style& style = node.style;
        // Intrinsic sizing has no basis to share out, so a percentage is auto.
        const float explicitSize = sizeOf(horizontal ? style.width : style.height, kAuto);
        if (!isAuto(explicitSize)) return explicitSize;

        const Edges edges = frameEdges(style);
        const float edgeTotal = horizontal ? edges.horizontal() : edges.vertical();

        if (!node.text.empty()) {
            // Asking for the height means asking "how tall is this at that
            // width", and the width a run gets is the *content* box — so the
            // padding and border come off before it is handed over. Getting
            // that wrong makes a padded paragraph measure one line short.
            const float contentWidth =
                horizontal ? std::numeric_limits<float>::infinity()
                           : std::max(0.0f, available - edges.horizontal());
            const auto metrics = measure(node, contentWidth);
            return (horizontal ? metrics.width : metrics.height) + edgeTotal;
        }

        // Children stack along this node's own direction and overlap across it.
        const bool stacksAlongAxis = isRow(style.direction) == horizontal;

        // Asking a row how tall it is means asking how tall its children are at
        // the widths they will actually get — which is a question only the flex
        // pass can answer. Handing each of them the row's whole width instead
        // makes a wrapped run measure one line where it will draw three.
        if (!stacksAlongAxis && !horizontal && isRow(style.direction)) {
            return tallestOnceResolved(id, std::max(0.0f, available - edges.horizontal())) +
                   edgeTotal;
        }

        float total = 0.0f;
        float largest = 0.0f;
        int count = 0;
        for (NodeId child = node.firstChild; child.valid(); child = arena_[child].nextSibling) {
            if (isFloating(arena_[child].style.position)) continue;
            const Edges m = arena_[child].style.margin;
            const float margins = horizontal ? m.horizontal() : m.vertical();
            const float size = intrinsicAlong(child, horizontal, available) + margins;
            total += size;
            largest = std::max(largest, size);
            ++count;
        }
        if (stacksAlongAxis) {
            if (count > 1) total += style.gap * static_cast<float>(count - 1);
            return total + edgeTotal;
        }
        return largest + edgeTotal;
    }

    /**
     * A measurement held between this node's own minimum and maximum.
     *
     * A percentage cannot be resolved here — intrinsic sizing has no basis to
     * take a share of — so one is left alone rather than guessed at, which is
     * the same answer the explicit size gets a few lines above.
     */
    float clampIntrinsic(NodeId id, bool horizontal, float size) {
        const Style& style = arena_[id].style;
        const float minimum = sizeOf(horizontal ? style.minWidth : style.minHeight, kAuto);
        const float maximum = sizeOf(horizontal ? style.maxWidth : style.maxHeight, kAuto);
        if (!isAuto(maximum)) size = std::min(size, maximum);
        if (!isAuto(minimum)) size = std::max(size, minimum);
        return size;
    }

    /**
     * The smallest this subtree can be along an axis without its content being
     * destroyed — CSS's min-content, which is what `min-width: auto` resolves
     * to for a flex item. Without it a crowded row squeezes its children to
     * slivers instead of overflowing honestly.
     *
     * Two cases answer zero, following the same rule CSS uses: a node that
     * clips its overflow, and a run of text that is allowed to elide. Both have
     * a defined way to survive being too small, so they do not force the
     * container open.
     */
    float minContentAlong(NodeId id, bool horizontal) {
        const Node& node = arena_[id];
        const Style& style = node.style;

        const float explicitMin = sizeOf(horizontal ? style.minWidth : style.minHeight, kAuto);
        if (!isAuto(explicitMin)) return explicitMin;

        if (clips(style.overflow)) return 0.0f;

        const Edges edges = frameEdges(style);
        const float edgeTotal = horizontal ? edges.horizontal() : edges.vertical();

        if (!node.text.empty()) {
            if (node.textStyle.overflow == TextOverflow::Ellipsis) return 0.0f;
            if (!horizontal) {
                return measure(node, std::numeric_limits<float>::infinity()).height + edgeTotal;
            }
            // The longest run that cannot be broken decides the floor.
            float widest = 0.0f;
            std::size_t start = 0;
            const std::string_view text = node.text;
            for (std::size_t i = 0; i <= text.size(); ++i) {
                if (i != text.size() && text[i] != ' ') continue;
                const std::string_view word = text.substr(start, i - start);
                if (!word.empty()) {
                    Node probe = node;
                    probe.text = word;
                    widest = std::max(widest,
                                      measure(probe, std::numeric_limits<float>::infinity()).width);
                }
                start = i + 1;
            }
            return widest + edgeTotal;
        }

        const bool stacksAlongAxis = isRow(style.direction) == horizontal;
        float total = 0.0f;
        float largest = 0.0f;
        int count = 0;
        for (NodeId child = node.firstChild; child.valid(); child = arena_[child].nextSibling) {
            const Style& cs = arena_[child].style;
            if (isFloating(cs.position)) continue;  // out of the flow
            const Edges m = cs.margin;
            const float margins = horizontal ? m.horizontal() : m.vertical();
            // A child that cannot shrink contributes its whole size, not just
            // its content minimum: nothing will take it back later.
            const float explicitSize = sizeOf(horizontal ? cs.width : cs.height, kAuto);
            const bool rigid = cs.shrink <= 0.0f && !isAuto(explicitSize);
            const float size =
                (rigid ? explicitSize : minContentAlong(child, horizontal)) + margins;
            total += size;
            largest = std::max(largest, size);
            ++count;
        }
        if (stacksAlongAxis) {
            if (count > 1) total += style.gap * static_cast<float>(count - 1);
            return total + edgeTotal;
        }
        return largest + edgeTotal;
    }

    /** Along whichever axis this node lays its own children out. */
    float intrinsicMain(NodeId id) {
        return intrinsicAlong(id, isRow(arena_[id].style.direction),
                              std::numeric_limits<float>::infinity());
    }

private:
    /** One line of a wrapping container: a slice of `scratch_`, and how much
     *  room it takes across. */
    struct Line {
        std::size_t first = 0;
        std::size_t count = 0;
        float cross = 0.0f;
    };

    Arena& arena_;
    const LayoutContext& context_;
    Rect viewport_{};
    Typography typography_;
    std::vector<Item> scratch_;
    /** Indexed with a base the same way `scratch_` is, so a wrapping container
     *  nested inside another does not clobber its parent's lines. */
    std::vector<Line> lines_;

    TextMetrics measure(const Node& node, float available) const {
        if (!context_.measure) return {};
        return context_.measure(node.text, node.textStyle, typography_, available);
    }

    static float resolveSize(float requested, float fallback, float minimum, float maximum) {
        return clampTo(isAuto(requested) ? fallback : requested, minimum, maximum);
    }

    /**
     * Turns a node's in-flow children into flex items on `scratch_`, and
     * returns how many. The caller records `scratch_.size()` first and resizes
     * back to it when done, because this recurses and the recursion pushes.
     *
     * Shared by placement and by the intrinsic pass, so both agree on what an
     * item's base size is — the two answering differently is how a box ends up
     * sized for one layout and drawn in another.
     */
    std::size_t collectItems(NodeId id, const Axis& axis, float availableCross,
                             const Basis& basis) {
        const Node& node = arena_[id];
        std::size_t count = 0;
        for (NodeId child = node.firstChild; child.valid(); child = arena_[child].nextSibling) {
            const Style& cs = arena_[child].style;
            if (isFloating(cs.position)) continue;
            Item item;
            item.id = child;
            item.grow = cs.grow;
            item.shrink = cs.shrink;
            item.margin = cs.margin;

            const float explicitMain =
                axis.main(sizeOf(cs.width, basis.width), sizeOf(cs.height, basis.height));
            const float declaredBasis = sizeOf(cs.basis, basis.along(axis.row));
            if (!isAuto(declaredBasis)) item.base = declaredBasis;
            else if (!isAuto(explicitMain)) item.base = explicitMain;
            else {
                // In a column the main size is a height, and a height depends
                // on the width this child will actually get — the cross space
                // less its own margins, not the container's content box.
                item.base = intrinsicAlong(
                    child, axis.row,
                    std::max(0.0f, availableCross - axis.crossEdges(item.margin)));
            }

            const float declaredMin =
                axis.main(sizeOf(cs.minWidth, basis.width), sizeOf(cs.minHeight, basis.height));
            item.minMain = isAuto(declaredMin) ? minContentAlong(child, axis.row) : declaredMin;
            item.maxMain =
                axis.main(sizeOf(cs.maxWidth, basis.width), sizeOf(cs.maxHeight, basis.height));
            item.base = clampTo(item.base, item.minMain, item.maxMain);
            item.target = item.base;

            scratch_.push_back(item);
            ++count;
        }
        return count;
    }

    /**
     * Breaks a container's items into lines, appending them to `lines_`.
     *
     * Greedy, as the flexbox spec describes: an item goes on the current line
     * unless it would overflow, and an item that does not fit on a line of its
     * own still gets one — otherwise a single over-wide child would produce an
     * empty line and loop.
     */
    std::size_t breakIntoLines(const Style& style, const Axis& axis, std::size_t base,
                               std::size_t count, float availableMain) {
        const std::size_t lineBase = lines_.size();
        if (!style.wrap || count == 0) {
            lines_.push_back({base, count, 0.0f});
            return lineBase;
        }

        std::size_t first = base;
        std::size_t inLine = 0;
        float used = 0.0f;
        for (std::size_t i = 0; i < count; ++i) {
            const Item& item = scratch_[base + i];
            // The *hypothetical* main size — base plus margins — which is what
            // the spec breaks on, before any growing or shrinking.
            const float outer = item.base + axis.mainEdges(item.margin);
            const float withGap = inLine > 0 ? style.gap + outer : outer;
            if (inLine > 0 && used + withGap > availableMain + 0.01f) {
                lines_.push_back({first, inLine, 0.0f});
                first = base + i;
                used = outer;
                inLine = 1;
                continue;
            }
            used += withGap;
            ++inLine;
        }
        if (inLine > 0) lines_.push_back({first, inLine, 0.0f});
        return lineBase;
    }

    /** The cross size a line needs: the tallest item in it, measured at the
     *  main size the flex pass gave it. */
    float crossSizeOfLine(const Axis& axis, const Line& line, const Basis& basis) {
        float tallest = 0.0f;
        for (std::size_t i = 0; i < line.count; ++i) {
            const Item item = scratch_[line.first + i];
            const Style& cs = arena_[item.id].style;
            float cross = axis.cross(sizeOf(cs.width, basis.width), sizeOf(cs.height, basis.height));
            if (isAuto(cross)) {
                cross = intrinsicAlong(item.id, !axis.row,
                                       axis.row ? item.target
                                                : std::numeric_limits<float>::infinity());
            }
            cross = clampTo(cross,
                            axis.cross(sizeOf(cs.minWidth, basis.width),
                                       sizeOf(cs.minHeight, basis.height)),
                            axis.cross(sizeOf(cs.maxWidth, basis.width),
                                       sizeOf(cs.maxHeight, basis.height)));
            tallest = std::max(tallest, cross + axis.crossEdges(item.margin));
        }
        return tallest;
    }

    /**
     * How tall a row is: the tallest of its children measured at the widths the
     * flex pass gives them, not at the row's own width. This is the one place
     * the intrinsic pass has to do real work rather than ask a child a
     * question, and it is what makes a paragraph beside a label come out the
     * right height.
     */
    float tallestOnceResolved(NodeId id, float availableMain) {
        const Axis axis{true};
        const std::size_t base = scratch_.size();
        // The cross space is the height being computed, so it is not known yet;
        // it only feeds a child's *main* base size, where an unbounded cross is
        // the right assumption anyway.
        // No basis: this *is* the pass that decides the container's size.
        const Basis basis{};
        const std::size_t count =
            collectItems(id, axis, std::numeric_limits<float>::infinity(), basis);

        const Style& style = arena_[id].style;
        const float gapTotal = count > 1 ? style.gap * static_cast<float>(count - 1) : 0.0f;
        float used = gapTotal;
        for (std::size_t i = 0; i < count; ++i) {
            used += scratch_[base + i].base + axis.mainEdges(scratch_[base + i].margin);
        }
        // A wrapping row is as tall as its lines *stacked*, not as its tallest
        // child: that is the whole difference between a row that overflows and
        // one that reflows, and getting it wrong here means the container is
        // sized for one line and draws three.
        if (style.wrap) {
            const std::size_t lineBase = breakIntoLines(style, axis, base, count, availableMain);
            const std::size_t lineCount = lines_.size() - lineBase;
            const float crossGap = isAuto(style.crossGap) ? style.gap : style.crossGap;

            float total = 0.0f;
            for (std::size_t i = 0; i < lineCount; ++i) {
                Line& line = lines_[lineBase + i];
                float lineUsed = line.count > 1 ? style.gap * static_cast<float>(line.count - 1)
                                                : 0.0f;
                for (std::size_t k = 0; k < line.count; ++k) {
                    lineUsed += scratch_[line.first + k].base +
                                axis.mainEdges(scratch_[line.first + k].margin);
                }
                resolveFlexibleLengths(line.first, line.count, availableMain - lineUsed);
                total += crossSizeOfLine(axis, line, basis);
            }
            if (lineCount > 1) total += crossGap * static_cast<float>(lineCount - 1);
            lines_.resize(lineBase);
            scratch_.resize(base);
            return total;
        }

        resolveFlexibleLengths(base, count, availableMain - used);

        float tallest = 0.0f;
        for (std::size_t i = 0; i < count; ++i) {
            // Copied: measuring recurses, and the recursion pushes onto the
            // same vector that a reference would point into.
            const Item item = scratch_[base + i];
            tallest = std::max(tallest, intrinsicAlong(item.id, false, item.target) +
                                            item.margin.vertical());
        }
        scratch_.resize(base);
        return tallest;
    }

    /**
     * CSS's "resolving flexible lengths", in the loop the spec actually
     * describes: hand out the free space, freeze whatever hit a bound, and give
     * what those items refused to the ones still able to move. Distributing
     * once — which is what this used to do — loses the surplus of every item
     * that ran into a max, and leaves the container short.
     */
    void resolveFlexibleLengths(std::size_t base, std::size_t count, float freeSpace) {
        if (count == 0) return;
        const bool growing = freeSpace > 0.0f;

        // An item with no flex factor in the direction being resolved never
        // moves, so it starts frozen.
        for (std::size_t i = 0; i < count; ++i) {
            Item& item = scratch_[base + i];
            item.target = item.base;
            item.frozen = growing ? item.grow <= 0.0f : item.shrink <= 0.0f;
        }
        if (freeSpace == 0.0f) return;

        // Each pass freezes at least one item, so the loop is bounded by the
        // number of items; the guard is belt and braces.
        for (std::size_t pass = 0; pass <= count; ++pass) {
            float factorTotal = 0.0f;
            float remaining = freeSpace;
            for (std::size_t i = 0; i < count; ++i) {
                const Item& item = scratch_[base + i];
                if (item.frozen) {
                    remaining -= item.target - item.base;
                    continue;
                }
                factorTotal += growing ? item.grow : item.shrink * item.base;
            }
            if (factorTotal <= 0.0f || std::fabs(remaining) < 0.01f) return;

            float violation = 0.0f;
            for (std::size_t i = 0; i < count; ++i) {
                Item& item = scratch_[base + i];
                if (item.frozen) continue;
                const float factor = growing ? item.grow : item.shrink * item.base;
                const float wanted = item.base + remaining * (factor / factorTotal);
                const float clamped = clampTo(wanted, item.minMain, item.maxMain);
                item.target = clamped;
                violation += clamped - wanted;
            }

            if (std::fabs(violation) < 0.01f) return;
            // Positive means items were held up by their minimum, negative that
            // they were held down by their maximum. Freeze that side and let
            // the rest absorb what is left.
            for (std::size_t i = 0; i < count; ++i) {
                Item& item = scratch_[base + i];
                if (item.frozen) continue;
                const bool atMin = item.target <= item.minMain + 0.01f;
                const bool atMax = item.target >= item.maxMain - 0.01f;
                if ((violation > 0.0f && atMin) || (violation < 0.0f && atMax)) item.frozen = true;
            }
        }
    }

    void place(NodeId id, const Rect& frame) {
        Node& node = arena_[id];
        const Style style = node.style;  // copied: the vector below may grow
        const Edges edges = frameEdges(style);

        node.frame = frame;
        node.content = frame.deflate(edges);

        if (!node.firstChild.valid()) return;

        const Axis axis{isRow(style.direction)};
        const float availableMain = axis.main(node.content.width, node.content.height);
        const float availableCross = axis.cross(node.content.width, node.content.height);

        // ---- children out of the flow --------------------------------------
        // `Absolute` is measured from this node's content box, so a caret, a
        // scrolled pane or a slider's knob is placed where it belongs without
        // anyone having to know where the parent ended up. `Fixed` is measured
        // from the window, which is what a popup anchored at a computed point
        // needs so the panel that built it cannot drag it around.
        for (NodeId child = node.firstChild; child.valid(); child = arena_[child].nextSibling) {
            const Style& cs = arena_[child].style;
            if (!isFloating(cs.position)) continue;
            // A floating child's percentages share the box it is anchored to:
            // the parent's content for `Absolute`, the window for `Fixed`.
            const Basis floatBasis =
                cs.position == Position::Absolute
                    ? Basis{node.content.width, node.content.height}
                    : Basis{viewport_.width, viewport_.height};
            const float declaredWidth = sizeOf(cs.width, floatBasis.width);
            const float declaredHeight = sizeOf(cs.height, floatBasis.height);
            const float width = isAuto(declaredWidth)
                                    ? intrinsicAlong(child, true, viewport_.width)
                                    : declaredWidth;
            const float height =
                isAuto(declaredHeight) ? intrinsicAlong(child, false, width) : declaredHeight;
            const Vec2 origin = cs.position == Position::Absolute
                                    ? Vec2{node.content.x, node.content.y}
                                    : Vec2{0.0f, 0.0f};
            place(child, Rect{origin.x + resolve(cs.left, 0.0f),
                              origin.y + resolve(cs.top, 0.0f),
                              clampTo(width, sizeOf(cs.minWidth, floatBasis.width),
                                      sizeOf(cs.maxWidth, floatBasis.width)),
                              clampTo(height, sizeOf(cs.minHeight, floatBasis.height),
                                      sizeOf(cs.maxHeight, floatBasis.height))});
        }

        // ---- collect ------------------------------------------------------
        const std::size_t base = scratch_.size();
        // Children measure their percentages against this box's content.
        const Basis basis{node.content.width, node.content.height};
        const std::size_t count = collectItems(id, axis, availableCross, basis);

        // ---- break into lines ---------------------------------------------
        // One line unless `wrap` says otherwise, so a container that does not
        // wrap runs exactly the path it always did.
        const std::size_t lineBase = breakIntoLines(style, axis, base, count, availableMain);
        const std::size_t lineCount = lines_.size() - lineBase;
        const float crossGap = isAuto(style.crossGap) ? style.gap : style.crossGap;

        float crossUsed = 0.0f;
        if (style.wrap) {
            for (std::size_t i = 0; i < lineCount; ++i) {
                Line& line = lines_[lineBase + i];
                // Each line resolves its own flexible lengths: an item only ever
                // competes for space with the items beside it, which is the
                // whole point of having lines.
                float lineUsed = line.count > 1 ? style.gap * static_cast<float>(line.count - 1)
                                                : 0.0f;
                for (std::size_t k = 0; k < line.count; ++k) {
                    lineUsed += scratch_[line.first + k].base +
                                axis.mainEdges(scratch_[line.first + k].margin);
                }
                resolveFlexibleLengths(line.first, line.count, availableMain - lineUsed);
                line.cross = crossSizeOfLine(axis, line, basis);
                crossUsed += line.cross;
            }
            if (lineCount > 1) crossUsed += crossGap * static_cast<float>(lineCount - 1);
        }

        // Where the block of lines starts, and how much extra each one takes.
        float lineCursor = 0.0f;
        float lineSpacing = crossGap;
        float lineStretch = 0.0f;
        if (style.wrap && lineCount > 0) {
            const float slack = std::max(0.0f, availableCross - crossUsed);
            switch (style.alignContent) {
                case AlignContent::Start: break;
                case AlignContent::Center: lineCursor = slack / 2.0f; break;
                case AlignContent::End: lineCursor = slack; break;
                case AlignContent::Stretch:
                    lineStretch = slack / static_cast<float>(lineCount);
                    break;
                case AlignContent::SpaceBetween:
                    if (lineCount > 1) lineSpacing += slack / static_cast<float>(lineCount - 1);
                    break;
                case AlignContent::SpaceAround:
                    if (lineCount > 0) {
                        const float unit = slack / static_cast<float>(lineCount);
                        lineCursor = unit / 2.0f;
                        lineSpacing += unit;
                    }
                    break;
            }
        }

        for (std::size_t lineIndex = 0; lineIndex < lineCount; ++lineIndex) {
            const Line line = lines_[lineBase + lineIndex];
            // A container that does not wrap has one line filling it, which is
            // what keeps `Align::Stretch` stretching to the whole container.
            const float lineCross = style.wrap ? line.cross + lineStretch : availableCross;
            placeLine(node, style, axis, line.first, line.count, availableMain, lineCursor,
                      lineCross, basis);
            lineCursor += lineCross + lineSpacing;
        }

        lines_.resize(lineBase);
        scratch_.resize(base);
    }

    /** Lays one line of items out: the free space along the main axis, then
     *  each item's cross size and alignment within the line. */
    void placeLine(Node& node, const Style& style, const Axis& axis, std::size_t base,
                   std::size_t count, float availableMain, float lineStart, float availableCross,
                   const Basis& basis) {

        const float gapTotal = count > 1 ? style.gap * static_cast<float>(count - 1) : 0.0f;
        if (!style.wrap) {
            // A single line resolves here; a wrapping container already did it
            // per line, and redoing it would hand every line the whole
            // container's free space.
            float used = gapTotal;
            for (std::size_t i = 0; i < count; ++i) {
                used += scratch_[base + i].base + axis.mainEdges(scratch_[base + i].margin);
            }
            resolveFlexibleLengths(base, count, availableMain - used);
        }

        float consumed = gapTotal;
        for (std::size_t i = 0; i < count; ++i) {
            consumed += scratch_[base + i].target + axis.mainEdges(scratch_[base + i].margin);
        }
        const float leftover = std::max(0.0f, availableMain - consumed);

        // ---- main-axis placement ------------------------------------------
        float cursor = 0.0f;
        float spacing = style.gap;
        switch (style.justify) {
            case Justify::Start: break;
            case Justify::Center: cursor = leftover / 2.0f; break;
            case Justify::End: cursor = leftover; break;
            case Justify::SpaceBetween:
                if (count > 1) spacing += leftover / static_cast<float>(count - 1);
                break;
            case Justify::SpaceAround:
                if (count > 0) {
                    const float unit = leftover / static_cast<float>(count);
                    cursor = unit / 2.0f;
                    spacing += unit;
                }
                break;
            case Justify::SpaceEvenly:
                if (count > 0) {
                    const float unit = leftover / static_cast<float>(count + 1);
                    cursor = unit;
                    spacing += unit;
                }
                break;
        }

        for (std::size_t i = 0; i < count; ++i) {
            // Copied, not referenced: placing this child recurses, the recursion
            // pushes its own items onto the same scratch vector, and a reference
            // into it would dangle the moment that vector grows.
            const Item item = scratch_[base + i];
            const Style& cs = arena_[item.id].style;
            const Align align = cs.alignSelf.value_or(style.align);

            const float outerCross = availableCross - axis.crossEdges(item.margin);
            float crossSize = axis.cross(sizeOf(cs.width, basis.width),
                                         sizeOf(cs.height, basis.height));
            if (isAuto(crossSize)) {
                // In a row the cross axis is the height, and the width that
                // height was computed from is this item's resolved main size,
                // not the row's height.
                const float availableFor = axis.row ? item.target : outerCross;
                crossSize = align == Align::Stretch
                                ? outerCross
                                : intrinsicAlong(item.id, !axis.row, availableFor);
            }
            crossSize = clampTo(crossSize,
                                axis.cross(sizeOf(cs.minWidth, basis.width),
                                           sizeOf(cs.minHeight, basis.height)),
                                axis.cross(sizeOf(cs.maxWidth, basis.width),
                                           sizeOf(cs.maxHeight, basis.height)));
            crossSize = std::min(crossSize, std::max(0.0f, outerCross));

            float crossOffset = 0.0f;
            switch (align) {
                case Align::Start:
                case Align::Baseline:  // no baseline pass yet; documented in the header
                case Align::Stretch: break;
                case Align::Center: crossOffset = (outerCross - crossSize) / 2.0f; break;
                case Align::End: crossOffset = outerCross - crossSize; break;
            }

            const float mainPos = cursor + axis.mainStart(item.margin);
            // `lineStart` is where this line begins across the container; for a
            // container that does not wrap it is simply zero.
            const float crossPos =
                lineStart + axis.crossStart(item.margin) + std::max(0.0f, crossOffset);

            const Rect childFrame =
                axis.row ? Rect{node.content.x + mainPos, node.content.y + crossPos, item.target, crossSize}
                         : Rect{node.content.x + crossPos, node.content.y + mainPos, crossSize, item.target};

            place(item.id, childFrame);
            cursor += item.target + axis.mainEdges(item.margin) + spacing;
        }
        // `scratch_` is *not* trimmed here: the caller owns the whole run of
        // items and the lines after this one still point into it.
    }
};

}  // namespace

TextMetrics approximateTextMetrics(std::string_view text, const TextStyle& style,
                                   const Typography& typography, float maxWidth) {
    const float size = fontSizeOf(style, typography);
    // Heavier faces are wider, and an italic is not. The factor is small but
    // the estimate is what decides elision, so a heading measured as regular
    // gets cut a character early.
    const float weightFactor =
        1.0f + (static_cast<float>(style.weight) - 400.0f) / 400.0f * 0.06f;
    const float advance =
        (style.role == FontRole::Ui ? 0.52f : 0.60f) * size * weightFactor;
    const float lineHeight = style.lineHeight > 0.0f ? style.lineHeight * size : size * 1.45f;

    const std::size_t glyphs = countGlyphs(text);

    const float natural = static_cast<float>(glyphs) * advance;
    // A single-line run reports its natural width even when that overflows: the
    // box decides how much room it gets, and the painter elides the rest.
    if (style.overflow != TextOverflow::Wrap) return {natural, lineHeight, lineHeight * 0.78f};

    const WrappedText wrapped = wrapText(
        text, maxWidth, style.maxLines,
        [&](std::string_view run) { return static_cast<float>(countGlyphs(run)) * advance; },
        style.wordBreak);
    return {wrapped.widest, static_cast<float>(wrapped.lines.size()) * lineHeight,
            lineHeight * 0.78f};
}

void layout(Arena& arena, NodeId root, const Rect& viewport, const LayoutContext& context) {
    if (!root.valid() || arena.empty()) return;
    Engine engine(arena, context);
    engine.run(root, viewport);
}

float intrinsicMainSize(const Arena& arena, NodeId id, const LayoutContext& context) {
    if (!id.valid()) return 0.0f;
    // The engine only reads the arena on this path; the const_cast keeps the
    // public API honest about that rather than making callers hand over a
    // mutable tree to ask a question.
    Engine engine(const_cast<Arena&>(arena), context);
    return engine.intrinsicMain(id);
}

namespace {

/** Deepest hit within one layer, and the highest layer that was reached. */
struct Hit {
    NodeId node;
    Layer layer = Layer::Content;
};

Hit hitTestIn(const Arena& arena, NodeId root, Vec2 point, Layer inherited) {
    if (!root.valid()) return {};
    const Node& node = arena[root];
    const Layer layer = node.style.layer > inherited ? node.style.layer : inherited;

    // An overlay is not clipped by whatever built it, so a menu escaping a
    // panel is still hit outside that panel's frame.
    //
    // Which is why this cannot stop descending at a node that misses the point.
    // A popup is a *child* of whatever opened it — a select's list hangs under
    // the 34-pixel row holding the box — and it is placed outside that row on
    // purpose. Returning here because the row misses the pointer made every
    // such popup unhittable: no hover, no click, and a list that could only be
    // dismissed. The subtree is walked instead, and only nodes that really
    // contain the point can win.
    const bool ownFrame = node.frame.contains(point);
    Hit best = ownFrame ? Hit{root, layer} : Hit{};
    int bestZ = std::numeric_limits<int>::min();
    for (NodeId child = node.firstChild; child.valid(); child = arena[child].nextSibling) {
        if (clips(node.style.overflow) && !ownFrame &&
            arena[child].style.layer == layer) {
            continue;
        }
        const Hit deeper = hitTestIn(arena, child, point, layer);
        if (!deeper.node.valid()) continue;
        // A later layer always wins; within a layer, the last one *drawn* does —
        // and what is drawn last is decided by `zIndex` before tree order, so
        // the same rule has to be applied here or the pointer finds something
        // other than what it is pointing at.
        if (!best.node.valid() || deeper.layer > best.layer) {
            best = deeper;
            bestZ = arena[child].style.zIndex;
        } else if (deeper.layer == best.layer && arena[child].style.zIndex >= bestZ) {
            best = deeper;
            bestZ = arena[child].style.zIndex;
        }
    }
    return best;
}

}  // namespace

NodeId hitTest(const Arena& arena, NodeId root, Vec2 point) {
    return hitTestIn(arena, root, point, Layer::Content).node;
}

}  // namespace gbui
