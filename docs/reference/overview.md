# Reference

The public API, one page per module. Each page lists what the module holds, the
types it exports and the calls you are expected to make; the headers themselves
carry the finer detail and the reasoning.

| Module | Include | Holds |
| --- | --- | --- |
| [core](/reference/core) | `gbui/core/*.hpp` | geometry, lengths, colour, cursors, JSON, vector paths |
| [style](/reference/style) | `gbui/style/*.hpp` | style properties, themes and tokens, the design system |
| [scene](/reference/scene) | `gbui/scene/*.hpp` | the node arena and the building API |
| [layout](/reference/layout) | `gbui/layout/*.hpp` | flexbox, measurement, wrapping, hit testing |
| [input](/reference/input) | `gbui/input/*.hpp` | hover, press, focus, keys, text editing |
| [anim](/reference/anim) | `gbui/anim/*.hpp` | the animation clock and its curves |
| [overlay](/reference/overlay) | `gbui/overlay/*.hpp` | where a floating box goes |
| [paint](/reference/paint) | `gbui/paint/*.hpp` | display list, painters, the software rasteriser |
| [widgets](/reference/widgets) | `gbui/widgets/*.hpp` | components and icons |
| [charts](/reference/charts) | `gbui/widgets/chart.hpp` | scales, series, and nine kinds of chart |
| [platform](/reference/platform) | `gbui/platform/*.hpp` | window, event loop, fonts, image decoding, opening a URL |

A module may depend on the ones above it and never on the ones below, with the
two documented exceptions in
[Architecture → Modules](/guide/architecture#modules).

## Conventions across the API

- **Failure is `std::optional`**, with an error string where the reason matters.
  Nothing throws, and nothing in the public API is `noexcept` by accident.
- **Options are structs**, so call sites name what they set and defaults stay
  invisible. Designated initialisers must be written in declaration order.
- **`kAuto` is a NaN** meaning "decide for me". Test it with `isAuto(value)`.
- **Sizes are a `Length`**, which converts implicitly from `float`: a plain
  number is pixels, `Length::percent(25)` is a share.
- **Colours come from `Token`**, resolved against the active `Theme`.
- **Coordinates are logical pixels**, with the origin at the top left. The
  display scale is applied once, in `DisplayList::setScale`.
- **State belongs to the application.** A component takes the value and returns
  what the user did to it; nothing here remembers anything between frames except
  `Interaction` and `Animator`, both of which the application owns.
- **Identity is a string tag**, never a `NodeId`: the tree is rebuilt every
  frame and only the tag survives it.
