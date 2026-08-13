// The one place stb_image is compiled. Kept apart from image.cpp for the same
// reason stbTruetypeImpl.cpp is kept apart from font.cpp: the library builds
// with -Wconversion -Werror, and third-party code was not written for that.
//
// Only the formats a UI actually meets are enabled — the rest are decoders
// nobody here will call, compiled into every binary that links this.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO          // the file is read by image.cpp, which reports why not
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM
#include "stb_image.h"
