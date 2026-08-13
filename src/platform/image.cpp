#include "gbui/platform/image.hpp"

#include <fstream>

#include "stb_image.h"

namespace gbui {

namespace {

/** The whole file, or nothing. Read in one go rather than streamed: a decoder
 *  wants the bytes, and a UI's images are kilobytes. */
std::vector<std::uint8_t> readFile(const std::string& path, std::string& error) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        error = "cannot open " + path;
        return {};
    }
    const std::streamsize size = file.tellg();
    if (size <= 0) {
        error = path + " is empty";
        return {};
    }
    file.seekg(0);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if (!file.read(reinterpret_cast<char*>(bytes.data()), size)) {
        error = "cannot read " + path;
        return {};
    }
    return bytes;
}

}  // namespace

Image Image::fromMemory(const std::uint8_t* data, std::size_t size) {
    Image out;
    if (!data || size == 0) {
        out.error_ = "no data";
        return out;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    // Four channels whatever the file has: a caller drawing a picture does not
    // want to find out it was greyscale.
    stbi_uc* pixels = stbi_load_from_memory(data, static_cast<int>(size), &width, &height,
                                            &channels, 4);
    if (!pixels) {
        const char* why = stbi_failure_reason();
        out.error_ = why ? why : "cannot decode";
        return out;
    }

    const auto bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
    out.pixels_.assign(pixels, pixels + bytes);
    out.width_ = width;
    out.height_ = height;
    stbi_image_free(pixels);
    return out;
}

Image Image::fromFile(const std::string& path) {
    Image out;
    std::string error;
    const std::vector<std::uint8_t> bytes = readFile(path, error);
    if (bytes.empty()) {
        out.error_ = error;
        return out;
    }
    out = fromMemory(bytes.data(), bytes.size());
    // The decoder's complaint is about the bytes; the path is what the reader
    // needs to know which file they came from.
    if (!out.valid()) out.error_ = path + ": " + out.error_;
    return out;
}

}  // namespace gbui
