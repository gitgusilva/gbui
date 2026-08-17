// The demo catalogue, compiled to WebAssembly.
//
// A C surface over `Host`, and nothing else — no rendering, no layout, no
// opinion about what a demo is. JavaScript creates a host, tells it how big the
// canvas is, pushes pointer and keyboard events at it, calls `frame` once per
// animation frame and copies `pixels` into an ImageData. That is the whole
// contract, and `demos/web/gbui-demos.js` is the reference implementation of
// the other side of it.
//
// Why a C surface rather than Embind: the wrapper is a hundred lines either
// way, and this one adds nothing to the download. The module is already the
// toolkit plus six screens plus a font; a binding layer that generates
// JavaScript glue for classes nobody outside this file will ever construct is
// weight for no reader's benefit.
//
// Strings returned from here are heap-allocated and the caller frees them with
// `_free`, which is exported for exactly that.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

#include <emscripten/emscripten.h>

#include "gbui/meta/components.hpp"
#include "gbui_demos/catalog.hpp"
#include "gbui_demos/demos.hpp"
#include "gbui_demos/host.hpp"

using namespace gbui;

namespace {

demos::Host* asHost(void* handle) { return static_cast<demos::Host*>(handle); }

/** A copy of `text` on the C heap, for JavaScript to read and free. */
char* duplicate(const std::string& text) {
    char* out = static_cast<char*>(std::malloc(text.size() + 1));
    if (!out) return nullptr;
    std::memcpy(out, text.c_str(), text.size() + 1);
    return out;
}

/** JSON string escaping, for the small set of characters the catalogue can
 *  actually contain. Everything in it is written in this repository, so the
 *  cases that matter are quotes and backslashes; the control characters are
 *  handled anyway rather than left as a trap for the next entry. */
void appendJsonString(std::string& out, std::string_view value) {
    out += '"';
    for (const char c : value) {
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                // UTF-8 continuation bytes are negative as `char` and pass
                // through untouched, which is what JSON wants.
                if (static_cast<unsigned char>(c) < 0x20) {
                    char escape[8];
                    std::snprintf(escape, sizeof(escape), "\\u%04x",
                                  static_cast<unsigned>(static_cast<unsigned char>(c)));
                    out += escape;
                } else {
                    out += c;
                }
        }
    }
    out += '"';
}

/**
 * Browser key names to the toolkit's vocabulary.
 *
 * By name rather than by number on purpose: a numeric table shared between C++
 * and JavaScript is two lists that have to be edited together, and the day one
 * of them is not is the day Tab starts inserting text. `KeyboardEvent.key` is
 * a string the platform already agrees on.
 */
Key keyFromName(std::string_view name) {
    if (name == "ArrowLeft") return Key::Left;
    if (name == "ArrowRight") return Key::Right;
    if (name == "ArrowUp") return Key::Up;
    if (name == "ArrowDown") return Key::Down;
    if (name == "Home") return Key::Home;
    if (name == "End") return Key::End;
    if (name == "PageUp") return Key::PageUp;
    if (name == "PageDown") return Key::PageDown;
    if (name == "Backspace") return Key::Backspace;
    if (name == "Delete") return Key::Delete;
    if (name == "Enter") return Key::Return;
    if (name == "Tab") return Key::Tab;
    if (name == "Escape") return Key::Escape;
    if (name == " ") return Key::Space;
    if (name == "+" || name == "=") return Key::Plus;
    if (name == "-" || name == "_") return Key::Minus;
    if (name.size() == 1) {
        switch (name[0]) {
            case 'a':
            case 'A':
                return Key::A;
            case 'c':
            case 'C':
                return Key::C;
            case 'v':
            case 'V':
                return Key::V;
            case 'x':
            case 'X':
                return Key::X;
            case 'z':
            case 'Z':
                return Key::Z;
            case 'y':
            case 'Y':
                return Key::Y;
            case 't':
            case 'T':
                return Key::T;
            case 'd':
            case 'D':
                return Key::D;
            default:
                break;
        }
    }
    return Key::Unknown;
}

/** The CSS name for a cursor. Returned as a string so the page can write it
 *  straight into `style.cursor` and this file stays the only place that knows
 *  the mapping. */
const char* cssCursor(Cursor cursor) {
    switch (cursor) {
        case Cursor::Pointer:
            return "pointer";
        case Cursor::Text:
            return "text";
        case Cursor::Hand:
            return "pointer";
        case Cursor::Grab:
            return "grab";
        case Cursor::Grabbing:
            return "grabbing";
        case Cursor::ResizeHorizontal:
            return "ew-resize";
        case Cursor::ResizeVertical:
            return "ns-resize";
        case Cursor::ResizeDiagonalUp:
            return "nesw-resize";
        case Cursor::ResizeDiagonalDown:
            return "nwse-resize";
        case Cursor::NotAllowed:
            return "not-allowed";
        case Cursor::Progress:
            return "progress";
        case Cursor::Wait:
            return "wait";
        case Cursor::Crosshair:
            return "crosshair";
        case Cursor::Help:
            return "help";
        case Cursor::Default:
            break;
    }
    return "default";
}

}  // namespace

extern "C" {

// ---------------------------------------------------------------------------
// The catalogue, without creating anything
// ---------------------------------------------------------------------------

/** Every demo as JSON, for a page that wants to build its own picker. Free the
 *  result with `_free`. */
EMSCRIPTEN_KEEPALIVE char* gbui_demos_catalogue() {
    std::string json = "[";
    bool first = true;
    for (const demos::DemoInfo& entry : demos::catalogue()) {
        if (!first) json += ',';
        first = false;
        json += "{\"id\":";
        appendJsonString(json, entry.id);
        json += ",\"title\":";
        appendJsonString(json, entry.title);
        json += ",\"sector\":";
        appendJsonString(json, entry.sector);
        json += ",\"summary\":";
        appendJsonString(json, entry.summary);
        json += ",\"tryThis\":";
        appendJsonString(json, entry.tryThis);
        json += ",\"palette\":";
        appendJsonString(json, entry.palette == demos::Palette::Dark    ? "dark"
                               : entry.palette == demos::Palette::Light ? "light"
                                                                        : "follow");
        json += ",\"width\":" + std::to_string(static_cast<int>(entry.design.x));
        json += ",\"height\":" + std::to_string(static_cast<int>(entry.design.y));
        json += ",\"highlights\":[";
        bool firstHighlight = true;
        for (const std::string_view highlight : entry.highlights) {
            if (!firstHighlight) json += ',';
            firstHighlight = false;
            appendJsonString(json, highlight);
        }
        json += "]}";
    }
    json += ']';
    return duplicate(json);
}

/**
 * Every component the library declares, as JSON, with everything a gallery
 * needs to describe one: its group, the header's own summary, the declaration
 * doc, the signature, whether it is a container, whether it reacts to the
 * pointer, and each option with its type, default and documentation.
 *
 * Read straight out of `gbui::meta`, which is generated from the headers — so
 * a page built on this cannot describe a component the library does not have,
 * or miss an option it does. Free the result with `_free`.
 */
EMSCRIPTEN_KEEPALIVE char* gbui_demos_components() {
    std::string json = "[";
    bool first = true;
    for (const meta::ComponentInfo& entry : meta::components()) {
        // One entry per declaration, so an overloaded component appears twice.
        // Merged here rather than in the page: the first carries the group and
        // the options, and a second signature is appended to it.
        if (!first) {
            const std::size_t previous = json.rfind("{\"name\":\"");
            if (previous != std::string::npos) {
                const std::string tail = json.substr(previous);
                const std::string needle = "{\"name\":\"" + std::string(entry.name) + "\",";
                if (tail.rfind(needle, 0) == 0) {
                    json += ",\"also\":";
                    appendJsonString(json, entry.signature);
                    continue;
                }
            }
        }
        if (!first) json += "},";
        first = false;

        json += "{\"name\":";
        appendJsonString(json, entry.name);
        json += ",\"group\":";
        appendJsonString(json, entry.group);
        json += ",\"header\":";
        appendJsonString(json, entry.header);
        json += ",\"summary\":";
        appendJsonString(json, entry.headerDoc.empty() ? entry.summary : entry.headerDoc);
        json += ",\"detail\":";
        appendJsonString(json, entry.summary);
        json += ",\"signature\":";
        appendJsonString(json, entry.signature);
        json += ",\"optionsType\":";
        appendJsonString(json, entry.optionsType);
        json += std::string(",\"container\":") + (entry.container ? "true" : "false");
        json += std::string(",\"interactive\":") + (entry.interactive ? "true" : "false");
        json += ",\"properties\":[";
        bool firstProperty = true;
        for (const meta::PropertyInfo& property : entry.properties) {
            if (!firstProperty) json += ',';
            firstProperty = false;
            json += "{\"name\":";
            appendJsonString(json, property.name);
            json += ",\"type\":";
            appendJsonString(json, property.type);
            json += ",\"default\":";
            appendJsonString(json, property.defaultText);
            json += ",\"doc\":";
            appendJsonString(json, property.doc);
            json += std::string(",\"optional\":") + (property.optional ? "true" : "false");
            json += ",\"choices\":[";
            bool firstChoice = true;
            for (const std::string_view choice : property.choices) {
                if (!firstChoice) json += ',';
                firstChoice = false;
                appendJsonString(json, choice);
            }
            json += "]}";
        }
        json += ']';
    }
    if (!first) json += '}';
    json += ']';
    return duplicate(json);
}

/** The skins a host can be switched between, as JSON. Free with `_free`. */
EMSCRIPTEN_KEEPALIVE char* gbui_demos_skins() {
    std::string json = "[";
    bool first = true;
    for (const demos::Skin& skin : demos::skins()) {
        if (!first) json += ',';
        first = false;
        json += "{\"id\":";
        appendJsonString(json, skin.id);
        json += ",\"name\":";
        appendJsonString(json, skin.name);
        json += '}';
    }
    json += ']';
    return duplicate(json);
}

// ---------------------------------------------------------------------------
// A host
// ---------------------------------------------------------------------------

/**
 * Creates a host showing `id`, sized in logical pixels.
 *
 * The font is registered here rather than left to the caller, because there is
 * exactly one place it can come from: the files preloaded into `/fonts` by the
 * build. `useBundledFontsOnly` first, so nothing goes looking through a
 * filesystem that does not exist.
 */
EMSCRIPTEN_KEEPALIVE void* gbui_demos_create(const char* id, int width, int height, float scale,
                                             int darkMode) {
    demos::HostOptions options;
    options.width = width;
    options.height = height;
    options.scale = scale;
    options.darkMode = darkMode != 0;
    options.demo = id ? id : "";

    auto* host = new demos::Host(options);
    host->useBundledFontsOnly();
    // Names the theme's own families ask for, so no theme has to be rewritten
    // for the browser. See tools/build_wasm.sh for where the files come from.
    host->addFont("Demo Sans", "/fonts/ui.ttf");
    host->addFont("Demo Sans", "/fonts/ui-bold.ttf", FontWeight::Bold);
    host->addFont("Demo Mono", "/fonts/mono.ttf");
    host->setFontFamilies("Demo Sans", "Demo Mono");
    return host;
}

EMSCRIPTEN_KEEPALIVE void gbui_demos_destroy(void* handle) { delete asHost(handle); }

EMSCRIPTEN_KEEPALIVE int gbui_demos_select(void* handle, const char* id) {
    return asHost(handle)->select(id ? id : "") ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE void gbui_demos_restart(void* handle) { asHost(handle)->restart(); }

/** Shows one component's live example instead of an application screen. */
EMSCRIPTEN_KEEPALIVE int gbui_demos_select_component(void* handle, const char* name) {
    return asHost(handle)->selectComponent(name ? name : "") ? 1 : 0;
}

/**
 * How tall one component's example wants its canvas, in logical pixels.
 *
 * No handle: it is a property of the example, not of a running host, and the
 * page wants it before it decides how much room to leave. One number for every
 * preview is what makes an open combobox look broken when the control is fine —
 * twelve rows and a filter box want four hundred pixels and a checkbox wants a
 * hundred and forty.
 */
EMSCRIPTEN_KEEPALIVE int gbui_demos_component_height(const char* name) {
    return demos::previewHeightFor(name ? name : "");
}

EMSCRIPTEN_KEEPALIVE void gbui_demos_set_skin(void* handle, const char* id) {
    asHost(handle)->setSkin(id ? id : "");
}

EMSCRIPTEN_KEEPALIVE void gbui_demos_set_dark(void* handle, int dark) {
    asHost(handle)->setDarkMode(dark != 0);
}

EMSCRIPTEN_KEEPALIVE void gbui_demos_set_font_size(void* handle, float size) {
    asHost(handle)->setFontSize(size);
}

EMSCRIPTEN_KEEPALIVE void gbui_demos_resize(void* handle, int width, int height, float scale) {
    asHost(handle)->resize(width, height, scale);
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

EMSCRIPTEN_KEEPALIVE void gbui_demos_pointer(void* handle, float x, float y, int down) {
    demos::Host* host = asHost(handle);
    host->pointerMove(x, y);
    host->pointerButton(down != 0);
}

/** The secondary button, level like the primary one. What opens a context
 *  menu, and the reason the page has to swallow the browser's own. */
EMSCRIPTEN_KEEPALIVE void gbui_demos_secondary(void* handle, int down) {
    asHost(handle)->secondaryButton(down != 0);
}

EMSCRIPTEN_KEEPALIVE void gbui_demos_pointer_leave(void* handle) { asHost(handle)->pointerLeave(); }

EMSCRIPTEN_KEEPALIVE void gbui_demos_wheel(void* handle, float lines) {
    asHost(handle)->wheel(lines);
}

EMSCRIPTEN_KEEPALIVE void gbui_demos_modifiers(void* handle, int shift, int ctrl, int alt,
                                               int meta) {
    Modifiers modifiers;
    modifiers.shift = shift != 0;
    modifiers.ctrl = ctrl != 0;
    modifiers.alt = alt != 0;
    modifiers.super = meta != 0;
    asHost(handle)->setModifiers(modifiers);
}

/** `name` is a `KeyboardEvent.key`. Returns 1 when the toolkit knows the key,
 *  so the page can decide whether to call `preventDefault`. */
EMSCRIPTEN_KEEPALIVE int gbui_demos_key(void* handle, const char* name, int repeat) {
    const Key key = keyFromName(name ? name : "");
    if (key == Key::Unknown) return 0;
    asHost(handle)->key(key, repeat != 0);
    return 1;
}

EMSCRIPTEN_KEEPALIVE void gbui_demos_text(void* handle, const char* utf8) {
    if (utf8) asHost(handle)->type(utf8);
}

// ---------------------------------------------------------------------------
// The frame
// ---------------------------------------------------------------------------

EMSCRIPTEN_KEEPALIVE void gbui_demos_frame(void* handle, float delta) {
    asHost(handle)->frame(delta);
}

/** The framebuffer, as a pointer into the wasm heap. Valid until the next
 *  `resize`, which is why the page re-reads it whenever the canvas changes. */
EMSCRIPTEN_KEEPALIVE const unsigned char* gbui_demos_pixels(void* handle) {
    return asHost(handle)->canvas().pixels();
}

EMSCRIPTEN_KEEPALIVE int gbui_demos_pixel_width(void* handle) {
    return asHost(handle)->canvas().width();
}

EMSCRIPTEN_KEEPALIVE int gbui_demos_pixel_height(void* handle) {
    return asHost(handle)->canvas().height();
}

/** The CSS cursor name for wherever the pointer is. Points into static
 *  storage; do not free it. */
EMSCRIPTEN_KEEPALIVE const char* gbui_demos_cursor(void* handle) {
    return cssCursor(asHost(handle)->cursor());
}

/** Whether a control has the keyboard. A page uses it to decide whether a key
 *  belongs to the demo or to the page around it. */
EMSCRIPTEN_KEEPALIVE int gbui_demos_has_focus(void* handle) {
    return asHost(handle)->focused().empty() ? 0 : 1;
}

}  // extern "C"
