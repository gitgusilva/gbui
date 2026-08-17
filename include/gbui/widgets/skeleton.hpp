// The shape of content that has not arrived, in place of it.
//
// A spinner says "something is happening somewhere"; a skeleton says "a list of
// six rows is about to appear *here*, and it will be this tall". The second one
// stops the page jumping when the data lands, which is the actual reason to draw
// it — not the shimmer.
//
// **It is a lie with a short shelf life.** Drawing a skeleton for something that
// takes four seconds is worse than drawing a spinner, because the reader spends
// four seconds looking at a layout that is not there. Under about a second it
// reads as the content settling; past that, say what is happening instead.
#pragma once

#include <string_view>

#include "gbui/scene/ui.hpp"

namespace gbui {

/** What the placeholder is standing in for, which is all that changes. */
enum class SkeletonShape {
    /** A line of text. Rounded, and one line high unless `height` says. */
    Text,
    /** A block: a card, a thumbnail, a chart. */
    Block,
    /** A round one — an avatar. `width` is the diameter and `height` is
     *  ignored. */
    Circle,
};

struct SkeletonOptions {
    /** What it is standing in for: a line, a block, or an avatar. */
    SkeletonShape shape = SkeletonShape::Text;
    float width = kAuto;
    float height = 0.0f;
    float grow = 0.0f;
    float radius = 0.0f;
    /**
     * Where in its sweep the shimmer is, in turns. Feed it a clock.
     *
     * The caller's, like `spinner`'s and `progressBar`'s: a component here
     * holds no state. Leave it at zero for a still placeholder, which is the
     * honest choice when several are on screen — a dozen shimmers out of step
     * with each other is a page that looks broken rather than busy.
     */
    float phase = 0.0f;
    /**
     * What is loading, announced once for the group.
     *
     * Put it on the *first* skeleton of a set and leave the rest silent. A
     * reader told "loading" six times has been told nothing six times, which is
     * why every one of these is hidden from the tree unless it is named.
     */
    std::string_view name{};
};

/** A placeholder in the shape of what is coming. */
NodeId skeleton(Ui& ui, const SkeletonOptions& options = {});

}  // namespace gbui
