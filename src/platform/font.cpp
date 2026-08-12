#include "gbui/platform/font.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include "gbui/layout/textWrap.hpp"
#include "stb_truetype.h"

namespace gbui {
namespace {

/**
 * Families to try when the theme asks for one this machine does not have.
 *
 * Ordered by *how well they render here*, not by popularity, and the reason is
 * this loader: it has no variable-font axes, so a family shipped as one
 * variable file per slant — Adwaita Sans, Noto Sans — is only ever drawn at
 * that file's default instance. A face with real static weights is a better
 * fallback than a nominally nicer one that cannot go bold.
 *
 * The platform's own UI font would be better still, and asking for it is a job
 * for `platform/` rather than a list; until then an application that cares
 * ships its faces and names them with `addFontFile`, which is exactly what that
 * exists for.
 */
const char* kSansFallbacks[] = {"IBMPlexSans", "LiberationSans", "DejaVuSans", "Arial",
                                "SegoeUI",     "Roboto",         "NotoSans",   "Cantarell",
                                "Ubuntu"};
const char* kMonoFallbacks[] = {"IBMPlexMono",   "LiberationMono", "DejaVuSansMono",
                                "CascadiaMono",  "Consolas",       "NotoSansMono",
                                "RobotoMono",    "Hack",           "Ubuntu Mono"};

std::string squash(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        if (c == ' ' || c == '-' || c == '_') continue;
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

bool isFontFile(const std::filesystem::path& path) {
    const std::string extension = squash(path.extension().string());
    // .ttc is a collection: several faces behind one file and an index this
    // loader does not take. Skipped rather than half-supported.
    return extension == ".ttf" || extension == ".otf";
}

/** What a file name says about the face inside it.
 *
 * Font files do not carry a machine-readable weight anywhere this loader can
 * see without parsing the OS/2 table, and their names are the convention every
 * tool leans on. Order matters: "semibold" contains "bold", and "extralight"
 * contains "light", so the longer names are tested first. */
FontWeight weightFromName(const std::string& squashed) {
    struct Entry {
        const char* token;
        FontWeight weight;
    };
    static const Entry kTokens[] = {
        {"extrabold", FontWeight::ExtraBold}, {"ultrabold", FontWeight::ExtraBold},
        {"semibold", FontWeight::SemiBold},   {"demibold", FontWeight::SemiBold},
        {"extralight", FontWeight::ExtraLight}, {"ultralight", FontWeight::ExtraLight},
        {"hairline", FontWeight::Thin},       {"black", FontWeight::Black},
        {"heavy", FontWeight::Black},         {"bold", FontWeight::Bold},
        {"medium", FontWeight::Medium},       {"light", FontWeight::Light},
        {"thin", FontWeight::Thin},
    };
    for (const Entry& entry : kTokens) {
        if (squashed.find(entry.token) != std::string::npos) return entry.weight;
    }
    return FontWeight::Regular;
}

bool italicFromName(const std::string& squashed) {
    return squashed.find("italic") != std::string::npos ||
           squashed.find("oblique") != std::string::npos;
}

/**
 * The name with every style word taken out, so what is left is the family.
 *
 * The point is to measure a file's distance from the family it matched without
 * that distance being *the style*: `LiberationSans-Bold` is three characters
 * shorter than `LiberationSans-Regular`, and a comparison that does not strip
 * "bold" and "regular" first concludes the bold file is the better Liberation
 * Sans. It is the same family. The weight is what tells them apart, and that is
 * scored separately.
 */
std::string withoutStyleWords(std::string squashed) {
    static const char* kWords[] = {
        "extrabold", "ultrabold", "semibold",  "demibold",  "extralight", "ultralight",
        "hairline",  "oblique",   "italic",    "regular",   "normal",     "book",
        "roman",     "black",     "heavy",     "bold",      "medium",     "light",
        "thin",      "condensed", "expanded",  "narrow",    "variablefont", "wght",
    };
    for (const char* word : kWords) {
        for (std::size_t at = squashed.find(word); at != std::string::npos;
             at = squashed.find(word)) {
            squashed.erase(at, std::strlen(word));
        }
    }
    return squashed;
}

/** Condensed and other width variants are not what a UI asked for; they are
 *  kept only as a last resort. */
bool isWidthVariant(const std::string& squashed) {
    return squashed.find("condensed") != std::string::npos ||
           squashed.find("expanded") != std::string::npos ||
           squashed.find("narrow") != std::string::npos;
}

}  // namespace

struct Font::Impl {
    std::vector<std::uint8_t> data;
    stbtt_fontinfo info{};
    Synthesis synthesis{};
};

char32_t nextCodepoint(std::string_view text, std::size_t& cursor) {
    if (cursor >= text.size()) return 0;
    const auto lead = static_cast<unsigned char>(text[cursor]);
    std::size_t extra = 0;
    char32_t value = lead;
    if (lead >= 0xF0) { extra = 3; value = lead & 0x07U; }
    else if (lead >= 0xE0) { extra = 2; value = lead & 0x0FU; }
    else if (lead >= 0xC0) { extra = 1; value = lead & 0x1FU; }
    ++cursor;
    for (std::size_t i = 0; i < extra && cursor < text.size(); ++i, ++cursor) {
        value = (value << 6) | (static_cast<unsigned char>(text[cursor]) & 0x3FU);
    }
    return value;
}

std::shared_ptr<Font> Font::load(const std::string& path, float pixelSize,
                                 const Synthesis& synthesis) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return nullptr;

    const auto size = static_cast<std::streamsize>(file.tellg());
    file.seekg(0);
    auto impl = std::make_shared<Impl>();
    impl->data.resize(static_cast<std::size_t>(size));
    if (!file.read(reinterpret_cast<char*>(impl->data.data()), size)) return nullptr;

    const int offset = stbtt_GetFontOffsetForIndex(impl->data.data(), 0);
    if (offset < 0 || !stbtt_InitFont(&impl->info, impl->data.data(), offset)) return nullptr;

    impl->synthesis = synthesis;
    auto font = std::make_shared<Font>();
    font->impl_ = impl;
    font->scale_ = stbtt_ScaleForPixelHeight(&impl->info, pixelSize);

    int ascent = 0;
    int descent = 0;
    int lineGap = 0;
    stbtt_GetFontVMetrics(&impl->info, &ascent, &descent, &lineGap);
    font->ascent_ = static_cast<float>(ascent) * font->scale_;
    font->descent_ = static_cast<float>(descent) * font->scale_;
    font->lineHeight_ = static_cast<float>(ascent - descent + lineGap) * font->scale_;
    return font;
}

const Glyph* Font::glyph(char32_t codepoint) {
    if (!impl_) return nullptr;
    if (const auto it = cache_.find(codepoint); it != cache_.end()) return &it->second;

    Glyph glyph;
    int advance = 0;
    int bearing = 0;
    stbtt_GetCodepointHMetrics(&impl_->info, static_cast<int>(codepoint), &advance, &bearing);
    glyph.advance = static_cast<float>(advance) * scale_;

    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    stbtt_GetCodepointBitmapBox(&impl_->info, static_cast<int>(codepoint), scale_, scale_, &x0, &y0,
                                &x1, &y1);
    glyph.width = x1 - x0;
    glyph.height = y1 - y0;
    glyph.bearingX = x0;
    glyph.bearingY = -y0;  // stb measures downwards from the baseline

    if (glyph.width > 0 && glyph.height > 0) {
        glyph.coverage.resize(static_cast<std::size_t>(glyph.width) *
                              static_cast<std::size_t>(glyph.height));
        stbtt_MakeCodepointBitmap(&impl_->info, glyph.coverage.data(), glyph.width, glyph.height,
                                  glyph.width, scale_, scale_, static_cast<int>(codepoint));
        applySynthesis(glyph);
    }

    return &cache_.emplace(codepoint, std::move(glyph)).first->second;
}

void Font::applySynthesis(Glyph& glyph) const {
    const Synthesis& synthesis = impl_->synthesis;
    if (!synthesis.any()) return;

    if (synthesis.embolden > 0.0f) {
        // Dilation in x by a fraction of a pixel: each sample takes the
        // strongest of itself and its neighbours, weighted by how far the
        // stroke should grow. Real bold redraws the outline; this thickens it,
        // which is close enough at UI sizes and wrong at display sizes.
        const int spread = std::max(1, static_cast<int>(std::lround(synthesis.embolden)));
        std::vector<std::uint8_t> widened = glyph.coverage;
        for (int y = 0; y < glyph.height; ++y) {
            const std::size_t row = static_cast<std::size_t>(y) *
                                    static_cast<std::size_t>(glyph.width);
            for (int x = 0; x < glyph.width; ++x) {
                std::uint8_t strongest = glyph.coverage[row + static_cast<std::size_t>(x)];
                for (int dx = 1; dx <= spread; ++dx) {
                    if (x - dx >= 0) {
                        strongest = std::max(strongest,
                                             glyph.coverage[row + static_cast<std::size_t>(x - dx)]);
                    }
                }
                widened[row + static_cast<std::size_t>(x)] = strongest;
            }
        }
        glyph.coverage.swap(widened);
        glyph.advance += synthesis.embolden;
    }

    if (synthesis.slant != 0.0f) {
        // Shearing about the baseline, so the glyph leans without drifting off
        // it. The bitmap grows by whatever the top row is pushed across.
        const float shear = synthesis.slant;
        const int extra = std::max(1, static_cast<int>(std::ceil(
                                          std::fabs(shear) * static_cast<float>(glyph.height))));
        const int width = glyph.width + extra;
        std::vector<std::uint8_t> sheared(static_cast<std::size_t>(width) *
                                              static_cast<std::size_t>(glyph.height),
                                          0);
        for (int y = 0; y < glyph.height; ++y) {
            // Rows above the baseline move right, rows below move left.
            const float fromBaseline = static_cast<float>(glyph.bearingY - y);
            const int shift = static_cast<int>(std::lround(shear * fromBaseline));
            for (int x = 0; x < glyph.width; ++x) {
                const int target = x + shift;
                if (target < 0 || target >= width) continue;
                sheared[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                        static_cast<std::size_t>(target)] =
                    glyph.coverage[static_cast<std::size_t>(y) *
                                       static_cast<std::size_t>(glyph.width) +
                                   static_cast<std::size_t>(x)];
            }
        }
        glyph.coverage.swap(sheared);
        glyph.width = width;
    }
}

float Font::advance(char32_t codepoint) {
    const Glyph* g = glyph(codepoint);
    return g ? g->advance : 0.0f;
}

TextMetrics Font::measure(std::string_view text) {
    float width = 0.0f;
    std::size_t cursor = 0;
    char32_t previous = 0;
    while (cursor < text.size()) {
        const char32_t codepoint = nextCodepoint(text, cursor);
        if (codepoint == 0) break;
        width += advance(codepoint);
        if (previous && impl_) {
            width += static_cast<float>(stbtt_GetCodepointKernAdvance(
                         &impl_->info, static_cast<int>(previous), static_cast<int>(codepoint))) *
                     scale_;
        }
        previous = codepoint;
    }
    return {width, lineHeight_, ascent_};
}

namespace {

/**
 * An environment variable's value, or empty.
 *
 * `std::getenv` is deprecated by MSVC's CRT: the pointer it hands back is
 * invalidated by a `putenv` on another thread. Nothing here sets one, but a
 * warning that has to be argued with on every build is a warning somebody
 * eventually switches off wholesale — so each platform's own call is used
 * instead, and the argument does not have to happen.
 */
std::string environmentVariable(const char* name) {
#if defined(_WIN32)
    char* value = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&value, &size, name) != 0 || value == nullptr) return {};
    std::string out(value);
    std::free(value);
    return out;
#else
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : std::string{};
#endif
}

}  // namespace

FontDatabase::FontDatabase() {
    addSearchPath("/usr/share/fonts");
    addSearchPath("/usr/local/share/fonts");
    const std::string home = environmentVariable("HOME");
    if (!home.empty()) {
        addSearchPath(home + "/.local/share/fonts");
        addSearchPath(home + "/.fonts");
    }
    addSearchPath("C:/Windows/Fonts");
    // Where Windows puts a font installed for one user rather than for the
    // machine — which is most of the fonts a person installs themselves, and
    // therefore most of the ones a theme is likely to name.
    const std::string localAppData = environmentVariable("LOCALAPPDATA");
    if (!localAppData.empty()) addSearchPath(localAppData + "/Microsoft/Windows/Fonts");
    addSearchPath("/System/Library/Fonts");
    addSearchPath("/Library/Fonts");
}

void FontDatabase::clearSearchPaths() {
    searchPaths_.clear();
    resolved_.clear();
    fonts_.clear();
}

void FontDatabase::addSearchPath(const std::string& directory) {
    if (std::filesystem::exists(directory)) searchPaths_.push_back(directory);
    resolved_.clear();
}

bool FontDatabase::addFontFile(std::string_view family, const std::string& path,
                               FontWeight weight, FontSlant slant) {
    if (!std::filesystem::exists(path)) return false;

    Face face;
    face.path = path;
    face.weight = weight;
    face.slant = slant;
    face.familyRank = 0;  // a face named by hand is the family, not a fallback for it

    // Registered under both keys: a caller naming a mono family should not have
    // to know which bucket the lookup will use.
    for (const char* suffix : {"|sans", "|mono"}) {
        registered_[squash(family) + suffix].push_back(face);
    }
    // Anything already resolved for this family is now stale.
    resolved_.erase(squash(family) + "|sans");
    resolved_.erase(squash(family) + "|mono");
    fonts_.clear();
    return true;
}

const std::vector<FontDatabase::Face>& FontDatabase::facesFor(std::string_view family,
                                                              bool monospace) const {
    const std::string key = squash(family) + (monospace ? "|mono" : "|sans");
    if (const auto it = resolved_.find(key); it != resolved_.end()) return it->second;

    // The theme's own family first, then the fallbacks in order.
    std::vector<std::string> wanted{squash(family)};
    for (const char* fallback : (monospace ? kMonoFallbacks : kSansFallbacks)) {
        wanted.push_back(squash(fallback));
    }

    std::vector<Face> faces;
    // Faces the application registered come first, so a bundled file wins over
    // a same-named one installed on the machine.
    if (const auto it = registered_.find(key); it != registered_.end()) {
        faces = it->second;
    }
    for (const auto& root : searchPaths_) {
        std::error_code error;
        for (auto it = std::filesystem::recursive_directory_iterator(
                 root, std::filesystem::directory_options::skip_permission_denied, error);
             it != std::filesystem::recursive_directory_iterator(); it.increment(error)) {
            if (error) break;
            if (!it->is_regular_file(error) || !isFontFile(it->path())) continue;

            const std::string squashed = squash(it->path().stem().string());
            for (std::size_t rank = 0; rank < wanted.size(); ++rank) {
                if (squashed.find(wanted[rank]) == std::string::npos) continue;
                // "NotoSansCJK" contains "NotoSans" but is the wrong script and
                // a 40 MB collection; whatever the name adds beyond the family
                // is a distance, folded into the rank so closer names win.
                const std::string bare = withoutStyleWords(squashed);
                const std::size_t extra =
                    bare.size() > wanted[rank].size() ? bare.size() - wanted[rank].size() : 0;
                Face face;
                face.path = it->path().string();
                face.weight = weightFromName(squashed);
                face.slant = italicFromName(squashed) ? FontSlant::Italic : FontSlant::Normal;
                face.familyRank = rank;
                face.nameDistance = extra + (isWidthVariant(squashed) ? 50 : 0);
                faces.push_back(std::move(face));
                break;
            }
        }
    }

    std::sort(faces.begin(), faces.end(), [](const Face& a, const Face& b) {
        if (a.familyRank != b.familyRank) return a.familyRank < b.familyRank;
        if (a.nameDistance != b.nameDistance) return a.nameDistance < b.nameDistance;
        return a.path < b.path;
    });
    return resolved_.emplace(key, std::move(faces)).first->second;
}

std::vector<std::string> FontDatabase::candidates(std::string_view family,
                                                  bool monospace) const {
    std::vector<std::string> paths;
    for (const Face& face : facesFor(family, monospace)) paths.push_back(face.path);
    return paths;
}

std::string FontDatabase::resolve(std::string_view family, bool monospace) const {
    const std::vector<std::string> paths = candidates(family, monospace);
    return paths.empty() ? std::string{} : paths.front();
}

std::shared_ptr<Font> FontDatabase::font(std::string_view family, bool monospace,
                                         float pixelSize, const FaceRequest& request) {
    char key[128];
    std::snprintf(key, sizeof(key), "%.*s|%c|%.1f|%d|%d", static_cast<int>(family.size()),
                  family.data(), monospace ? 'm' : 's', static_cast<double>(pixelSize),
                  static_cast<int>(request.weight), static_cast<int>(request.slant));
    if (const auto it = fonts_.find(key); it != fonts_.end()) return it->second;

    const std::vector<Face>& faces = facesFor(family, monospace);
    const int wanted = static_cast<int>(request.weight);

    // Score every face, and the *order of the terms is the whole thing*: which
    // family it is beats how well the face matches, which in turn beats how the
    // file happens to be named. Getting that order wrong is not a near miss —
    // when the name outranked the weight, `LiberationSans-Bold` beat
    // `-Regular` on being three characters shorter and every label in the
    // window came out bold.
    //
    // A wrong slant costs more than a wrong weight because a synthetic oblique
    // is more convincing than a synthetic bold: the shear is exactly what a
    // real oblique does, while emboldening only fattens the strokes.
    const Face* best = nullptr;
    long bestScore = 0;
    for (const Face& face : faces) {
        const long weightPenalty = std::abs(wanted - static_cast<int>(face.weight));
        const long slantPenalty = face.slant == request.slant ? 0 : 1000;
        const long score = static_cast<long>(face.familyRank) * 1000000 +
                           (weightPenalty + slantPenalty) * 100 +
                           static_cast<long>(face.nameDistance);
        if (!best || score < bestScore) {
            best = &face;
            bestScore = score;
        }
    }

    std::shared_ptr<Font> font;
    if (best) {
        // Whatever is missing after the best match is made up for.
        Synthesis synthesis;
        const int shortfall = wanted - static_cast<int>(best->weight);
        // Below 150 the difference is not worth faking; above it, the stroke
        // grows with the gap and with the size.
        if (shortfall >= 150) {
            synthesis.embolden = pixelSize * 0.02f * (static_cast<float>(shortfall) / 300.0f);
        }
        if (request.slant == FontSlant::Italic && best->slant == FontSlant::Normal) {
            synthesis.slant = 0.21f;   // about 12 degrees, the usual oblique
        }
        font = Font::load(best->path, pixelSize, synthesis);

        // A face that will not open is not the end: walk down the list.
        if (!font) {
            for (const Face& face : faces) {
                font = Font::load(face.path, pixelSize, synthesis);
                if (font) break;
            }
        }
    }

    fonts_.emplace(key, font);
    return font;
}

/** The size a run resolves to: its own, or the theme's for that role. */
float fontSizeFor(const TextStyle& style, const Typography& typography) {
    if (!isAuto(style.size)) return style.size;
    return style.role == FontRole::Editor ? typography.editorFontSize : typography.uiFontSize;
}

std::shared_ptr<Font> fontFor(FontDatabase& fonts, const TextStyle& style,
                              const Typography& typography, float scale) {
    const bool monospace = style.role != FontRole::Ui;
    const std::string& family = style.role == FontRole::Ui      ? typography.uiFont
                                : style.role == FontRole::Mono  ? typography.monoFont
                                                                : typography.editorFont;
    return fonts.font(family, monospace, fontSizeFor(style, typography) * scale,
                      FaceRequest{style.weight, style.slant});
}

MeasureText measureWith(FontDatabase& fonts, float scale) {
    const float ratio = scale > 0.0f ? scale : 1.0f;
    return [&fonts, ratio](std::string_view text, const TextStyle& style,
                           const Typography& typography, float maxWidth) -> TextMetrics {
        // Rasterised at the device size, reported in logical units. Wrapping
        // therefore breaks at the same words on every display, instead of a
        // 200% screen fitting twice as much on a line.
        const std::shared_ptr<Font> font = fontFor(fonts, style, typography, ratio);
        // No font on this machine: the estimate is still better than nothing,
        // and the layout stays deterministic instead of collapsing to zero.
        if (!font) return approximateTextMetrics(text, style, typography, maxWidth);

        TextMetrics single = font->measure(text);
        single.width /= ratio;
        single.height /= ratio;
        single.baseline /= ratio;
        // A line box may be taller than the face asks for. The extra is split
        // above and below, so the baseline stays centred in it — otherwise a
        // 1.6 line height pushes every run up against the top of its box.
        const float step = style.lineHeight > 0.0f
                               ? style.lineHeight * fontSizeFor(style, typography)
                               : single.height;
        const float baseline = single.baseline + (step - single.height) / 2.0f;
        if (style.overflow != TextOverflow::Wrap) return {single.width, step, baseline};

        // Nothing to break on: no width to break at and no hard break in the
        // text. Returning the raw advance here is not just a shortcut — it is
        // the only correct answer. `wrapText` ends every line at the last
        // *word*, so it reports "a " as exactly as wide as "a", which is right
        // for laying out a wrapped paragraph and wrong for anyone asking how
        // far along a run a given offset sits. The rich editor asks exactly
        // that to place its caret, and a space typed at the end of a line moved
        // it nowhere.
        const bool couldBreak =
            (maxWidth > 0.0f && !std::isinf(maxWidth)) || text.find('\n') != std::string_view::npos;
        if (!couldBreak) return {single.width, step, baseline};

        const WrappedText wrapped =
            wrapText(text, maxWidth, style.maxLines,
                     [&](std::string_view run) { return font->measure(run).width / ratio; },
                     style.wordBreak);
        return {wrapped.widest, static_cast<float>(wrapped.lines.size()) * step, baseline};
    };
}

}  // namespace gbui
