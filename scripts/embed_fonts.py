#!/usr/bin/env python3
"""Embed the built-in JetBrains Mono faces into src/Gui/ofxOceanodeEmbeddedFontData.cpp.

The fonts are compressed with the same stb_compress stream format that ImGui's
binary_to_compressed_c tool produces, so ImFontAtlas::AddFontFromMemoryCompressedTTF
decodes them. A Python decoder mirroring imgui.cpp's stb_decompress round-trips every
font before anything is written.

    python3 scripts/embed_fonts.py <JetBrainsMono-2.304 folder>

The folder is the upstream release (fonts/ttf/*.ttf plus OFL.txt), for example
bin/data/config/font/JetBrainsMono-2.304 of an app. JetBrains Mono is licensed
under the SIL Open Font License 1.1; the licence text is copied to
licenses/JetBrainsMono-OFL.txt.
"""
import pathlib, shutil, sys, zlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
FACES = [("JetBrainsMonoMedium", "JetBrainsMono-Medium.ttf"),
         ("JetBrainsMonoExtraBold", "JetBrainsMono-ExtraBold.ttf")]
WINDOW = 1 << 19  # match distances must fit the 0x10/0x18 opcodes


def compress(data: bytes) -> bytes:
    """Greedy LZ77 in stb_compress's token format (see stb_decompress_token in imgui.cpp)."""
    out = bytearray(b"\x57\xbc\x00\x00" + (0).to_bytes(4, "big") + len(data).to_bytes(4, "big")
                    + WINDOW.to_bytes(4, "big"))
    lit = bytearray()

    def flush_literals():
        nonlocal lit
        pos = 0
        while pos < len(lit):
            n = min(65536, len(lit) - pos)
            chunk = lit[pos:pos + n]
            if n <= 32:
                out.append(0x20 + n - 1)
            elif n <= 2048:
                out.extend((0x0800 + n - 1).to_bytes(2, "big"))
            else:
                out.append(0x07); out.extend((n - 1).to_bytes(2, "big"))
            out.extend(chunk)
            pos += n
        lit = bytearray()

    def emit_match(dist, length):
        # dist >= 1, length >= 1; pick the shortest opcode that fits.
        if length <= 128 and dist <= 256:
            out.append(0x80 + length - 1); out.append(dist - 1)
        elif length <= 256 and dist <= 16384:
            out.extend((0x4000 + dist - 1).to_bytes(2, "big")); out.append(length - 1)
        elif length <= 256 and dist <= WINDOW:
            out.extend((0x180000 + dist - 1).to_bytes(3, "big")); out.append(length - 1)
        elif dist <= WINDOW:
            out.extend((0x100000 + dist - 1).to_bytes(3, "big")); out.extend((length - 1).to_bytes(2, "big"))
        else:
            raise ValueError(dist)

    table = {}  # 3-byte hash -> most recent position
    n = len(data)
    i = 0
    MIN = 3
    while i < n:
        best_len, best_dist = 0, 0
        if i + MIN <= n:
            key = data[i:i + MIN]
            cand = table.get(key)
            if cand is not None and i - cand <= WINDOW:
                length = MIN
                limit = min(65536, n - i)
                while length < limit and data[cand + length] == data[i + length]:
                    length += 1
                best_len, best_dist = length, i - cand
            table[key] = i
        if best_len >= MIN:
            flush_literals()
            emit_match(best_dist, best_len)
            # Index the skipped positions so later matches can reach them.
            for k in range(i + 1, min(i + best_len, n - MIN + 1)):
                table[data[k:k + MIN]] = k
            i += best_len
        else:
            lit.append(data[i]); i += 1
    flush_literals()
    out.extend(b"\x05\xfa")
    out.extend(zlib.adler32(data).to_bytes(4, "big"))
    return bytes(out)


def decompress(stream: bytes) -> bytes:
    """Port of imgui.cpp's stb_decompress, used only to verify the encoder."""
    assert stream[:4] == b"\x57\xbc\x00\x00"
    olen = int.from_bytes(stream[8:12], "big")
    out = bytearray()
    i = 16
    in2 = lambda o: (stream[i + o] << 8) | stream[i + o + 1]
    in3 = lambda o: (stream[i + o] << 16) | in2(o + 1)

    def match(dist, length):
        assert 1 <= dist <= len(out)
        for _ in range(length):
            out.append(out[-dist])

    while True:
        b = stream[i]
        if b >= 0x20:
            if b >= 0x80:
                match(stream[i + 1] + 1, b - 0x80 + 1); i += 2
            elif b >= 0x40:
                match(in2(0) - 0x4000 + 1, stream[i + 2] + 1); i += 3
            else:
                n = b - 0x20 + 1; out.extend(stream[i + 1:i + 1 + n]); i += 1 + n
        elif b >= 0x18:
            match(in3(0) - 0x180000 + 1, stream[i + 3] + 1); i += 4
        elif b >= 0x10:
            match(in3(0) - 0x100000 + 1, in2(3) + 1); i += 5
        elif b >= 0x08:
            n = in2(0) - 0x0800 + 1; out.extend(stream[i + 2:i + 2 + n]); i += 2 + n
        elif b == 0x07:
            n = in2(1) + 1; out.extend(stream[i + 3:i + 3 + n]); i += 3 + n
        elif b == 0x06:
            match(in3(1) + 1, stream[i + 4] + 1); i += 5
        elif b == 0x04:
            match(in3(1) + 1, in2(4) + 1); i += 6
        elif b == 0x05 and stream[i + 1] == 0xfa:
            assert len(out) == olen, (len(out), olen)
            assert zlib.adler32(bytes(out)) == int.from_bytes(stream[i + 2:i + 6], "big")
            return bytes(out)
        else:
            raise ValueError(f"bad opcode {b:#x} at {i}")


def c_array(name, stream):
    padded = stream + b"\0" * (-len(stream) % 4)
    words = [int.from_bytes(padded[k:k + 4], "little") for k in range(0, len(padded), 4)]
    lines = [f"const unsigned int {name}_compressed_size = {len(stream)};",
             f"const unsigned int {name}_compressed_data[{len(words)}] = {{"]
    for k in range(0, len(words), 12):
        lines.append("    " + "".join(f"0x{w:08x}," for w in words[k:k + 12]))
    lines.append("};")
    return "\n".join(lines)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    folder = pathlib.Path(sys.argv[1])
    arrays = []
    for name, file in FACES:
        data = (folder / "fonts/ttf" / file).read_bytes()
        stream = compress(data)
        assert decompress(stream) == data, file
        print(f"{file}: {len(data)} -> {len(stream)} bytes")
        arrays.append(c_array(name, stream))
    (ROOT / "licenses").mkdir(exist_ok=True)
    shutil.copy(folder / "OFL.txt", ROOT / "licenses/JetBrainsMono-OFL.txt")
    header = f'''// Generated by scripts/embed_fonts.py from {folder.name}. Do not edit.
//
// JetBrains Mono, Copyright 2020 The JetBrains Mono Project Authors
// (https://github.com/JetBrains/JetBrainsMono), licensed under the SIL Open Font
// License 1.1: see licenses/JetBrainsMono-OFL.txt. Compressed in ImGui's stb format
// for ImFontAtlas::AddFontFromMemoryCompressedTTF.

#include "ofxOceanodeFonts.h"

namespace ofxOceanodeEmbeddedFonts {{

'''
    (ROOT / "src/Gui/ofxOceanodeEmbeddedFontData.cpp").write_text(header + "\n\n".join(arrays) + "\n\n} // namespace ofxOceanodeEmbeddedFonts\n")
    print("wrote src/Gui/ofxOceanodeEmbeddedFontData.cpp")


if __name__ == "__main__":
    main()
