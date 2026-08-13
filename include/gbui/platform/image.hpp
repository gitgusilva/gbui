// Turning a file into pixels.
//
// The other half of `gbui/core/image.hpp`, and deliberately on this side of the
// line: `Bitmap` is what the toolkit draws, this is what a decoder produces,
// and everything above `platform/` stays free of both the decoder and the
// filesystem. It is the same division the font module works under — text is
// laid out by the toolkit and turned into glyphs down here.
//
// PNG, JPEG, BMP, GIF, TGA, PSD, HDR, PIC and PNM, through the vendored
// `stb_image.h`. Whatever it can read, this can read.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "gbui/core/image.hpp"

namespace gbui {

/**
 * Decoded pixels, owned.
 *
 * `Bitmap` borrows and this holds — which is the whole reason both exist. A
 * picture is decoded once and kept for as long as it is drawn, so an
 * application puts one of these in a member or a cache and hands `bitmap()` to
 * the widget every frame.
 *
 * Always eight-bit RGBA, whatever the file was: a caller drawing a picture does
 * not want to know whether it happened to be greyscale.
 */
class Image {
public:
    Image() = default;

    bool valid() const { return !pixels_.empty() && width_ > 0 && height_ > 0; }
    int width() const { return width_; }
    int height() const { return height_; }

    /** A view of the pixels, for `image()` and anything else that draws. Valid
     *  only while this object is. */
    Bitmap bitmap() const {
        return valid() ? Bitmap{pixels_.data(), width_, height_, 0} : Bitmap{};
    }

    /** Why the last load failed, or empty. Worth keeping: "the picture did not
     *  appear" has a dozen causes and the decoder knows which one it was. */
    const std::string& error() const { return error_; }

    /** Reads and decodes a file. */
    static Image fromFile(const std::string& path);
    /** Decodes bytes already in hand — a resource compiled into the binary, a
     *  download, a blob out of a database. */
    static Image fromMemory(const std::uint8_t* data, std::size_t size);

private:
    std::vector<std::uint8_t> pixels_;
    int width_ = 0;
    int height_ = 0;
    std::string error_;
};

}  // namespace gbui
