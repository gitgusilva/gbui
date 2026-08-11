# Theming

A theme is 24 semantic colour tokens plus typography, loaded from a JSON file.
Components name tokens; they never name colours. That is the whole contract, and
it is what lets an application re-theme without being rebuilt.

## The tokens

| Group | Tokens |
| --- | --- |
| Surfaces | `bg`, `bgElevated`, `bgOverlay`, `surfaceHover` |
| Lines | `border`, `borderStrong` |
| Text | `textStrong`, `text`, `textMuted` |
| Accent | `accent`, `accentHover`, `accentFg` |
| Diff | `added`, `removed`, `modified` |
| Graph | `graph1` … `graph8`, `graphMarker` |

Typography carries `uiFont`, `uiFontSize`, `monoFont`, `editorFont`,
`editorFontSize`, `editorLineHeight` and `radius`.

## Using them

```cpp
text(ui, "main", {.color = Token::TextStrong});           // yes
ui.begin({.background = Fill{Token::Accent, 0.18f}});     // a token at an alpha

TextStyle wrong;
wrong.color = Fill{Color{242, 242, 242}};                 // no
```

A `Fill` is a token, or a literal colour, or a token at an alpha. Reaching for
the literal is occasionally right — a brand mark, a traffic light — and is
always worth a comment saying why the theme does not decide it.

A gradient is a list of stops, and each stop carries a `Fill` rather than a
colour, so a gradient is themeable like everything else:

```cpp
ui.begin({.backgroundGradient = Gradient::linear(Fill{Token::Accent},
                                                 Fill{Token::Accent, 0.0f})});
```

`angle` follows CSS — 0 points up, 90 points right — and `Gradient::radial` runs
from the centre outwards. Text takes one too, through `TextStyle::colorGradient`.

## Loading a theme

```cpp
std::string error;
const auto theme = Theme::fromFile("themes/nord/theme.json", &error);
if (!theme) {
    std::fprintf(stderr, "%s\n", error.c_str());   // "colors.accentHover is missing"
    return;
}
```

The file format is the one the
[gitbox-themes](https://github.com/gitgusilva/gitbox-themes) registry publishes,
so a theme written for the web application works here unchanged:

```json
{
  "id": "nord",
  "name": "Nord",
  "type": "dark",
  "colors": { "bg": "#2E3440", "accent": "#88C0D0", "...": "..." },
  "typography": { "uiFont": "IBM Plex Sans", "uiFontSize": 13, "radius": 6 }
}
```

All 24 colours are required. A theme missing one fails to load and says which,
because the person who can fix it is the person who wrote the file.

`Theme::dark()` and `Theme::light()` are built in, so you can draw before any
file is read. So are `Theme::material()`, `Theme::cupertino()` and
`Theme::fluent()`, each in a light and a dark form — the published palettes of
those three systems, mapped onto these 24 tokens. They are approximations by
necessity, since each of those systems has a hundred-odd roles and this has
twenty-four, and they exist to prove the toolkit re-themes convincingly rather
than to certify anything as compliant.

## Three helpers worth knowing

```cpp
theme.graphLane(index);       // lane colour for the commit graph, wrapping at eight
theme.onAccent();             // a foreground that stays legible on the accent fill
theme.focusRing(surface);     // a ring that stands out from what it is drawn over
```

`onAccent` prefers the theme's own `accentFg` and only overrides it when the
pair fails WCAG contrast. This is not hypothetical: of the eleven themes in the
registry, Solarized Dark and Solarized Light both declare an `accentFg` that
fails against their accent, and without the check their primary button is
unreadable.

`focusRing` answers the same kind of question one level up. A ring in the accent
around a control *filled* with the accent is invisible — and that is precisely
the control a keyboard user is most likely to be on — so the ring is the accent
when it clears 3:1 against the surface behind it and the strongest text colour
when it does not.

## Beyond colour: the Design

A theme is 24 colours and a type scale. That is not a design system. Material,
Cupertino and Fluent differ far more in **shape and behaviour** than in palette:
how round a control is, how tall, whether a press throws ink from the point you
touched, how fast anything moves and on what curve. Repainting Material's
palette onto square controls with no press feedback does not produce Material;
it produces the same toolkit in purple.

So those decisions live in a `Design`, handed to the builder beside the theme:

```cpp
ui.setDesign(Design::material());     // or cupertino(), fluent(), gitbox()
```

| Carries | Why it is not a per-call-site option |
| --- | --- |
| `controlRadius`, `controlHeight`, `borderWidth` | Every control has to agree, or nothing lines up |
| `switchWidth/Height/Knob`, `checkboxSize`, `radioSize` | The switch is what a person recognises a design system by |
| `press`, `rippleAlpha`, `hoverAlpha` | Whether a press throws ink is a system's decision, not a screen's |
| `motion` | One `Transition`, so switching design changes the *feel* too |
| `chart` | Line weight, tick count, donut thickness and the series palette |

A component asks the active design what a press looks like rather than taking a
`ripple` flag from whoever wrote the screen. That is the difference between a
design system and a pile of options.

The design is handed to the *builder* rather than read at paint time because
these decisions change the tree: a control's height and radius are laid out, and
whether a press throws ink decides whether a node exists at all.

## Seeing every theme at once

```sh
./build/examples/gbui_gallery out/ path/to/gitbox-themes/themes
```

One screen, rendered in every theme it finds, at four widths, with an
`index.html` to flip through them. Nothing in that screen is theme-aware — only
the `Theme` handed to `layout` and `record` changes — which makes the sweep a
useful review of both the themes and the components.
