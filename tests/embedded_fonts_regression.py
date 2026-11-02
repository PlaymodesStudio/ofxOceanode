#!/usr/bin/env python3
"""Regression test for the embedded built-in fonts.

Decodes src/Gui/ofxOceanodeEmbeddedFontData.cpp with the real stb_decompress from
imgui_draw.cpp, checks both faces are TrueType files of the advertised size, and builds
an ImGui atlas from them through AddFontFromMemoryCompressedTTF, the call the
canvas uses. Requires Python 3 and clang++; ASan and UBSan are enabled.
"""
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
IMGUI = Path(os.environ.get("OCEANODE_TEST_IMGUI_SRC", str(ROOT.parent / "ofxImGui/libs/imgui/src")))

HARNESS = r'''
#include "imgui_draw.cpp" // for the static stb_decompress helpers
#include "ofxOceanodeFonts.h"
#include <cassert>
#include <cstdio>
#include <vector>

static void check(const char* label, const unsigned int* data, unsigned int size){
    const auto* bytes = reinterpret_cast<const unsigned char*>(data);
    const unsigned int length = stb_decompress_length(bytes);
    std::vector<unsigned char> ttf(length);
    assert(stb_decompress(ttf.data(), bytes, size) == length);
    // TrueType sfnt version 0x00010000.
    assert(ttf[0] == 0 && ttf[1] == 1 && ttf[2] == 0 && ttf[3] == 0);
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig cfg;
    ImFont* font = io.Fonts->AddFontFromMemoryCompressedTTF(data, (int)size, 13.0f, &cfg);
    assert(font);
    io.Fonts->Build();
    assert(font->CalcTextSizeA(13.0f, FLT_MAX, 0.0f, "Oceanode").x > 0.0f);
    printf("%s: %u compressed -> %u bytes, text width %.1f\n", label, size, length,
           font->CalcTextSizeA(13.0f, FLT_MAX, 0.0f, "Oceanode").x);
    io.Fonts->Clear();
}

int main(){
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    using namespace ofxOceanodeEmbeddedFonts;
    check("JetBrainsMono-Medium", JetBrainsMonoMedium_compressed_data, JetBrainsMonoMedium_compressed_size);
    check("JetBrainsMono-ExtraBold", JetBrainsMonoExtraBold_compressed_data, JetBrainsMonoExtraBold_compressed_size);
    assert(ofxOceanodeFonts::embeddedRegular().data == JetBrainsMonoMedium_compressed_data);
    assert(ofxOceanodeFonts::embeddedBold().size == JetBrainsMonoExtraBold_compressed_size);
    ImGui::DestroyContext();
    puts("PASS: embedded fonts decode, are TrueType, and build an atlas.");
    return 0;
}
'''

# ofxOceanodeFonts.cpp needs openFrameworks; the harness only needs the accessors.
STUB = r'''
#include "ofxOceanodeFonts.h"
ofxOceanodeFonts::Embedded ofxOceanodeFonts::embeddedRegular(){
    return {ofxOceanodeEmbeddedFonts::JetBrainsMonoMedium_compressed_data,
            ofxOceanodeEmbeddedFonts::JetBrainsMonoMedium_compressed_size};
}
ofxOceanodeFonts::Embedded ofxOceanodeFonts::embeddedBold(){
    return {ofxOceanodeEmbeddedFonts::JetBrainsMonoExtraBold_compressed_data,
            ofxOceanodeEmbeddedFonts::JetBrainsMonoExtraBold_compressed_size};
}
'''


def main():
    production = (ROOT / "src/Gui/ofxOceanodeFonts.cpp").read_text()
    # The stub must match the production accessors it stands in for.
    for name in ("JetBrainsMonoMedium_compressed_data", "JetBrainsMonoExtraBold_compressed_data"):
        assert name in production, name
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        (work / "imconfig.h").write_text("// Standalone ImGui without OF adapters.\n")
        (work / "harness.cpp").write_text(HARNESS)
        (work / "stub.cpp").write_text(STUB)
        exe = work / "harness"
        subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++17", "-O1", "-g",
                        "-fsanitize=address,undefined", "-I", str(IMGUI), "-I", str(work), "-I", str(ROOT / "src/Gui"),
                        str(work / "harness.cpp"), str(work / "stub.cpp"),
                        str(ROOT / "src/Gui/ofxOceanodeEmbeddedFontData.cpp"),
                        *[str(IMGUI / f) for f in ("imgui.cpp", "imgui_tables.cpp", "imgui_widgets.cpp")],
                        "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True, cwd=work)


if __name__ == "__main__":
    sys.exit(main())
