#include "gbui_demos/demos.hpp"

#include <algorithm>

#include "registry.hpp"

namespace gbui::demos {

const std::vector<DemoInfo>& catalogue() {
    // Built once, on first use, and in the order the gallery reads best:
    // the familiar screen first, the two process screens together, the two
    // operational ones after them, and the market desk last — it is the one
    // whose chart is still moving while the reader looks at it.
    static const std::vector<DemoInfo> entries = {
        analyticsDemo(), weatherDemo(),    scadaDemo(), productionDemo(),
        gridDemo(),      logisticsDemo(), marketsDemo(),
    };
    return entries;
}

const DemoInfo* find(std::string_view id) {
    const auto& entries = catalogue();
    const auto match = std::find_if(entries.begin(), entries.end(),
                                    [&](const DemoInfo& entry) { return entry.id == id; });
    return match == entries.end() ? nullptr : &*match;
}

std::unique_ptr<Demo> create(std::string_view id) {
    const DemoInfo* entry = find(id);
    return entry && entry->create ? entry->create() : nullptr;
}

}  // namespace gbui::demos
