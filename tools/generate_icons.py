#!/usr/bin/env python3
"""Generates the icon table from Lucide's SVGs.

    tools/generate_icons.py [icon-name ...]

Fetches each icon, normalises every shape to path data and writes
include/gbui/widgets/icons.hpp and src/widgets/icons.cpp. Circles, rects, lines
and polylines are converted here so the runtime parser only ever sees a `d`
string, and the C++ stays free of shape-specific code.

Icons are Lucide, ISC licensed — see assets/icons/LICENSE.
"""
import pathlib
import re
import sys
import urllib.request
import xml.etree.ElementTree as ET

LUCIDE = "https://raw.githubusercontent.com/lucide-icons/lucide/main/icons/{}.svg"
NS = "{http://www.w3.org/2000/svg}"
ROOT = pathlib.Path(__file__).resolve().parent.parent

# Every icon the toolkit ships, which is what a bare run regenerates. It has to
# be the whole set: the generator writes the file from this list alone, so a name
# missing from it is an icon deleted from the library by anyone who runs the
# script without arguments.
DEFAULT_ICONS = [
    "archive", "bold", "chart-pie", "check", "chevron-down", "chevron-left", "chevron-right",
    "chevron-up", "circle-alert", "clock-fading", "download", "eye", "eye-off", "file",
    "file-minus", "file-plus", "folder", "git-branch", "git-commit-horizontal", "git-merge",
    "heading", "image", "italic", "link", "list", "list-ordered", "minus", "package",
    "panel-left", "plus", "quote", "refresh-cw", "rotate-ccw", "search", "settings",
    "strikethrough", "terminal", "trending-down", "trending-up", "underline", "upload", "x",
]

# The constant that turns a quarter circle into a cubic.
KAPPA = 0.5522847498


def circle_to_path(cx, cy, r):
    k = r * KAPPA
    return (f"M{cx - r},{cy} C{cx - r},{cy - k} {cx - k},{cy - r} {cx},{cy - r} "
            f"C{cx + k},{cy - r} {cx + r},{cy - k} {cx + r},{cy} "
            f"C{cx + r},{cy + k} {cx + k},{cy + r} {cx},{cy + r} "
            f"C{cx - k},{cy + r} {cx - r},{cy + k} {cx - r},{cy} Z")


def rect_to_path(x, y, w, h, rx):
    if not rx:
        return f"M{x},{y} L{x + w},{y} L{x + w},{y + h} L{x},{y + h} Z"
    k = rx * KAPPA
    return (f"M{x + rx},{y} L{x + w - rx},{y} C{x + w - rx + k},{y} {x + w},{y + rx - k} "
            f"{x + w},{y + rx} L{x + w},{y + h - rx} C{x + w},{y + h - rx + k} "
            f"{x + w - rx + k},{y + h} {x + w - rx},{y + h} L{x + rx},{y + h} "
            f"C{x + rx - k},{y + h} {x},{y + h - rx + k} {x},{y + h - rx} L{x},{y + rx} "
            f"C{x},{y + rx - k} {x + rx - k},{y} {x + rx},{y} Z")


def absolute_start(d):
    """Makes a path's opening moveto absolute without changing what follows.

    SVG treats the first moveto of a path as absolute even when written `m`.
    That matters here because several <path> elements are concatenated into one
    string: left alone, the second path's `m` would be read as relative to where
    the first one ended.

    Simply uppercasing it is not enough, and that mistake shipped once. A
    lowercase moveto implies *relative* linetos for the pairs that follow it, so
    `m4 17 6-6-6-6` has to become `M4 17 l6-6-6-6` — uppercasing alone turned
    those into absolute coordinates and drew the terminal icon at (-6, -6).
    """
    text = d.strip()
    if not text.startswith("m"):
        return text
    match = re.match(r"m\s*(-?[\d.]+)[,\s]+(-?[\d.]+)\s*(.*)", text, re.S)
    if not match:
        return "M" + text[1:]
    x, y, rest = match.groups()
    rest = rest.strip()
    if not rest:
        return f"M{x},{y}"
    # A command letter speaks for itself; bare numbers are the implicit
    # relative lineto the lowercase moveto promised.
    return f"M{x},{y} {rest}" if rest[0].isalpha() else f"M{x},{y} l{rest}"


def number(element, name, default=0.0):
    return float(element.get(name, default))


def svg_to_path(svg_text, name):
    root = ET.fromstring(svg_text)
    parts = []
    for element in root:
        tag = element.tag.replace(NS, "")
        if tag == "path":
            parts.append(absolute_start(element.get("d")))
        elif tag == "circle":
            parts.append(circle_to_path(number(element, "cx"), number(element, "cy"),
                                        number(element, "r")))
        elif tag == "ellipse":
            parts.append(circle_to_path(number(element, "cx"), number(element, "cy"),
                                        number(element, "rx")))
        elif tag == "rect":
            parts.append(rect_to_path(number(element, "x"), number(element, "y"),
                                      number(element, "width"), number(element, "height"),
                                      number(element, "rx")))
        elif tag == "line":
            parts.append(f"M{number(element, 'x1')},{number(element, 'y1')} "
                         f"L{number(element, 'x2')},{number(element, 'y2')}")
        elif tag in ("polyline", "polygon"):
            points = [p for p in re.split(r"[,\s]+", element.get("points").strip()) if p]
            pairs = list(zip(points[0::2], points[1::2]))
            data = "M" + " L".join(f"{x},{y}" for x, y in pairs)
            parts.append(data + " Z" if tag == "polygon" else data)
        else:
            raise SystemExit(f"{name}: unhandled <{tag}>")
    return " ".join(parts)


def enum_name(slug):
    return "".join(part.capitalize() for part in slug.replace("_", "-").split("-"))


def main():
    names = sorted(sys.argv[1:] or DEFAULT_ICONS)
    icons = {}
    for name in names:
        with urllib.request.urlopen(LUCIDE.format(name), timeout=30) as response:
            icons[name] = svg_to_path(response.read().decode(), name)

    header = ['// The icon set, generated from Lucide.',
              '//',
              '// Do not edit by hand: `tools/generate_icons.py` fetches the SVGs and writes',
              '// this file. Circles, rects, lines and polylines are converted to path data at',
              '// generation time, so the runtime parser only ever sees a `d` string.',
              '//',
              '// Icons are Lucide, ISC licensed — see assets/icons/LICENSE.',
              '#pragma once',
              '',
              '#include <optional>',
              '#include <string_view>',
              '',
              'namespace gbui {',
              '',
              "/** Every icon the toolkit ships. Names follow Lucide's, in CamelCase. */",
              'enum class Icon {']
    header += [f"    {enum_name(name)}," for name in names]
    header += ['    Count,',
               '};',
               '',
               "/** The path data for an icon, on Lucide's 24x24 grid. */",
               'std::string_view iconPath(Icon icon);',
               '',
               '/** The icon named as Lucide names it ("git-branch"), or nothing. */',
               'std::optional<Icon> iconFromName(std::string_view name);',
               '',
               '}  // namespace gbui',
               '']
    (ROOT / "include/gbui/widgets/icons.hpp").write_text("\n".join(header))

    source = ['#include "gbui/widgets/icons.hpp"',
              '',
              '#include <array>',
              '',
              'namespace gbui {',
              'namespace {',
              '',
              'struct Entry {',
              '    std::string_view name;',
              '    std::string_view path;',
              '};',
              '',
              '// Generated from the Lucide icon set (ISC). Index-aligned with Icon.',
              'constexpr std::array<Entry, static_cast<std::size_t>(Icon::Count)> kIcons{{']
    source += [f'    {{"{name}", "{icons[name]}"}},' for name in names]
    source += ['}};',
               '',
               '}  // namespace',
               '',
               'std::string_view iconPath(Icon icon) {',
               '    return kIcons[static_cast<std::size_t>(icon)].path;',
               '}',
               '',
               'std::optional<Icon> iconFromName(std::string_view name) {',
               '    for (std::size_t i = 0; i < kIcons.size(); ++i) {',
               '        if (kIcons[i].name == name) return static_cast<Icon>(i);',
               '    }',
               '    return std::nullopt;',
               '}',
               '',
               '}  // namespace gbui',
               '']
    (ROOT / "src/widgets/icons.cpp").write_text("\n".join(source))
    print(f"{len(names)} icons written")


if __name__ == "__main__":
    main()
