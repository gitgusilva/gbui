# Test images

Four pictures, six by four pixels each, under a kilobyte all together. They are
here so `Image::fromFile` is tested against *files* — a decoder exercised only
through a byte array in a header is a decoder nobody has watched read a file,
and the formats it claims to support are a claim until one of each has been
opened.

| File | What it proves |
| --- | --- |
| `swatch.png` | PNG, the format everything ships logos in |
| `swatch.jpg` | JPEG, whose pixels come back *close* rather than exact |
| `swatch.bmp` | a third format, and one with no compression to hide behind |
| `hole.png` | alpha: part of it is transparent, and has to stay that way |

Written by ImageMagick rather than by this project, which is the point: a
decoder tested only against files the same repository also writes agrees with
itself and proves nothing.
