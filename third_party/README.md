# Vendored code

| File | Origin | Licence |
| --- | --- | --- |
| `stb_truetype.h` | [nothings/stb](https://github.com/nothings/stb) v1.26 | public domain (or MIT) |
| `stb_image.h` | [nothings/stb](https://github.com/nothings/stb) v2.30 | public domain (or MIT) |

`stb_truetype.h` rasterises glyphs for the `platform/font` module and
`stb_image.h` decodes pictures for `platform/image`. Both are vendored rather
than fetched so a checkout builds offline, and each is compiled in exactly one
translation unit — `src/platform/stbTruetypeImpl.cpp`, `src/platform/stbImageImpl.cpp` —
with warnings disabled, because third-party code was not written for this
project's flags.

Nothing outside `platform/` may include it: core, style, scene, layout, paint
and widgets stay free of third-party code.
