#!/usr/bin/env bash
# Builds the documentation, one directory per version.
#
#   tools/build_docs.sh              build the current version, plus every
#                                    version listed in docs/.vitepress/versions.ts
#   tools/build_docs.sh 0.1 0.2      build those versions instead of that list
#
# The current version lands at the root of docs/.vitepress/dist and every older
# one also gets its own directory, so a link to /v0.2/guide/layout keeps working
# after 0.3 ships. VitePress has no versioning of its own; this convention is
# what the Vite and Vue sites use.
#
# Two environment variables, both set by the Pages workflow and both optional
# here:
#
#   GBUI_DOCS_BASE      where the site is served from — "/" locally, "/gbui/" on
#                       a GitHub project page. An archived version is built at
#                       that base plus its own directory.
#   GBUI_DOCS_SITE_URL  absolute URL of the site root, for the links that point
#                       from one version to another. See versions.ts for why
#                       those cannot be relative.
set -euo pipefail

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$HERE"
OUT=docs/.vitepress/dist
SITE_BASE=${GBUI_DOCS_BASE:-/}
# A base VitePress accepts: leading and trailing slash, no doubles.
SITE_BASE="/${SITE_BASE#/}"
SITE_BASE="${SITE_BASE%/}/"

command -v npm >/dev/null || { echo "npm is required" >&2; exit 1; }
[ -d node_modules ] || npm install

# Without arguments the versions to keep are the ones the site advertises, so
# the script and the version dropdown cannot disagree about what exists.
current=$(node --input-type=module -e '
    import { current } from "./docs/.vitepress/versions.ts";
    console.log(current);
' 2>/dev/null || true)

versions=("$@")
if [ ${#versions[@]} -eq 0 ]; then
    # Loudly, not quietly: a failure here would otherwise publish a dropdown
    # advertising versions whose directories were never built.
    if ! listing=$(node --input-type=module -e '
        import { archived } from "./docs/.vitepress/versions.ts";
        for (const v of archived) console.log(v);
    ' 2>/dev/null); then
        echo 'cannot read the archived list from docs/.vitepress/versions.ts' >&2
        echo "node 22.6+ reads TypeScript directly; on an older one, name the" >&2
        echo "versions instead: tools/build_docs.sh 0.2" >&2
        exit 1
    fi
    mapfile -t versions <<< "$listing"
fi

echo "==> current (base ${SITE_BASE})"
GBUI_DOCS_BASE="$SITE_BASE" npx vitepress build docs
mkdir -p "$OUT"

# The current version gets its permanent address on the day it ships, not on
# the day it is replaced. Built rather than copied: a copy would still carry the
# root's base, so every link inside /v0.2/ would quietly walk back out to
# whatever the latest docs happen to be by then.
#
# When the next release moves this version into `archived`, the same directory
# is rebuilt from its tag — the URL does not change, and neither does what it
# says.
if [ -n "$current" ]; then
    echo "==> v${current} (current, pinned)"
    tmp=$(mktemp -d)
    GBUI_DOCS_VERSION="$current" GBUI_DOCS_BASE="${SITE_BASE}v${current}/" \
        npx vitepress build docs --outDir "$tmp"
    rm -rf "${OUT:?}/v${current}"
    mkdir -p "$OUT/v${current}"
    cp -r "$tmp/." "$OUT/v${current}/"
    rm -rf "$tmp"
fi

# Older versions are built from their tags into a worktree, so the content is
# whatever that release actually said rather than today's text with a label.
for version in "${versions[@]:-}"; do
    [ -z "$version" ] && continue

    # A version's directory is that release *line*, so the tag to build is the
    # newest tag in it: `/v0.2/` should say what 0.2.1 says, because 0.2.1 is
    # what someone reading `/v0.2/` would install. `v0.2` and `v0.2.0` and
    # `v0.2.1` are all candidates and the version sort picks between them, which
    # also means a release tagged `v0.2.0` is found rather than skipped in
    # silence — that would leave a dead entry in the dropdown.
    tag=$(git tag --list "v${version}" "v${version}.*" --sort=-v:refname | head -n 1)
    if [ -z "$tag" ]; then
        echo "no tag for $version, skipping" >&2
        continue
    fi

    echo "==> v${version} (from ${tag})"
    worktree=$(mktemp -d)
    git worktree add --detach "$worktree" "$tag" >/dev/null
    (
        cd "$worktree"
        ln -s "$HERE/node_modules" node_modules
        GBUI_DOCS_VERSION="$version" \
        GBUI_DOCS_BASE="${SITE_BASE}v${version}/" \
        GBUI_DOCS_SITE_URL="${GBUI_DOCS_SITE_URL:-}" \
            npx vitepress build docs
    )
    rm -rf "${OUT:?}/v${version}"
    mkdir -p "$OUT/v${version}"
    cp -r "$worktree/docs/.vitepress/dist/." "$OUT/v${version}/"
    git worktree remove --force "$worktree"
done

echo "==> $OUT"
