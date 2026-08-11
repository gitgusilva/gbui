// The icon set, generated from Lucide.
//
// Do not edit by hand: `tools/generate_icons.py` fetches the SVGs and writes
// this file. Circles, rects, lines and polylines are converted to path data at
// generation time, so the runtime parser only ever sees a `d` string.
//
// Icons are Lucide, ISC licensed — see assets/icons/LICENSE.
#pragma once

#include <optional>
#include <string_view>

namespace gbui {

/** Every icon the toolkit ships. Names follow Lucide's, in CamelCase. */
enum class Icon {
    Archive,
    Bold,
    ChartPie,
    Check,
    ChevronDown,
    ChevronLeft,
    ChevronRight,
    ChevronUp,
    CircleAlert,
    ClockFading,
    Download,
    Eye,
    EyeOff,
    File,
    FileMinus,
    FilePlus,
    Folder,
    GitBranch,
    GitCommitHorizontal,
    GitMerge,
    Heading,
    Image,
    Italic,
    Link,
    List,
    ListOrdered,
    Minus,
    Package,
    PanelLeft,
    Plus,
    Quote,
    RefreshCw,
    RotateCcw,
    Search,
    Settings,
    Strikethrough,
    Terminal,
    Underline,
    Upload,
    X,
    Count,
};

/** The path data for an icon, on Lucide's 24x24 grid. */
std::string_view iconPath(Icon icon);

/** The icon named as Lucide names it ("git-branch"), or nothing. */
std::optional<Icon> iconFromName(std::string_view name);

}  // namespace gbui
