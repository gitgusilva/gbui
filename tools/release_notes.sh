#!/usr/bin/env bash
# Prints one version's section of CHANGELOG.md, for the body of a GitHub release.
#
#   tools/release_notes.sh 0.2.1          from the changelog in this checkout
#   tools/release_notes.sh 0.2.1 v0.2.1   from the changelog as it was at a tag
#
# The release notes are not written twice. The changelog is the record — written
# from the history, reviewed in the pull request that shipped the change — and a
# release whose notes were typed into a web form on the day is a second version
# of the same text that starts drifting immediately. This reads the first one.
#
# A section runs from its `## [x.y.z]` heading to the next `##` heading, with the
# link-reference block at the bottom of the file left off: those are relative to
# the changelog, and on a release page they would be footnotes to nothing.
set -euo pipefail

version=${1:-}
ref=${2:-}
if [ -z "$version" ]; then
    echo "usage: $(basename "$0") <version> [git-ref]" >&2
    exit 2
fi

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$HERE"

if [ -n "$ref" ]; then
    changelog=$(git show "$ref:CHANGELOG.md")
else
    changelog=$(cat CHANGELOG.md)
fi

# `## [0.2.1] — 2026-08-13`, and also a bare `## [0.2.1]` for a section written
# before the tag date was known. The version is matched literally, so 0.2 does
# not answer for 0.2.1.
notes=$(printf '%s\n' "$changelog" | awk -v version="$version" '
    # Start at this version'"'"'s heading, stop at the next one.
    $0 ~ "^## \\[" version "\\]([^0-9].*)?$" { collecting = 1; next }
    collecting && /^## / { exit }
    # The `[0.2]: https://…` block at the foot of the file is changelog
    # furniture, not part of the section.
    collecting && /^\[[^]]+\]: / { next }
    collecting { print }
')

if [ -z "${notes//[[:space:]]/}" ]; then
    echo "no section for $version in CHANGELOG.md${ref:+ at $ref}" >&2
    exit 1
fi

# Trim the blank lines the heading above and the section below leave behind:
# buffer runs of them and only emit one once something follows.
printf '%s\n' "$notes" | awk '
    NF == 0 { if (started) pending++; next }
    { while (pending-- > 0) print ""; pending = 0; started = 1; print }
'
