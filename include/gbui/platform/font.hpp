// Text, for real.
//
// The layout engine takes measurement as a callback precisely so this can exist
// outside it: a FontDatabase resolves a family name to a file on disk, a Font
// shapes and rasterises with stb_truetype, and `measureWith` adapts it to the
// signature `LayoutContext` expects.
//
// This is a platform module. Nothing in core, layout or paint links against it,
// and the SVG backend still runs without it.
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "gbui/layout/layout.hpp"
#include "gbui/style/style.hpp"

namespace gbui {

/** A rasterised glyph, in 8-bit coverage. */
struct Glyph {
    int width = 0;
    int height = 0;
    int bearingX = 0;  ///< left edge relative to the pen
    int bearingY = 0;  ///< top edge relative to the baseline, positive upwards
    float advance = 0.0f;
    std::vector<std::uint8_t> coverage;  ///< width * height, row-major
};

/** What a run asks a face to be. */
struct FaceRequest {
    FontWeight weight = FontWeight::Regular;
    FontSlant slant = FontSlant::Normal;
};

/** What the toolkit does when no file matches the request.
 *
 * Real faces are always preferred: a designed bold is not a thickened regular,
 * and a true italic is not a sheared one. But a UI that silently draws its
 * headings at regular weight because the machine lacks a Bold file is worse
 * than one that emboldens, so synthesis is the fallback rather than the plan. */
struct Synthesis {
    /** Dilates the coverage to fake weight. */
    float embolden = 0.0f;
    /** Shears the glyph to fake an italic, in x per unit of y. */
    float slant = 0.0f;

    bool any() const { return embolden > 0.0f || slant != 0.0f; }
};

/** One face at one size. Glyphs are rasterised on first use and cached. */
class Font {
public:
    static std::shared_ptr<Font> load(const std::string& path, float pixelSize,
                                      const Synthesis& synthesis = {});

    const Glyph* glyph(char32_t codepoint);
    float advance(char32_t codepoint);

    float ascent() const { return ascent_; }
    float descent() const { return descent_; }
    float lineHeight() const { return lineHeight_; }

    /** Width of a run, and the metrics a line box needs. */
    TextMetrics measure(std::string_view text);

private:
    /** Thickens or shears a rasterised glyph, when the machine had no face. */
    void applySynthesis(Glyph& glyph) const;

    struct Impl;
    std::shared_ptr<Impl> impl_;
    std::map<char32_t, Glyph> cache_;
    float scale_ = 1.0f;
    float ascent_ = 0.0f;
    float descent_ = 0.0f;
    float lineHeight_ = 0.0f;
};

/**
 * Resolves the family names a theme asks for — "IBM Plex Sans", "IBM Plex Mono"
 * — to files that exist on this machine, falling back through a list of common
 * families so a theme naming a font nobody has still renders.
 */
class FontDatabase {
public:
    FontDatabase();

    /** Adds a directory to search. /usr/share/fonts and ~/.local/share/fonts
     *  are already in the list. */
    void addSearchPath(const std::string& directory);

    /**
     * Forgets every directory to look in, including the platform's own.
     *
     * For an application that ships its faces and wants *only* those: with no
     * system paths left, the same files are found on every machine and a window
     * drawn on Linux, Windows and macOS matches to the pixel. Call it before
     * `addFontFile` or `addSearchPath`, since it drops what is already there.
     */
    void clearSearchPaths();

    /**
     * Registers a font file under a family name — CSS's `@font-face`.
     *
     * This is what makes an application look the same on three platforms: a
     * family resolved from the machine is whatever that machine happens to
     * have, and the metrics of the substitute are not the metrics of the
     * design. Shipping the face and naming it here removes the substitution
     * entirely.
     *
     * Registered faces are searched *before* anything installed, so a bundled
     * "Inter" wins over a system one, and a family can be built up a weight at
     * a time the way `@font-face` does:
     *
     *     fonts.addFontFile("Inter", "assets/Inter-Regular.ttf");
     *     fonts.addFontFile("Inter", "assets/Inter-SemiBold.ttf", FontWeight::SemiBold);
     *     fonts.addFontFile("Inter", "assets/Inter-Italic.ttf", FontWeight::Regular,
     *                       FontSlant::Italic);
     *
     * Returns false when the file does not exist, so a caller can fall back
     * rather than render nothing.
     */
    bool addFontFile(std::string_view family, const std::string& path,
                     FontWeight weight = FontWeight::Regular,
                     FontSlant slant = FontSlant::Normal);

    /** Files that could serve a family, best first. More than one is returned
     *  because a candidate can be unloadable — a collection, a variable font
     *  stb cannot open — and the caller keeps trying down the list. */
    std::vector<std::string> candidates(std::string_view family, bool monospace) const;

    /** What a file's name says it is. Parsed once and cached with the path. */
    struct Face {
        std::string path;
        FontWeight weight = FontWeight::Regular;
        FontSlant slant = FontSlant::Normal;
        /** Position of the family it matched, in the fallback list. */
        std::size_t familyRank = 0;
        /**
         * How much name the file has beyond the family it matched, *once the
         * style words are taken out* — "NotoSansCJK" is 3 away from "NotoSans"
         * and is a different family; "NotoSans-Bold" is 0 away and is the same
         * one. Only ever a tiebreak: it separates families, never faces, and a
         * weight or slant that actually matches outranks it.
         */
        std::size_t nameDistance = 0;
    };

    /** Every face that could serve the family, unranked by request. */
    const std::vector<Face>& facesFor(std::string_view family, bool monospace) const;

    /** The first candidate that exists, or an empty string. Convenience for
     *  diagnostics; `font()` is what callers want. */
    std::string resolve(std::string_view family, bool monospace) const;

    /** The closest face to the request, synthesising what the machine lacks. */
    std::shared_ptr<Font> font(std::string_view family, bool monospace, float pixelSize,
                               const FaceRequest& request = {});

private:
    std::vector<std::string> searchPaths_;
    /** Faces registered by hand, keyed the same way `resolved_` is. Consulted
     *  first, and never evicted by a rescan. */
    std::map<std::string, std::vector<Face>> registered_;
    mutable std::map<std::string, std::vector<Face>> resolved_;
    std::map<std::string, std::shared_ptr<Font>> fonts_;
};

/**
 * Builds the callback `LayoutContext::measure` wants from a database. This is
 * the seam that turns "laid out about right" into text that measures the way
 * it will draw.
 *
 * `scale` is the display's device pixels per logical pixel. The face is
 * rasterised at the *device* size — which is the whole point of a HiDPI
 * display, and what makes the glyphs sharp rather than a magnified 13-pixel
 * bitmap — and the metrics are divided back down, so layout keeps working in
 * logical units and never learns about the display at all.
 */
MeasureText measureWith(FontDatabase& fonts, float scale = 1.0f);

/** The font a run resolves to, so the painter and the layout agree. */
/** The face a run resolves to, at `size * scale` device pixels. */
std::shared_ptr<Font> fontFor(FontDatabase& fonts, const TextStyle& style,
                              const Typography& typography, float scale = 1.0f);

/** Decodes UTF-8 one code point at a time. Exposed because both the measurer
 *  and the rasteriser walk text the same way. */
char32_t nextCodepoint(std::string_view text, std::size_t& cursor);

}  // namespace gbui
