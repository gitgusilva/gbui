#!/usr/bin/env python3
"""Draws the pipeline diagram — build → layout → paint → backend.

It was ASCII art, and ASCII art is a promise about character widths that no
renderer keeps: the arrow and the box-drawing characters come from whichever
fallback font has them, at whichever width that font gives them, and the
columns stop lining up. A drawing is a drawing.

Two files come out, one per theme, because no single colour is readable on both
a white and a near-black background — 4.5:1 against one of them costs you the
other. They are generated from this one description so they cannot drift.

    tools/generate_diagram.py        # writes docs/public/pipeline-{light,dark}.svg
"""

from pathlib import Path

STAGES = [
    ("scene", "Arena + Ui", "build"),
    ("layout", "flexbox", "frames"),
    ("paint", "DisplayList", "commands"),
    ("backend", "Canvas · SVG · yours", "pixels"),
]

# Geometry, in user units. The viewBox is the only thing a consumer sees, so the
# drawing scales to whatever width it is given.
PAD, BOX_W, BOX_H, GAP, TOP = 8, 192, 104, 36, 28
WIDTH = PAD * 2 + BOX_W * len(STAGES) + GAP * (len(STAGES) - 1)
HEIGHT = TOP * 2 + BOX_H

THEMES = {
    "light": {
        "surface": "#f6f8fa",
        "border": "#d0d7de",
        "module": "#0969da",
        "text": "#1f2328",
        "muted": "#6e7781",
    },
    "dark": {
        "surface": "#161b22",
        "border": "#30363d",
        "module": "#58a6ff",
        "text": "#e6edf3",
        "muted": "#8b949e",
    },
}

SANS = "ui-sans-serif,-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif"
MONO = "ui-monospace,SFMono-Regular,Menlo,Consolas,'Liberation Mono',monospace"


def render(colors: dict[str, str]) -> str:
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {WIDTH} {HEIGHT}" '
        f'width="{WIDTH}" height="{HEIGHT}" role="img" '
        f'aria-label="The pipeline: scene builds a tree, layout writes frames, '
        f'paint records a display list, a backend draws it.">',
        "<defs>",
        f'<marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" '
        f'markerHeight="6" orient="auto-start-reverse">'
        f'<path d="M0,0 L10,5 L0,10 z" fill="{colors["muted"]}"/></marker>',
        "</defs>",
    ]

    for index, (module, subject, phase) in enumerate(STAGES):
        x = PAD + index * (BOX_W + GAP)
        centre = x + BOX_W / 2
        parts += [
            f'<rect x="{x}" y="{TOP}" width="{BOX_W}" height="{BOX_H}" rx="10" '
            f'fill="{colors["surface"]}" stroke="{colors["border"]}"/>',
            f'<text x="{centre}" y="{TOP + 32}" text-anchor="middle" font-family="{MONO}" '
            f'font-size="14" font-weight="600" fill="{colors["module"]}">{module}</text>',
            f'<text x="{centre}" y="{TOP + 60}" text-anchor="middle" font-family="{SANS}" '
            f'font-size="14" fill="{colors["text"]}">{subject}</text>',
            f'<text x="{centre}" y="{TOP + 84}" text-anchor="middle" font-family="{SANS}" '
            f'font-size="12" fill="{colors["muted"]}">{phase}</text>',
        ]

        if index + 1 < len(STAGES):
            start, end = x + BOX_W + 6, x + BOX_W + GAP - 6
            parts.append(
                f'<line x1="{start}" y1="{TOP + BOX_H / 2}" x2="{end}" y2="{TOP + BOX_H / 2}" '
                f'stroke="{colors["muted"]}" stroke-width="1.5" marker-end="url(#arrow)"/>'
            )

    parts.append("</svg>")
    return "\n".join(parts) + "\n"


def main() -> None:
    out = Path(__file__).resolve().parent.parent / "docs" / "public"
    out.mkdir(parents=True, exist_ok=True)
    for name, colors in THEMES.items():
        path = out / f"pipeline-{name}.svg"
        path.write_text(render(colors), encoding="utf-8")
        print(f"wrote {path.relative_to(path.parents[3])}")


if __name__ == "__main__":
    main()
