#!/usr/bin/env bash
# Builds the documentation, one directory per version.
#
#   tools/build_docs.sh                 build the working tree as the current version
#   tools/build_docs.sh 0.1 0.2         also build those tags into /v0.1/ and /v0.2/
#
# The newest build lands at the root of docs/.vitepress/dist and every version
# also gets its own directory, so a link to /v0.2/guide/layout keeps working
# after 0.3 ships. VitePress has no versioning of its own; this convention is
# what the Vite and Vue sites use.
set -euo pipefail

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$HERE"
OUT=docs/.vitepress/dist

command -v npm >/dev/null || { echo "npm is required" >&2; exit 1; }
[ -d node_modules ] || npm install

echo "==> current"
npx vitepress build docs
mkdir -p "$OUT"

# Older versions are built from their tags into a worktree, so the content is
# whatever that release actually said rather than today's text with a label.
for version in "$@"; do
    tag="v${version}"
    git rev-parse "$tag" >/dev/null 2>&1 || { echo "no tag $tag, skipping"; continue; }
    echo "==> $tag"
    worktree=$(mktemp -d)
    git worktree add --detach "$worktree" "$tag" >/dev/null
    (
        cd "$worktree"
        ln -s "$HERE/node_modules" node_modules
        GBUI_DOCS_VERSION="$version" GBUI_DOCS_BASE="/v${version}/" npx vitepress build docs
    )
    rm -rf "${OUT:?}/v${version}"
    mkdir -p "$OUT/v${version}"
    cp -r "$worktree/docs/.vitepress/dist/." "$OUT/v${version}/"
    git worktree remove --force "$worktree"
done

echo "==> $OUT"
