// Builds a tree, lays it out and paints it to SVG, using nothing but the
// installed headers and the `gbui::` namespace — no `using namespace`, so every
// name here is one the library actually exports.
//
// It draws nothing on screen on purpose: the point is the link, and a test that
// needs a display cannot run on a build machine.
#include <cstdio>

#include "gbui/layout/layout.hpp"
#include "gbui/paint/paint.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/style/theme.hpp"
#include "gbui/widgets/components.hpp"

int main() {
    gbui::Arena arena;
    gbui::Ui ui(arena);
    {
        auto column = ui.column({.gap = 8.0f,
                                      .padding = gbui::Edges::all(12.0f),
                                      .background = gbui::Fill{gbui::Token::Bg}});
        gbui::text(ui, "Linked against gbui", {.color = gbui::Token::TextStrong});
        gbui::button(ui, "COMMIT", {.variant = gbui::ButtonVariant::Primary, .block = true});
    }

    const gbui::Theme theme = gbui::Theme::dark();
    gbui::LayoutContext context;
    context.theme = &theme;
    gbui::layout(arena, ui.root(), gbui::Rect{0, 0, 320, 120}, context);

    gbui::DisplayList list;
    gbui::record(arena, ui.root(), theme, list);

    gbui::SvgPainter painter(320, 120, theme.color(gbui::Token::Bg));
    painter.paint(list);
    const std::string document = painter.finish();

    std::printf("%zu nodes, %zu draw commands, %zu bytes of SVG\n", arena.size(), list.size(),
                document.size());

    // A frame that produced nothing would still exit 0, and a link test that
    // cannot fail is not a test.
    return arena.size() > 0 && list.size() > 0 && !document.empty() ? 0 : 1;
}
