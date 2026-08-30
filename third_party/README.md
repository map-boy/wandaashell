# third_party

Vendored dependencies, unmodified.

## miniaudio.h

- Version: 0.11.25 (2026-03-04)
- Upstream: https://github.com/mackron/miniaudio
- License: public domain (Unlicense) or MIT-0, at your option. Full text is at
  the bottom of `miniaudio.h`.

Single-header audio playback library. wandaashell uses it for the one thing it
needs - playing a bundled MP3 without blocking - on Windows, Linux and macOS
alike, which is why there is no per-OS audio backend in the platform layer.
The MP3 decoder (dr_mp3) is bundled inside miniaudio itself, so there is no
second dependency.

Update procedure: replace the file wholesale from upstream and rebuild on all
three desktop platforms. Do not patch it locally - keeping it byte-identical to
a tagged upstream release is the point of vendoring it.
