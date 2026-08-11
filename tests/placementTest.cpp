#include "gbui/overlay/placement.hpp"

#include "harness.hpp"

using namespace gbui;

namespace {

/** A window with room on every side of a small anchor in the middle. */
const Rect kWindow{0, 0, 1000, 800};
const Rect kCentre{480, 380, 40, 20};

}  // namespace

TEST("a preferred side is used when it fits") {
    const auto below = place(kCentre, {200, 100}, kWindow, {.preferred = Placement::Bottom});
    CHECK_NEAR(below.rect.y, kCentre.bottom() + 6.0f);
    CHECK(!below.flipped);
    // Centred on the anchor, which is what an unaligned placement means.
    CHECK_NEAR(below.rect.x, kCentre.x + (kCentre.width - 200.0f) / 2.0f);

    const auto above = place(kCentre, {200, 100}, kWindow, {.preferred = Placement::Top});
    CHECK_NEAR(above.rect.bottom(), kCentre.y - 6.0f);

    const auto right = place(kCentre, {200, 100}, kWindow, {.preferred = Placement::Right});
    CHECK_NEAR(right.rect.x, kCentre.right() + 6.0f);
}

TEST("alignment pins the box to an edge of the anchor") {
    const auto start = place(kCentre, {200, 100}, kWindow, {.preferred = Placement::BottomStart});
    CHECK_NEAR(start.rect.x, kCentre.x);

    const auto end = place(kCentre, {200, 100}, kWindow, {.preferred = Placement::BottomEnd});
    CHECK_NEAR(end.rect.right(), kCentre.right());
}

TEST("a box that does not fit below is flipped above") {
    // An anchor near the bottom edge: below there is no room, above there is.
    const Rect low{480, 720, 40, 20};
    const auto placed = place(low, {200, 150}, kWindow, {.preferred = Placement::Bottom});

    CHECK(placed.flipped);
    CHECK(sideOf(placed.placement) == Placement::Top);
    CHECK_NEAR(placed.rect.bottom(), low.y - 6.0f);
}

TEST("flipping is refused when the other side is no better") {
    // Taller than the window: neither side fits, so it stays where it was asked
    // to go rather than jumping to an equally bad place.
    const auto placed = place(kCentre, {200, 900}, kWindow, {.preferred = Placement::Bottom});
    CHECK(!placed.flipped);
    CHECK(sideOf(placed.placement) == Placement::Bottom);
}

TEST("shift slides along the cross axis to stay inside") {
    // Anchored at the left edge, a wide box would hang off it.
    const Rect edge{10, 380, 40, 20};
    const auto placed = place(edge, {300, 100}, kWindow, {.preferred = Placement::Bottom});

    CHECK(placed.rect.x >= kWindow.x + 8.0f);
    CHECK_NEAR(placed.rect.x, 8.0f);
    // Shifting must not detach it from the anchor: the side axis is untouched.
    CHECK_NEAR(placed.rect.y, edge.bottom() + 6.0f);
}

TEST("shift can be turned off") {
    const Rect edge{10, 380, 40, 20};
    const auto placed =
        place(edge, {300, 100}, kWindow, {.preferred = Placement::Bottom, .shift = false});
    CHECK(placed.rect.x < 0.0f);
}

TEST("auto picks the side with room and prefers below on a tie") {
    // Room everywhere: below wins, because that is where a menu is expected.
    const auto centred = place(kCentre, {100, 100}, kWindow, {.preferred = Placement::Auto});
    CHECK(sideOf(centred.placement) == Placement::Bottom);

    // Pinned to the bottom edge, with room above and to the right.
    const Rect low{480, 770, 40, 20};
    const auto raised = place(low, {100, 300}, kWindow, {.preferred = Placement::Auto});
    CHECK(sideOf(raised.placement) != Placement::Bottom);
}

TEST("the gap and the margin are respected") {
    const auto placed = place(kCentre, {100, 60}, kWindow,
                              {.preferred = Placement::Bottom, .gap = 20.0f, .margin = 32.0f});
    CHECK_NEAR(placed.rect.y, kCentre.bottom() + 20.0f);

    const Rect edge{0, 380, 40, 20};
    const auto shifted = place(edge, {300, 60}, kWindow,
                               {.preferred = Placement::Bottom, .margin = 32.0f});
    CHECK_NEAR(shifted.rect.x, 32.0f);
}

TEST("placement names round-trip") {
    CHECK(placementFromName("bottom-start") == std::optional<Placement>(Placement::BottomStart));
    CHECK(placementFromName("left") == std::optional<Placement>(Placement::Left));
    CHECK(!placementFromName("sideways").has_value());
    CHECK_EQ(placementName(Placement::TopEnd), std::string_view("top-end"));

    for (const Placement placement :
         {Placement::Auto, Placement::Top, Placement::TopStart, Placement::TopEnd,
          Placement::Bottom, Placement::BottomStart, Placement::BottomEnd, Placement::Left,
          Placement::LeftStart, Placement::LeftEnd, Placement::Right, Placement::RightStart,
          Placement::RightEnd}) {
        CHECK(placementFromName(placementName(placement)) == std::optional<Placement>(placement));
    }
}

TEST("an alignment survives a flip") {
    const Rect low{480, 720, 40, 20};
    const auto placed = place(low, {200, 150}, kWindow, {.preferred = Placement::BottomStart});
    CHECK(placed.flipped);
    // Flipped to the top, still aligned to the anchor's left edge.
    CHECK(placed.placement == Placement::TopStart);
    CHECK_NEAR(placed.rect.x, low.x);
}
