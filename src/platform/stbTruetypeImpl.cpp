// The one place stb_truetype is compiled. Kept apart from font.cpp so the
// library can build with -Wconversion -Werror without third-party code, which
// was never written for those flags, deciding otherwise.
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
