// Where a picture lands, and what it puts down when it gets there.
#include "gbui/core/image.hpp"

#include <array>
#include <string>

#include "gbui/paint/canvas.hpp"
#include "gbui/platform/image.hpp"
#include "harness.hpp"

using namespace gbui;

TEST("contain fits the whole picture and centres what is left over") {
    // A square into a wide box: as tall as the box, centred sideways.
    const Rect box{0, 0, 200, 100};
    const Rect out = fitted(box, {50, 50}, ImageFit::Contain);
    CHECK_NEAR(out.width, 100.0f);
    CHECK_NEAR(out.height, 100.0f);
    CHECK_NEAR(out.x, 50.0f);
    CHECK_NEAR(out.y, 0.0f);
}

/** The difference that matters: `Cover` reaches outside the box, and the caller
 *  is expected to clip. A fit that never overflowed would be `Contain` again. */
TEST("cover fills the box and hangs over the edges") {
    const Rect out = fitted(Rect{0, 0, 200, 100}, {50, 50}, ImageFit::Cover);
    CHECK_NEAR(out.width, 200.0f);
    CHECK_NEAR(out.height, 200.0f);
    CHECK_NEAR(out.y, -50.0f);
}

TEST("fill takes the box and none keeps its own size") {
    const Rect box{10, 20, 200, 100};
    CHECK_NEAR(fitted(box, {50, 50}, ImageFit::Fill).width, 200.0f);
    CHECK_NEAR(fitted(box, {50, 50}, ImageFit::Fill).height, 100.0f);

    const Rect own = fitted(box, {50, 50}, ImageFit::None);
    CHECK_NEAR(own.width, 50.0f);
    CHECK_NEAR(own.height, 50.0f);
    // Centred, which is the only placement that means anything without a fit.
    CHECK_NEAR(own.x, 85.0f);
    CHECK_NEAR(own.y, 45.0f);
}

TEST("a picture with no size is not resized into nothing") {
    const Rect box{0, 0, 40, 40};
    CHECK_NEAR(fitted(box, {0, 0}, ImageFit::Contain).width, 40.0f);
}

namespace {

/** A 2x2 picture: red, green on the top row; blue and a hole on the bottom. */
struct Tiny {
    std::array<std::uint8_t, 16> pixels{
        255, 0, 0, 255,   0, 255, 0, 255,
        0, 0, 255, 255,   0, 0, 0, 0,
    };
    Bitmap bitmap() { return {pixels.data(), 2, 2, 0}; }
};

Color at(const Canvas& canvas, int x, int y) {
    const std::uint8_t* p = canvas.pixels() + static_cast<std::size_t>(y) * canvas.pitch() +
                            static_cast<std::size_t>(x) * 4;
    return Color{p[0], p[1], p[2], static_cast<float>(p[3]) / 255.0f};
}

}  // namespace

/**
 * Drawn at four times its size, each corner of the destination is squarely
 * inside one source pixel — which is what says the sampler maps pixel centres
 * to pixel centres rather than drifting half a texel towards the origin.
 */
TEST("a picture lands the right way up") {
    Canvas canvas;
    canvas.resize(8, 8);
    canvas.clear(Color{0, 0, 0, 1.0f});
    Tiny tiny;
    canvas.blitImage(Rect{0, 0, 8, 8}, tiny.bitmap(), 0.0f, 1.0f, Clip{Rect{0, 0, 8, 8}});

    CHECK(at(canvas, 0, 0).r > 200);   // red, top left
    CHECK(at(canvas, 0, 0).g < 60);
    CHECK(at(canvas, 7, 0).g > 200);   // green, top right
    CHECK(at(canvas, 0, 7).b > 200);   // blue, bottom left
}

/** The hole in the corner is transparent, so what was under it is what stays.
 *  A blit that ignored alpha would have painted it black. */
TEST("a transparent pixel leaves what is behind it") {
    Canvas canvas;
    canvas.resize(8, 8);
    canvas.clear(Color{255, 255, 255, 1.0f});
    Tiny tiny;
    canvas.blitImage(Rect{0, 0, 8, 8}, tiny.bitmap(), 0.0f, 1.0f, Clip{Rect{0, 0, 8, 8}});
    const Color corner = at(canvas, 7, 7);
    CHECK(corner.r > 200);
    CHECK(corner.g > 200);
    CHECK(corner.b > 200);
}

TEST("opacity scales the whole picture") {
    Canvas canvas;
    canvas.resize(8, 8);
    canvas.clear(Color{0, 0, 0, 1.0f});
    Tiny tiny;
    canvas.blitImage(Rect{0, 0, 8, 8}, tiny.bitmap(), 0.0f, 0.5f, Clip{Rect{0, 0, 8, 8}});
    // Half of red over black is darker than red and lighter than black.
    const Color corner = at(canvas, 0, 0);
    CHECK(corner.r > 40);
    CHECK(corner.r < 220);
}

// ---------------------------------------------------------------------------
// Decoding
// ---------------------------------------------------------------------------

/**
 * A real PNG, byte for byte — two pixels, written by another tool.
 *
 * Another tool's on purpose: a decoder tested only against files this project
 * also wrote agrees with itself and proves nothing. This one arrives with the
 * ancillary chunks a real encoder leaves behind — gamma, a timestamp, three
 * text records — which a reader has to walk past to reach the pixels.
 */
TEST("a png decodes to the pixels it says") {
    static const std::uint8_t kDot[] = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D,
        0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x06, 0x00, 0x00, 0x00, 0xF4, 0x22, 0x7F, 0x8A, 0x00, 0x00, 0x00,
        0x20, 0x63, 0x48, 0x52, 0x4D, 0x00, 0x00, 0x7A, 0x26, 0x00, 0x00, 0x80,
        0x84, 0x00, 0x00, 0xFA, 0x00, 0x00, 0x00, 0x80, 0xE8, 0x00, 0x00, 0x75,
        0x30, 0x00, 0x00, 0xEA, 0x60, 0x00, 0x00, 0x3A, 0x98, 0x00, 0x00, 0x17,
        0x70, 0x9C, 0xBA, 0x51, 0x3C, 0x00, 0x00, 0x00, 0x06, 0x62, 0x4B, 0x47,
        0x44, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xA0, 0xBD, 0xA7, 0x93, 0x00,
        0x00, 0x00, 0x07, 0x74, 0x49, 0x4D, 0x45, 0x07, 0xEA, 0x08, 0x0D, 0x0E,
        0x2D, 0x11, 0xF9, 0x15, 0x56, 0x4F, 0x00, 0x00, 0x00, 0x25, 0x74, 0x45,
        0x58, 0x74, 0x64, 0x61, 0x74, 0x65, 0x3A, 0x63, 0x72, 0x65, 0x61, 0x74,
        0x65, 0x00, 0x32, 0x30, 0x32, 0x36, 0x2D, 0x30, 0x38, 0x2D, 0x31, 0x33,
        0x54, 0x31, 0x34, 0x3A, 0x34, 0x35, 0x3A, 0x31, 0x37, 0x2B, 0x30, 0x30,
        0x3A, 0x30, 0x30, 0xDB, 0xE9, 0x53, 0x70, 0x00, 0x00, 0x00, 0x25, 0x74,
        0x45, 0x58, 0x74, 0x64, 0x61, 0x74, 0x65, 0x3A, 0x6D, 0x6F, 0x64, 0x69,
        0x66, 0x79, 0x00, 0x32, 0x30, 0x32, 0x36, 0x2D, 0x30, 0x38, 0x2D, 0x31,
        0x33, 0x54, 0x31, 0x34, 0x3A, 0x34, 0x35, 0x3A, 0x31, 0x37, 0x2B, 0x30,
        0x30, 0x3A, 0x30, 0x30, 0xAA, 0xB4, 0xEB, 0xCC, 0x00, 0x00, 0x00, 0x28,
        0x74, 0x45, 0x58, 0x74, 0x64, 0x61, 0x74, 0x65, 0x3A, 0x74, 0x69, 0x6D,
        0x65, 0x73, 0x74, 0x61, 0x6D, 0x70, 0x00, 0x32, 0x30, 0x32, 0x36, 0x2D,
        0x30, 0x38, 0x2D, 0x31, 0x33, 0x54, 0x31, 0x34, 0x3A, 0x34, 0x35, 0x3A,
        0x31, 0x37, 0x2B, 0x30, 0x30, 0x3A, 0x30, 0x30, 0xFD, 0xA1, 0xCA, 0x13,
        0x00, 0x00, 0x00, 0x11, 0x49, 0x44, 0x41, 0x54, 0x08, 0xD7, 0x63, 0xF8,
        0xCF, 0xC0, 0xF0, 0x9F, 0x81, 0xE1, 0x7F, 0x03, 0x00, 0x0F, 0x7A, 0x03,
        0x7E, 0x1B, 0xDA, 0x3D, 0x85, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E,
        0x44, 0xAE, 0x42, 0x60, 0x82,
    };
    const Image image = Image::fromMemory(kDot, sizeof(kDot));
    CHECK(image.valid());
    CHECK(image.width() == 2);
    CHECK(image.height() == 1);

    const Bitmap map = image.bitmap();
    CHECK(map.pixels[0] > 200);    // an opaque red pixel
    CHECK(map.pixels[1] < 60);
    CHECK(map.pixels[3] == 255);
    // And a half-transparent blue one, which is the part that matters: alpha
    // survives the trip, and it is not premultiplied on the way through.
    CHECK(map.pixels[6] > 200);
    CHECK(map.pixels[7] > 60);
    CHECK(map.pixels[7] < 220);
}

/** A failure has to say why. "The picture did not appear" has a dozen causes
 *  and the decoder is the only thing that knows which one it was. */
TEST("rubbish decodes to nothing, with a reason") {
    const std::uint8_t junk[] = {1, 2, 3, 4, 5, 6, 7, 8};
    const Image image = Image::fromMemory(junk, sizeof(junk));
    CHECK(!image.valid());
    CHECK(!image.error().empty());
    CHECK(!image.bitmap().valid());
}

TEST("a file that is not there says so, and names itself") {
    const Image image = Image::fromFile("no/such/picture.png");
    CHECK(!image.valid());
    CHECK(image.error().find("no/such/picture.png") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Files
// ---------------------------------------------------------------------------
//
// `tests/data` holds four six-by-four pictures written by ImageMagick. A
// decoder exercised only through a byte array in a header is one nobody has
// watched read a file, and the formats it claims are a claim until one of each
// has been opened.

namespace {

std::string dataPath(const char* name) { return std::string(GBUI_TEST_DATA) + "/" + name; }

}  // namespace

TEST("a png on disk decodes") {
    const Image image = Image::fromFile(dataPath("swatch.png"));
    CHECK(image.error().empty());
    CHECK(image.valid());
    CHECK(image.width() == 6);
    CHECK(image.height() == 4);
    // A gradient from the accent blue to a green: blue leads at one end and
    // green at the other, which is a claim about the *picture* rather than
    // about a byte.
    const Bitmap map = image.bitmap();
    const std::uint8_t* first = map.pixels;
    const std::uint8_t* last = map.pixels + static_cast<std::size_t>(6 * 4 - 1) * 4;
    CHECK(first[2] > first[1]);
    CHECK(last[1] > last[2]);
    CHECK(first[3] == 255);
}

/** JPEG, which is the one whose pixels come back *close* rather than exact —
 *  so the test asks about the shape of the picture, not about a byte. */
TEST("a jpeg on disk decodes") {
    const Image image = Image::fromFile(dataPath("swatch.jpg"));
    CHECK(image.error().empty());
    CHECK(image.valid());
    CHECK(image.width() == 6);
    CHECK(image.height() == 4);
    CHECK(image.bitmap().pixels[3] == 255);   // opaque, whatever the file had
}

TEST("a bmp on disk decodes") {
    const Image image = Image::fromFile(dataPath("swatch.bmp"));
    CHECK(image.valid());
    CHECK(image.width() == 6);
    CHECK(image.height() == 4);
}

/** Transparency survives the trip. A decoder that dropped alpha would put an
 *  opaque box around every logo, which is the failure nobody notices until it
 *  is on a dark background. */
TEST("a picture with a transparent part keeps it") {
    const Image image = Image::fromFile(dataPath("hole.png"));
    CHECK(image.valid());
    const Bitmap map = image.bitmap();
    CHECK(map.pixels[3] == 255);          // the drawn part, opaque
    CHECK(map.pixels[5 * 4 + 3] == 0);    // the rest of the row, not there at all
}

/** And the whole way through: read from a file, decoded, drawn by the painter. */
TEST("a decoded picture can be drawn") {
    const Image image = Image::fromFile(dataPath("swatch.png"));
    CHECK(image.valid());
    Canvas canvas;
    canvas.resize(12, 8);
    canvas.clear(Color{0, 0, 0, 1.0f});
    canvas.blitImage(Rect{0, 0, 12, 8}, image.bitmap(), 0.0f, 1.0f, Clip{Rect{0, 0, 12, 8}});
    CHECK(at(canvas, 1, 1).b > 60);
}
