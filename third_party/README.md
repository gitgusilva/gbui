# Vendored code

| File | Origin | Licence |
| --- | --- | --- |
| `stb_truetype.h` | [nothings/stb](https://github.com/nothings/stb) v1.26 | public domain (or MIT) |

`stb_truetype.h` rasterises glyphs for the `platform/font` module. It is
vendored rather than fetched so a checkout builds offline, and it is compiled in
exactly one translation unit — `src/platform/stb_truetype_impl.cpp` — with
warnings disabled, because third-party code was not written for this project's
flags.

Nothing outside `platform/` may include it: core, style, scene, layout, paint
and widgets stay free of third-party code.
