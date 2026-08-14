#include "gbui/widgets/image.hpp"

#include "gbui/widgets/text.hpp"

namespace gbui {

NodeId image(Ui& ui, const Bitmap& source, const ImageOptions& options) {
    Style style;
    // Its own size when it was given none, which is what `<img>` with no
    // attributes does: a picture has a size, and a box that ignored it would
    // make every call site state one twice.
    style.width = !options.width.isAuto() ? options.width
                  : source.valid()        ? Length{static_cast<float>(source.width)}
                                          : Length{};
    style.height = !options.height.isAuto() ? options.height
                   : source.valid()         ? Length{static_cast<float>(source.height)}
                                            : Length{};
    style.grow = options.grow;
    style.shrink = options.shrink;
    style.padding = options.padding;
    style.radius = options.radius;
    if (options.background) style.background = *options.background;
    // Clipped, because `Cover` and `None` are the two fits that reach outside
    // the box on purpose and a picture that painted over its neighbours would
    // be a bug in every one of them.
    style.overflow = Overflow::Hidden;

    if (source.valid()) {
        // The picture is a *child* of the box, not the box itself.
        //
        // A node's clip applies to what it contains, not to what it draws — the
        // same rule a background follows, and the right one. `Cover` and `None`
        // reach outside the box on purpose, so drawn on the clipping node they
        // were drawn outside their own clip and painted over the neighbours.
        // One level down they are cut by it.
        auto scope = ui.scope(style);
        Style inner;
        inner.position = Position::Absolute;
        inner.left = 0.0f;
        inner.top = 0.0f;
        inner.width = Length::percent(100);
        inner.height = Length::percent(100);
        ui.picture(source, inner, options.fit, options.opacity);
        return scope.id();
    }

    // Nothing to draw. The alt text goes in the same box, centred, so a row of
    // logos with one missing keeps its shape instead of closing up.
    style.align = Align::Center;
    style.justify = Justify::Center;
    auto scope = ui.scope(style);
    if (!options.alt.empty()) {
        text(ui, options.alt,
             {.color = Token::TextMuted,
              .weight = FontWeight::SemiBold,
              .size = 11.0f,
              .align = TextAlign::Center,
              .overflow = TextOverflow::Ellipsis});
    }
    return scope.id();
}

}  // namespace gbui
