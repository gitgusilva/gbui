// Which file a request for a family, a weight and a slant actually opens.
#include "gbui/platform/font.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "harness.hpp"

using namespace gbui;

namespace {

/** A directory of empty files named like real font files.
 *
 * The names are the whole point — this loader decides what a file holds by
 * reading its name — so the files need no contents, and `facesFor` never opens
 * them. `font()` would, which is why these tests ask `facesFor` and score the
 * faces the same way it does rather than loading anything. */
struct FakeFontDir {
    std::filesystem::path root;

    explicit FakeFontDir(std::string_view name) {
        root = std::filesystem::temp_directory_path() / ("gbui-fonts-" + std::string(name));
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);
    }
    ~FakeFontDir() { std::filesystem::remove_all(root); }

    void add(std::string_view fileName) const { std::ofstream(root / fileName) << "x"; }
};

/** The face `font()` would pick, chosen by the same rule and without opening
 *  anything. Returns the file name alone, which is what a test wants to read. */
std::string picked(const FontDatabase& fonts, std::string_view family, FontWeight weight,
                   FontSlant slant) {
    const std::vector<FontDatabase::Face>& faces = fonts.facesFor(family, false);
    const FontDatabase::Face* best = nullptr;
    long bestScore = 0;
    for (const FontDatabase::Face& face : faces) {
        const long weightPenalty =
            std::abs(static_cast<int>(weight) - static_cast<int>(face.weight));
        const long slantPenalty = face.slant == slant ? 0 : 1000;
        const long score = static_cast<long>(face.familyRank) * 1000000 +
                           (weightPenalty + slantPenalty) * 100 +
                           static_cast<long>(face.nameDistance);
        if (!best || score < bestScore) {
            best = &face;
            bestScore = score;
        }
    }
    return best ? std::filesystem::path(best->path).filename().string() : std::string{};
}

}  // namespace

/**
 * The regression this file exists for.
 *
 * The name distance used to be folded into the family rank and multiplied above
 * the weight penalty, so `Acme-Bold.ttf` — three characters shorter than
 * `Acme-Regular.ttf` — scored better as a *regular* face than the regular one
 * did, and every unstyled label in the window came out bold.
 */
TEST("a request for regular does not land on the bold file") {
    FakeFontDir dir{"weights"};
    dir.add("Acme-Regular.ttf");
    dir.add("Acme-Bold.ttf");
    dir.add("Acme-Italic.ttf");
    dir.add("Acme-BoldItalic.ttf");
    dir.add("Acme-Light.ttf");

    FontDatabase fonts;
    fonts.clearSearchPaths();
    fonts.addSearchPath(dir.root.string());

    CHECK(picked(fonts, "Acme", FontWeight::Regular, FontSlant::Normal) == "Acme-Regular.ttf");
    CHECK(picked(fonts, "Acme", FontWeight::Bold, FontSlant::Normal) == "Acme-Bold.ttf");
    CHECK(picked(fonts, "Acme", FontWeight::Light, FontSlant::Normal) == "Acme-Light.ttf");
    CHECK(picked(fonts, "Acme", FontWeight::Regular, FontSlant::Italic) == "Acme-Italic.ttf");
    CHECK(picked(fonts, "Acme", FontWeight::Bold, FontSlant::Italic) == "Acme-BoldItalic.ttf");
}

TEST("a weight with no file of its own takes the nearest one") {
    FakeFontDir dir{"nearest"};
    dir.add("Acme-Regular.ttf");
    dir.add("Acme-Bold.ttf");

    FontDatabase fonts;
    fonts.clearSearchPaths();
    fonts.addSearchPath(dir.root.string());

    // Medium is 100 from Regular and 200 from Bold, so it rounds down rather
    // than emboldening — synthetic weight is the last resort, not the first.
    CHECK(picked(fonts, "Acme", FontWeight::Medium, FontSlant::Normal) == "Acme-Regular.ttf");
    CHECK(picked(fonts, "Acme", FontWeight::SemiBold, FontSlant::Normal) == "Acme-Bold.ttf");
}

/** A family whose name is a prefix of another's must not swallow it: the
 *  distance is measured on what is left after the style words come out, so
 *  "Sans" and "SansCJK" stay apart while "Sans-Bold" stays home. */
TEST("a longer family name is a different family, a style word is not") {
    FakeFontDir dir{"prefix"};
    dir.add("AcmeSans-Regular.ttf");
    dir.add("AcmeSans-Bold.ttf");
    dir.add("AcmeSansCJK-Regular.ttf");

    FontDatabase fonts;
    fonts.clearSearchPaths();
    fonts.addSearchPath(dir.root.string());

    CHECK(picked(fonts, "AcmeSans", FontWeight::Regular, FontSlant::Normal) ==
          "AcmeSans-Regular.ttf");
    CHECK(picked(fonts, "AcmeSans", FontWeight::Bold, FontSlant::Normal) == "AcmeSans-Bold.ttf");
    CHECK(picked(fonts, "AcmeSansCJK", FontWeight::Regular, FontSlant::Normal) ==
          "AcmeSansCJK-Regular.ttf");
}

/** A family that is not installed falls through to the fallback list rather
 *  than to nothing. */
TEST("a missing family falls back instead of failing") {
    FakeFontDir dir{"fallback"};
    dir.add("DejaVuSans.ttf");

    FontDatabase fonts;
    fonts.clearSearchPaths();
    fonts.addSearchPath(dir.root.string());

    CHECK(picked(fonts, "NoSuchFamilyAnywhere", FontWeight::Regular, FontSlant::Normal) ==
          "DejaVuSans.ttf");
}

/** Condensed is a different width, not a different weight, and a UI that asked
 *  for neither should not get it when a plain face is on the machine. */
TEST("a width variant loses to the plain face") {
    FakeFontDir dir{"width"};
    dir.add("Acme-Condensed.ttf");
    dir.add("Acme-Regular.ttf");

    FontDatabase fonts;
    fonts.clearSearchPaths();
    fonts.addSearchPath(dir.root.string());

    CHECK(picked(fonts, "Acme", FontWeight::Regular, FontSlant::Normal) == "Acme-Regular.ttf");
}
