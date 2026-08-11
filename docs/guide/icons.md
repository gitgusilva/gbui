# Icons

The toolkit ships 40 [Lucide](https://lucide.dev) glyphs, drawn as vector
geometry rather than bitmaps, so they stay sharp at any size and take their
colour from the theme.

```cpp
icon(ui, Icon::GitBranch, {.color = Token::Accent, .size = 16.0f});

button(ui, "FETCH", {.variant = ButtonVariant::Ghost, .leading = Icon::RefreshCw});
```

| Option | Default | Meaning |
| --- | --- | --- |
| `color` | `Token::Text` | Any theme token |
| `size` | `16.0f` | Side of the square the glyph is drawn in |
| `stroke` | `2.0f` | Stroke width on Lucide's 24-unit grid, scaled with the icon |

An icon node has `shrink = 0`: its size is its meaning, and a crowded row must
elide something else.

## How it works

Icon data is a string of SVG path commands on a 24×24 grid.
[`parseSvgPath`](/reference/core#path) flattens it to polylines, `record`
scales it into the node's content box, and the painter strokes it. Both
backends handle it — the SVG writer emits a `<path>`, the software rasteriser
computes coverage from the distance to the geometry.

Nothing about this is icon-specific. The same `DrawPath` command is what the
charts and the commit graph are drawn with — through `Ui::draw`, which takes a
list of `Shape`s in the node's own coordinates.

## Adding icons

```sh
tools/generate_icons.py git-pull-request folder-open tag
```

The script fetches those icons from Lucide, normalises every shape — circles,
rects, lines, polylines — to path data, and regenerates
`include/gbui/widgets/icons.hpp` and `src/widgets/icons.cpp`. Run it with no
arguments to rebuild the default set.

Do not edit the generated files by hand.

## Using your own artwork

Any SVG path data works, not just the generated table:

```cpp
ui.vector("M4 12 L12 4 L20 12", style, Fill{Token::Accent}, 2.0f);
```

The parser handles `M L H V C S Q T A Z` in both cases. Arcs matter more than
they look: Lucide spells most of its rounded corners as `a`, and this library
shipped once without arc support and drew half its icons in pieces.

## Licence

The icon data is Lucide, ISC licensed. The text of that licence is in
`assets/icons/LICENSE` and must ship with any binary that includes the table.
