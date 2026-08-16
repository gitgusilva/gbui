#!/usr/bin/env python3
"""Generates the component metadata table from the widget headers.

    tools/generate_meta.py [--check]

Reads include/gbui/widgets/*.hpp and writes src/meta/components.cpp: one entry
per component function, with its group, its documentation, and every member of
its options struct with that member's type, default and doc comment.

Five things read that table — the designer's properties panel, the code
generator, the Qt importer, the documentation's gallery and a conformance test
— and a table written by hand rots on the first new option. See
include/gbui/meta/components.hpp.

**It is not a C++ parser and must not become one.** It understands the shape
these headers are actually written in: an options struct is an aggregate of
plain members with default member initialisers and a doc comment above each,
and a component is a free function taking `Ui&` and returning `NodeId` or
`Ui::Scope`. A header it cannot read is an error naming that header, never a
silent omission — which is the only property that makes a generated table
trustworthy.

`--check` regenerates into memory and exits non-zero if the file on disk
differs, which is what CI runs.
"""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
WIDGETS = ROOT / "include" / "gbui" / "widgets"
OUT = ROOT / "src" / "meta" / "components.cpp"
# The same table as JSON, for the documentation site.
#
# Emitted from here rather than parsed again in TypeScript: two parsers over
# the same headers is two things to keep in step, and the second one would be
# discovered to have drifted by a reader rather than by a build.
JSON_OUT = ROOT / "docs" / ".vitepress" / "theme" / "components.json"

# The umbrella headers decide the groups, so the grouping in the documentation
# and the grouping a developer already sees in the includes are the same one.
#
# `controls.hpp` is deliberately absent. It still exists and still compiles, but
# it now includes the two umbrellas that replaced it, and listing it here would
# make it claim every file they hold — first umbrella wins below — putting the
# whole set back under one word. A compatibility header contributes no group.
UMBRELLAS = {
    "elements.hpp": "Elements",
    "components.hpp": "Components",
    "containers.hpp": "Containers",
    "overlays.hpp": "Overlays",
}
# Two headers an umbrella does not decide for.
#
# `chart.hpp` comes in through `components.hpp` for the convenience of a caller
# who wants everything, but nine unrelated charts are a group by any reader's
# reckoning. `icons.hpp` is generated and has no umbrella at all.
EXPLICIT_GROUPS = {"chart.hpp": "Charts", "icons.hpp": "Elements"}
# The reading order, which is also the order the four questions at the top of
# `elements.hpp` are asked in: the leaves first, then the things that hold them,
# then the things that float over them, then the composed editors. "Other" is
# last and should stay empty — a component nothing gathers is a missing include
# in an umbrella, not a category.
GROUP_ORDER = ["Elements", "Containers", "Overlays", "Components", "Charts", "Other"]

# ---- the shapes this understands -------------------------------------------

# A component declaration at namespace scope: any return type, a name, and an
# argument list containing `Ui&`. Anchored to column zero, which is what
# separates a free function in these headers from a method inside a class —
# every declaration this table is about is unindented, and every one it is not
# is indented. A cheap discriminator, and a correct one for this tree.
#
# `[[nodiscard]]` appears on the ones returning a value the caller must not
# drop, and the return type is wide because a component reports what the user
# did with it: `bool`, `SliderResult`, `std::optional<std::size_t>`, `void`.
FUNCTION = re.compile(
    r"^(?:\[\[nodiscard\]\]\s*)?"
    r"(?P<returns>[\w:]+(?:\s*<[^;()]*?>)?)\s+"
    # Braces are allowed inside the arguments and cannot be excluded: every
    # options parameter here ends `= {}`, and a pattern that refused them
    # matched only the handful of components that take none.
    r"(?P<name>\w+)\s*\("
    r"(?P<args>[^;]*?)\)\s*;",
    re.M | re.S,
)

STRUCT = re.compile(r"struct\s+(\w+)\s*(?::\s*[\w:]+\s*)?\{(.*?)\n\};", re.S)
ENUM = re.compile(r"enum\s+class\s+(\w+)\s*(?::\s*\w+\s*)?\{(.*?)\}\s*;", re.S)

# One member: an optional doc comment, then `Type name = default;` / `Type
# name{};` / `Type name;`.
MEMBER = re.compile(
    r"(?P<doc>(?:^[ \t]*(?:/\*\*.*?\*/|///<?[^\n]*)\n)+)?"
    r"^[ \t]*(?!return|static|using|friend|constexpr)"
    r"(?P<type>[\w:]+(?:\s*<[^;{}]*?>)?(?:\s*\*|\s*&)?)\s+"
    r"(?P<name>\w+)\s*"
    r"(?:=\s*(?P<default>[^;]+?)|\{(?P<braced>[^;]*?)\})?\s*;"
    r"(?P<trailing>[ \t]*///<[^\n]*)?",
    re.M | re.S,
)

# ---- the house style, enforced ----------------------------------------------
#
# Two rules, and they are here rather than in a linter because this script is
# already the thing that reads every header and already fails the build when it
# cannot. Both exist because the *output* depends on them: the first paragraph
# of a header becomes the gallery card, and a member's doc comment becomes its
# row in the properties table.
#
# See CONTRIBUTING for the style in full.

# How long an opening paragraph may be before it has stopped being a summary.
#
# The failure this catches is specific: `generate_meta` harvests the first
# paragraph and puts it on the card, so a header that opens with a design
# argument ships that argument as its summary. A blank comment line ends the
# paragraph, which is all a long preamble has to do to comply.
MAX_SUMMARY = 240

# The words this library uses with one meaning everywhere, which therefore need
# no comment in the forty-odd options structs that repeat them.
#
# Documenting `float width` once per struct is forty copies to keep in step, and
# the house style says a comment that restates the code is deleted rather than
# improved. Anything *not* on this list is component-specific and has to say
# what it is — which is the whole of the rule.
SHARED_NAMES = {
    # layout, in CSS's own vocabulary
    "direction", "justify", "align", "gap", "padding", "margin", "overflow",
    "width", "height", "minWidth", "maxWidth", "minHeight", "maxHeight",
    "grow", "shrink", "basis", "layer",
    # appearance
    "background", "backgroundGradient", "border", "borderWidth", "radius",
    "opacity", "color", "size", "weight", "slant", "underline", "strikeThrough",
    "cursor",
    # identity and state
    "id", "name", "label", "role", "disabled", "readOnly", "focusable",
    "placeholder", "leading",
    # numbers and ranges
    "minimum", "maximum", "step", "scale", "autoScale", "tickCount",
    "valueFormat",
    # things three or more components already agree about
    "axisWidth", "categories", "grid", "hover", "legend", "link", "tooltip",
    "rowHeight", "dismissOnEscape", "dismissOnOutsideClick",
}

# A compound of a shared *dimension* is shared too: `cellPadding` is as
# self-evident as `padding`, and `scrollbarWidth` as `width`. Only dimensions,
# deliberately — `secondStep` ends in a shared word and does not mean "the
# second step", which is exactly the kind of name that needs a sentence.
SHARED_SUFFIXES = {"width", "height", "padding", "margin", "gap", "radius",
                   "color", "opacity", "size"}


def needs_doc(name):
    """Whether this member has to carry a doc comment."""
    if name in SHARED_NAMES:
        return False
    words = re.findall(r"[A-Z]?[a-z0-9]+", name)
    return not (words and words[-1].lower() in SHARED_SUFFIXES)


KINDS = [
    ("bool", "Bool"),
    ("float", "Number"),
    ("double", "Number"),
    ("int", "Number"),
    ("std::size_t", "Number"),
    ("Length", "Length"),
    ("std::string_view", "Text"),
    ("std::string", "Text"),
    ("Token", "Token"),
    ("Icon", "Icon"),
    ("Edges", "Edges"),
]


def clean_doc(raw):
    """A doc comment as one line, without its markers."""
    if not raw:
        return ""
    text = raw.strip()
    text = re.sub(r"^/\*\*|\*/$", " ", text)
    lines = []
    for line in text.splitlines():
        line = line.strip()
        line = re.sub(r"^///<?", "", line)
        line = re.sub(r"^\*\s?", "", line)
        lines.append(line.strip())
    return re.sub(r"\s+", " ", " ".join(lines)).strip()


def kind_of(type_text, enums):
    """The coarse editor kind for a C++ type."""
    inner = type_text
    match = re.match(r"std::optional<(.+)>$", inner.strip())
    if match:
        inner = match.group(1).strip()
    inner = inner.strip()
    # `Icon` is an enumeration and gets its own kind: an editor picks one from a
    # grid of glyphs, not from a menu of forty names.
    for prefix, kind in KINDS:
        if inner == prefix:
            return kind, inner
    if inner in enums:
        return "Enum", inner
    return "Opaque", inner


def header_doc(source):
    """The first paragraph of the `//` block a header opens with.

    Every file in `widgets/` starts with one sentence saying what the thing is,
    and it is almost always a better summary than any single overload's doc —
    which is per declaration and often explains a distinction rather than the
    component. Only the first paragraph: the rest of these preambles argue
    about design, which belongs in the header and not in a gallery card.
    """
    lines = []
    for line in source.splitlines():
        stripped = line.strip()
        if stripped.startswith("#"):
            break
        if not stripped.startswith("//"):
            if lines:
                break
            continue
        body = stripped[2:].strip()
        if not body:  # a blank comment line ends the first paragraph
            break
        lines.append(body)
    return re.sub(r"\s+", " ", " ".join(lines)).strip()


def parse_header(path, enums):
    """Every options struct and component function in one header."""
    source = path.read_text()
    fileDoc = header_doc(source)

    # Comments inside a member's default would confuse the member regex, and
    # block comments between members are documentation for the next one.
    structs = {}
    for name, body in STRUCT.findall(source):
        if not name.endswith("Options") and not name.endswith("State"):
            continue
        members = []
        for m in MEMBER.finditer(body):
            member_type = re.sub(r"\s+", " ", m.group("type")).strip()
            default = (m.group("default") or m.group("braced") or "").strip()
            default = re.sub(r"\s+", " ", default)
            doc = clean_doc(m.group("doc")) or clean_doc(m.group("trailing"))
            kind, inner = kind_of(member_type, enums)
            members.append(
                {
                    "name": m.group("name"),
                    "kind": kind,
                    "type": member_type,
                    "default": default,
                    "choices": enums.get(inner, []) if kind == "Enum" else [],
                    "doc": doc,
                    "optional": member_type.startswith("std::optional<"),
                }
            )
        structs[name] = members

    components = []
    for match in FUNCTION.finditer(source):
        returns = match.group("returns")
        name, args = match.group("name"), match.group("args")
        if "Ui&" not in args and "Ui &" not in args:
            continue
        if returns in {"return", "explicit", "friend", "using", "typedef"}:
            continue

        # The doc comment immediately above the declaration — the *last* one,
        # and only if nothing but whitespace separates it from the signature.
        # A search for the first `/**` in everything above matched the top of
        # the file and swallowed the header's own preamble.
        head = source[: match.start()]
        doc = ""
        doc_match = re.search(r"/\*\*((?:(?!\*/).)*)\*/\s*$", head, re.S)
        if doc_match:
            doc = clean_doc("/**" + doc_match.group(1) + "*/")

        options = ""
        for candidate in re.findall(r"const\s+(\w+Options)\s*&", args):
            options = candidate
            break

        signature = re.sub(r"\s+", " ", match.group(0)).strip()
        components.append(
            {
                "returns": re.sub(r"\s+", " ", returns).strip(),
                "name": name,
                "header": f"gbui/widgets/{path.name}",
                "summary": doc,
                "headerDoc": fileDoc,
                "options": options,
                "properties": structs.get(options, []),
                "container": returns.strip() == "Ui::Scope",
                "interactive": "Interaction" in args,
                "signature": signature,
            }
        )
    return components


def group_map():
    """Which group each header belongs to, from the umbrella that includes it."""
    groups = {}
    for umbrella, group in UMBRELLAS.items():
        path = WIDGETS / umbrella
        if not path.exists():
            raise SystemExit(f"generate_meta: {umbrella} is missing")
        for included in re.findall(r'#include\s+"gbui/widgets/(\w+\.hpp)"', path.read_text()):
            groups.setdefault(included, group)
    groups.update(EXPLICIT_GROUPS)
    return groups


def cpp_string(text):
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def render(entries):
    out = [
        "// GENERATED by tools/generate_meta.py — do not edit.",
        "//",
        "// One entry per component, read out of include/gbui/widgets/*.hpp. Regenerate",
        "// after adding a component or an option; CI checks that this file matches the",
        "// headers, so a stale table fails the build rather than misleading a tool.",
        '#include "gbui/meta/components.hpp"',
        "",
        "namespace gbui::meta {",
        "namespace {",
        "",
        "std::vector<ComponentInfo> build() {",
        "    std::vector<ComponentInfo> out;",
        f"    out.reserve({len(entries)});",
        "",
    ]
    for entry in entries:
        out.append("    out.push_back(ComponentInfo{")
        out.append(f"        {cpp_string(entry['name'])},")
        out.append(f"        {cpp_string(entry['group'])},")
        out.append(f"        {cpp_string(entry['header'])},")
        out.append(f"        {cpp_string(entry['summary'])},")
        out.append(f"        {cpp_string(entry['headerDoc'])},")
        out.append(f"        {cpp_string(entry['options'])},")
        if entry["properties"]:
            out.append("        {")
            for prop in entry["properties"]:
                choices = ", ".join(cpp_string(c) for c in prop["choices"])
                out.append(
                    "            PropertyInfo{"
                    f"{cpp_string(prop['name'])}, PropertyKind::{prop['kind']}, "
                    f"{cpp_string(prop['type'])}, {cpp_string(prop['default'])}, "
                    f"{{{choices}}}, {cpp_string(prop['doc'])}, "
                    f"{'true' if prop['optional'] else 'false'}}},"
                )
            out.append("        },")
        else:
            out.append("        {},")
        out.append(f"        {'true' if entry['container'] else 'false'},")
        out.append(f"        {'true' if entry['interactive'] else 'false'},")
        out.append(f"        {cpp_string(entry['signature'])},")
        out.append("    });")
    out += [
        "    return out;",
        "}",
        "",
        "}  // namespace",
        "",
        "const std::vector<ComponentInfo>& components() {",
        "    static const std::vector<ComponentInfo> table = build();",
        "    return table;",
        "}",
        "",
        "const ComponentInfo* find(std::string_view name) {",
        "    for (const ComponentInfo& entry : components()) {",
        "        if (entry.name == name) return &entry;",
        "    }",
        "    return nullptr;",
        "}",
        "",
        "const std::vector<std::string_view>& groups() {",
        "    static const std::vector<std::string_view> order = [] {",
        "        std::vector<std::string_view> out;",
        "        for (const ComponentInfo& entry : components()) {",
        "            bool seen = false;",
        "            for (const std::string_view group : out) seen = seen || group == entry.group;",
        "            if (!seen) out.push_back(entry.group);",
        "        }",
        "        return out;",
        "    }();",
        "    return order;",
        "}",
        "",
        "}  // namespace gbui::meta",
        "",
    ]
    return "\n".join(out)


def merge_overloads(entries):
    """One record per component, with the extra signatures listed.

    C++ declares a component once per overload and the table above keeps them
    apart, because a signature is a signature. A gallery wants one card per
    component with both signatures on it, and doing that merge here rather than
    in the page means every consumer gets the same answer.
    """
    merged = {}
    order = []
    for entry in entries:
        record = merged.get(entry["name"])
        if record is None:
            record = {
                "name": entry["name"],
                "group": entry["group"],
                "header": entry["header"],
                "summary": entry["headerDoc"] or entry["summary"],
                "signatures": [],
                "notes": [],
                "optionsType": entry["options"],
                "container": entry["container"],
                "interactive": entry["interactive"],
                "properties": entry["properties"],
            }
            merged[entry["name"]] = record
            order.append(entry["name"])
        record["signatures"].append(entry["signature"])
        if entry["summary"] and entry["summary"] not in record["notes"]:
            record["notes"].append(entry["summary"])
        # An overload that takes the options struct is the one that describes
        # the component; the other may take none at all.
        if entry["options"] and not record["optionsType"]:
            record["optionsType"] = entry["options"]
            record["properties"] = entry["properties"]
        record["interactive"] = record["interactive"] or entry["interactive"]
    return [merged[name] for name in order]


def main():
    groups = group_map()

    # Every enum in the widget headers, so a property that is one can carry its
    # choices. Collected across all of them because a header names enums that
    # another one's options use.
    enums = {}
    for path in sorted(WIDGETS.glob("*.hpp")):
        for name, body in ENUM.findall(path.read_text()):
            # The comments come out first. Several of these enumerations
            # document every value, and a comma inside a sentence is not a
            # separator between enumerators — splitting before stripping put
            # half an English clause into the choices.
            body = re.sub(r"/\*.*?\*/", " ", body, flags=re.S)
            body = re.sub(r"//[^\n]*", " ", body)
            values = [v.strip().split("=")[0].strip() for v in body.split(",")]
            enums[name] = [v for v in values if re.fullmatch(r"\w+", v)]

    entries = []
    complaints = []
    for path in sorted(WIDGETS.glob("*.hpp")):
        if path.name in UMBRELLAS:
            continue
        group = groups.get(path.name, "Other")
        header_checked = False
        for entry in parse_header(path, enums):
            entry["group"] = group
            entries.append(entry)

            # The style gate. Reported all at once rather than at the first
            # failure: a contributor fixing one header should not have to run
            # this five times to find the other four.
            if not header_checked:
                header_checked = True
                if not entry["headerDoc"]:
                    complaints.append(
                        f"{path.name}: opens with no sentence saying what it is")
                elif len(entry["headerDoc"]) > MAX_SUMMARY:
                    complaints.append(
                        f"{path.name}: its opening paragraph is {len(entry['headerDoc'])} "
                        f"characters and becomes the gallery card — put a blank comment "
                        f"line after the first sentence")
            for prop in entry["properties"]:
                if not prop["doc"] and needs_doc(prop["name"]):
                    complaints.append(
                        f"{path.name}: {entry['options']}::{prop['name']} has no doc comment")

    if not entries:
        raise SystemExit("generate_meta: parsed no components — the headers changed shape")

    if complaints:
        for complaint in sorted(set(complaints)):
            print(complaint, file=sys.stderr)
        print(f"generate_meta: {len(set(complaints))} header(s) do not meet the house style; "
              f"see CONTRIBUTING", file=sys.stderr)
        return 1

    order = {name: i for i, name in enumerate(GROUP_ORDER)}
    entries.sort(key=lambda e: (order.get(e["group"], len(order)), e["header"], e["name"]))

    text = render(entries)
    payload = json.dumps(merge_overloads(entries), indent=2) + "\n"

    if "--check" in sys.argv:
        stale = []
        if (OUT.read_text() if OUT.exists() else "") != text:
            stale.append(OUT)
        if (JSON_OUT.read_text() if JSON_OUT.exists() else "") != payload:
            stale.append(JSON_OUT)
        if stale:
            for path in stale:
                print(f"{path.relative_to(ROOT)} is stale", file=sys.stderr)
            print("run tools/generate_meta.py", file=sys.stderr)
            return 1
        print(f"{len(entries)} components, both tables are current")
        return 0

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(text)
    JSON_OUT.parent.mkdir(parents=True, exist_ok=True)
    JSON_OUT.write_text(payload)
    print(f"{OUT.relative_to(ROOT)}: {len(entries)} components in "
          f"{len({e['group'] for e in entries})} groups")
    print(f"{JSON_OUT.relative_to(ROOT)}: the same table, for the site")
    return 0


if __name__ == "__main__":
    sys.exit(main())
