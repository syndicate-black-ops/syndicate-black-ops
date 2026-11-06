# Video player libraries (vendored)

Used by the client's video player (`src/cl_dll/video/`). Plain copies of the release files the
decoder needs; nothing is downloaded at build time. Built static into client.dll / client.so by
`src/cl_dll/video/video.cmake`.

| Folder | Upstream | Version | Files kept | License |
|---|---|---|---|---|
| `libogg/` | https://github.com/xiph/ogg | v1.3.6 (be05b13) | `src/bitwise.c`, `src/framing.c`, `src/crctable.h`, `include/ogg/{ogg,os_types}.h`; `include/ogg/config_types.h` written for this copy (stdint types) | BSD-3-Clause (`COPYING`) |
| `libtheora/` | https://github.com/xiph/theora | v1.2.0 (8e48087) | the decoder sources of `lib/` (no encoder, no legacy API wrapper, no assembly), `include/theora/{codec,theora,theoradec}.h` | BSD-3-Clause (`COPYING`, `LICENSE`) |
| `stb_vorbis/` | https://github.com/nothings/stb | stb_vorbis.c v1.22 (stb d14d46f) | `stb_vorbis.c` | public domain or MIT (end of the file) |

Updating: replace the files with the same ones from a newer release, keep `config_types.h`.
