#!/usr/bin/env bash
# Builds the demo screens for the browser and drops the result where the
# documentation site can serve it.
#
#   tools/build_wasm.sh                 build into docs/public/demo/
#   tools/build_wasm.sh --out DIR       somewhere else
#   tools/build_wasm.sh --debug         unoptimised, with assertions
#
# It needs the Emscripten SDK. Point EMSDK at a checkout, or let the script
# find one at ~/emsdk or /usr/lib/emsdk; if `emcc` is already on the PATH it is
# used as it is.
#
# ---- fonts ------------------------------------------------------------------
#
# A browser has no /usr/share/fonts, and a UI toolkit with no face draws no
# text at all — so three faces travel with the module, preloaded into its
# virtual filesystem at /fonts. They are not committed to this repository: a
# megabyte of binary in a source tree is a megabyte every clone pays for, and
# the licences differ per file. The script finds them instead, in this order:
#
#   1. $GBUI_DEMO_FONT_DIR, holding ui.ttf, ui-bold.ttf and mono.ttf
#   2. demos/web/fonts, the same three names
#   3. whatever this machine has, preferring the metric-compatible families
#
# Case 3 is what a contributor gets for free and what CI uses; case 1 is how a
# release pins the faces so the published demo looks the same everywhere.
set -euo pipefail

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$HERE"

OUT="$HERE/docs/public/demo"
BUILD_TYPE=MinSizeRel
BUILD_DIR="$HERE/build-wasm"

while [ $# -gt 0 ]; do
    case "$1" in
        --out) OUT="$2"; shift 2 ;;
        --debug) BUILD_TYPE=Debug; BUILD_DIR="$HERE/build-wasm-debug"; shift ;;
        --build-dir) BUILD_DIR="$2"; shift 2 ;;
        -h|--help) sed -n '2,30p' "${BASH_SOURCE[0]}"; exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
done

# ---- the toolchain ----------------------------------------------------------
if ! command -v emcmake >/dev/null 2>&1; then
    for candidate in "${EMSDK:-}" "$HOME/emsdk" /usr/lib/emsdk /opt/emsdk; do
        [ -n "$candidate" ] && [ -f "$candidate/emsdk_env.sh" ] || continue
        # shellcheck disable=SC1091
        source "$candidate/emsdk_env.sh" >/dev/null 2>&1 || true
        break
    done
fi
if ! command -v emcmake >/dev/null 2>&1; then
    cat >&2 <<'MESSAGE'
emcmake was not found.

Install the Emscripten SDK and either activate it in this shell or point EMSDK
at the checkout:

    git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
    ~/emsdk/emsdk install latest && ~/emsdk/emsdk activate latest
    source ~/emsdk/emsdk_env.sh
MESSAGE
    exit 1
fi

# ---- the faces --------------------------------------------------------------
FONT_STAGE="$BUILD_DIR/fonts"
rm -rf "$FONT_STAGE"
mkdir -p "$FONT_STAGE"

# The first readable file of those named. `fc-match` is consulted first when it
# exists, because it knows what this machine actually has.
find_face() {
    local family="$1"; shift
    if command -v fc-match >/dev/null 2>&1; then
        local matched
        matched=$(fc-match -f '%{file}' "$family" 2>/dev/null || true)
        # fc-match always answers, with whatever it considers closest — so the
        # answer is only taken when it really is the family that was asked for.
        if [ -n "$matched" ] && [ -r "$matched" ]; then
            local got
            got=$(fc-match -f '%{family}' "$family" 2>/dev/null || true)
            case "${got,,}" in
                *"${family,,}"*) echo "$matched"; return 0 ;;
            esac
        fi
    fi
    for path in "$@"; do
        [ -r "$path" ] && { echo "$path"; return 0; }
    done
    return 1
}

stage_face() {
    local name="$1"; shift
    local source=""

    for directory in "${GBUI_DEMO_FONT_DIR:-}" "$HERE/demos/web/fonts"; do
        [ -n "$directory" ] && [ -r "$directory/$name" ] && { source="$directory/$name"; break; }
    done

    if [ -z "$source" ]; then
        source=$("$@") || true
    fi
    if [ -z "$source" ]; then
        echo "no face found for $name" >&2
        return 1
    fi
    cp "$source" "$FONT_STAGE/$name"
    echo "    $name  <-  $source"
}

echo "==> fonts"
stage_face ui.ttf find_face "Liberation Sans" \
    /usr/share/fonts/liberation-sans-fonts/LiberationSans-Regular.ttf \
    /usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf \
    /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf \
    /usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf \
    /System/Library/Fonts/Helvetica.ttc
stage_face ui-bold.ttf find_face "Liberation Sans:bold" \
    /usr/share/fonts/liberation-sans-fonts/LiberationSans-Bold.ttf \
    /usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf \
    /usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf \
    /usr/share/fonts/dejavu-sans-fonts/DejaVuSans-Bold.ttf
stage_face mono.ttf find_face "Liberation Mono" \
    /usr/share/fonts/liberation-mono-fonts/LiberationMono-Regular.ttf \
    /usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf \
    /usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf \
    /usr/share/fonts/dejavu-sans-mono-fonts/DejaVuSansMono.ttf

# ---- build ------------------------------------------------------------------
echo "==> configure ($BUILD_TYPE)"
emcmake cmake -S "$HERE" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DGBUI_BUILD_TESTS=OFF \
    -DGBUI_BUILD_EXAMPLES=OFF \
    -DGBUI_BUILD_DEMOS=ON \
    -DGBUI_PLATFORM_SDL2=OFF \
    -DGBUI_WERROR=OFF \
    -DGBUI_WASM_FONT_DIR="$FONT_STAGE" >/dev/null

echo "==> build"
cmake --build "$BUILD_DIR" --target gbui_demos_wasm --parallel

# ---- publish ----------------------------------------------------------------
mkdir -p "$OUT"
for artefact in gbui-demos.js gbui-demos.wasm gbui-demos.data; do
    if [ -f "$BUILD_DIR/demos/$artefact" ]; then
        cp "$BUILD_DIR/demos/$artefact" "$OUT/$artefact"
    fi
done
# The loader is written by hand and lives beside the sources it drives, so a
# change to it is a change to a file a reader can find.
cp "$HERE/demos/web/gbui-embed.js" "$OUT/gbui-embed.js"

echo "==> $OUT"
ls -lh "$OUT" | tail -n +2
