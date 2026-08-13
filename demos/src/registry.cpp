#include "gbui_demos/demos.hpp"

#include <algorithm>

#include "registry.hpp"

namespace gbui::demos {

const std::vector<DemoInfo>& catalogue() {
    // Built once, on first use, and in the order the gallery reads best: the
    // market desk first, because it is the one still moving while the reader
    // looks at it; then the familiar dashboard, the two process screens
    // together, and the two operational ones after them.
    static const std::vector<DemoInfo> entries = {
        marketsDemo(), analyticsDemo(), weatherDemo(),
        scadaDemo(),   productionDemo(), gridDemo(),
        logisticsDemo(),
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
