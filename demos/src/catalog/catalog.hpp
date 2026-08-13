// What the example files share: the public catalogue header, the widget
// umbrellas, and the one declaration each file publishes.
//
// Internal to `demos/src/catalog`, so it is a header here rather than one
// anybody includes.
#pragma once

#include <string>
#include <vector>

#include "gbui_demos/catalog.hpp"

namespace gbui::demos::catalog {

/** One per file, called in order by `registry.cpp`. Split by the same groups
 *  the metadata uses, so a reader looking for `slider` opens `controls.cpp`
 *  for the same reason they would open `gbui/widgets/controls.hpp`. */
void addComponentExamples(std::vector<Example>& out);
void addContainerExamples(std::vector<Example>& out);
void addControlExamples(std::vector<Example>& out);
void addOverlayExamples(std::vector<Example>& out);
void addChartExamples(std::vector<Example>& out);

}  // namespace gbui::demos::catalog
