// The widgets module's private helpers.
//
// Not installed and not part of the API: everything here is shared between two
// or more component files and nothing else. A helper used by exactly one
// component stays in that component's own file, where it is easier to read
// beside the thing it serves.
#pragma once

#include <string>
#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/overlay/placement.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/floating.hpp"

namespace gbui::detail {

// ---- controls --------------------------------------------------------------

/** Every control dims the same way, so the rule lives in one place. */
float opacityFor(bool disabled);

/** The ring that says where the keyboard is, as a control's own border. */
Border ringFor(bool ring, bool hovered);

/** A control is activated by clicking it or by pressing Space or Return while
 *  it has the keyboard. Both paths land here so no control invents its own. */
bool activated(const Interaction& input, std::string_view id, bool disabled);

/** Opens the row a control and its label share. */
Ui::Scope controlRow(Ui& ui, std::string_view id, bool disabled);

/** Space between a control and its label. */
void labelGap(Ui& ui);

/** Background and text colour for a field in a given state. */
struct FieldPalette {
    Fill background;
    Token label;
    Token border;
};

FieldPalette paletteForField(bool disabled, bool readOnly, bool hovered);

/** Surface, label and border for any disabled control that has a surface of its
 *  own — checkbox, radio, switch, select, button. Applied instead of the
 *  control's own colours, not on top of them. */
FieldPalette disabledPalette();

std::string formatNumber(double value, int decimals, std::string_view suffix);
double clampAndSnap(double value, double minimum, double maximum, double step);

// ---- text geometry ---------------------------------------------------------

/** One bullet per character, for a password field. */
std::string bulletsFor(std::string_view text);
/** How many characters — not bytes — come before `offset`. */
std::size_t countCharacters(std::string_view text, std::size_t offset);
/** The byte offset `count` characters into `text`. The inverse of the above. */
std::size_t characterAt(std::string_view text, std::size_t count);
/** Which character boundary a click at `x` lands on, measured from the start of
 *  the run. The caret's measurement, run the other way. */
std::size_t offsetAtX(const Ui& ui, std::string_view text, const TextStyle& style, float x);

// ---- floating surfaces -----------------------------------------------------

/** The window to keep a floating box inside. */
Rect boundsFor(const Interaction& input, const FloatingOptions& options);
PlacementOptions placementOptionsFrom(const FloatingOptions& options);

/** Opens an absolutely positioned surface in a layer above the content. */
Ui::Scope floating(Ui& ui, const Rect& rect, Layer layer, const Edges& padding,
                   float gapBetweenItems, Direction direction, float maxHeight = kAuto);

/** Rough size of a floating box before layout has run: the first frame
 *  estimates and the second corrects. */
Vec2 estimateSize(const Interaction& input, std::string_view id, Vec2 fallback);

/** An advance-width estimate, for the one frame before the real geometry. */
float measureTextWidth(std::string_view text, float fontSize);

}  // namespace gbui::detail
