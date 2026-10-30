#!/usr/bin/env python3
"""A layout whose window settings dock a window into a split node.

ImGui keeps settings of windows that are not open, with their last dock node.
A preset layout saved after that node was split (here: a mixer's window from
before it was encapsulated) made ImGui assert "split node has no tab bar" when
the preset was loaded while the window existed. ofxOceanodeDropStaleDockIds
drops such references and keeps healthy ones. Builds plain Dear ImGui (the
copy ofxImGui ships) headless: the raw layout must still assert, so the test
keeps proving the cause, and the cleaned layout must load.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
IMGUI = ROOT.parent / "ofxImGui/libs/imgui/src"

LAYOUT = """[Window][DockSpace]
Pos=0,0
Size=1728,1051
Collapsed=0

[Window][Nodes]
Pos=0,20
Size=248,696
Collapsed=0
DockId=0x00000001,0

[Window][###Canvas]
Pos=252,20
Size=1476,696
Collapsed=0
DockId=0x00000002,0

[Window][SC Mixer Pro 1]
Pos=0,425
Size=1728,626
Collapsed=0
DockId=0x00000005,0

[Window][Macro 2/SC Mixer Pro 1]
Pos=0,720
Size=1728,331
Collapsed=0
DockId=0x00000006,0

[Docking][Data]
DockSpace     ID=0x14621557 Window=0x3DA2F1DE Pos=0,86 Size=1728,1031 Split=Y
  DockNode    ID=0x00000005 Parent=0x14621557 SizeRef=1728,696 Split=X
    DockNode  ID=0x00000001 Parent=0x00000005 SizeRef=248,43 Selected=0x36DC96AB
    DockNode  ID=0x00000002 Parent=0x00000005 SizeRef=1476,43 CentralNode=1 Selected=0x4F89F0DC
  DockNode    ID=0x00000006 Parent=0x14621557 SizeRef=1728,331 Selected=0x5D316016
"""

CASES = r'''
#include "imgui.h"
#include "Gui/ofxOceanodeDockingIni.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
int main(int argc, char** argv) {
    std::ifstream file(argv[1]); std::stringstream text; text << file.rdbuf();
    const bool clean = argc > 2 && std::strcmp(argv[2], "clean") == 0;
    const std::string ini = clean ? ofxOceanodeDropStaleDockIds(text.str()) : text.str();
    if(clean) {
        // Only the reference to the split node goes.
        if(ini.find("DockId=0x00000005") != std::string::npos) return 2;
        for(const char* kept : {"DockId=0x00000001,0", "DockId=0x00000002,0", "DockId=0x00000006,0", "Split=X"})
            if(ini.find(kept) == std::string::npos) return 3;
        if(ofxOceanodeDropStaleDockIds(ini) != ini) return 4; // already clean: unchanged
    }
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr; io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.DisplaySize = ImVec2(1728, 1117); io.DeltaTime = 1.f / 60;
    unsigned char* pixels; int w, h; io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    // As a preset loaded while the app runs: the windows exist, then the layout loads.
    for(int frame = 0; frame < 8; ++frame) {
        if(frame == 4) ImGui::LoadIniSettingsFromMemory(ini.c_str(), ini.size());
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0)); ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("DockSpace", nullptr, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar);
        ImGui::DockSpace(ImGui::GetID("MyDockSpace"));
        ImGui::End();
        for(const char* name : {"Nodes", "###Canvas", "SC Mixer Pro 1", "Macro 2/SC Mixer Pro 1"}) { ImGui::Begin(name); ImGui::End(); }
        ImGui::Render();
    }
    ImGui::DestroyContext();
    std::puts("loaded");
    return 0;
}
'''


def main():
    with tempfile.TemporaryDirectory(prefix="oceanode-docking-ini-") as folder:
        folder = Path(folder)
        (folder / "imconfig.h").write_text("#pragma once\n")  # plain Dear ImGui, no OF
        (folder / "main.cpp").write_text(CASES)
        (folder / "layout.ini").write_text(LAYOUT)
        binary = folder / "regression"
        subprocess.run(["clang++", "-std=c++17", "-O0", "-g", "-w", "-I" + str(folder), "-I" + str(IMGUI),
                        "-I" + str(ROOT / "src"), str(folder / "main.cpp"),
                        *[str(IMGUI / name) for name in ("imgui.cpp", "imgui_draw.cpp", "imgui_tables.cpp", "imgui_widgets.cpp")],
                        "-o", str(binary)], check=True)
        raw = subprocess.run([str(binary), str(folder / "layout.ini")], capture_output=True, text=True)
        assert raw.returncode != 0 and "TabBar" in raw.stderr, "the raw layout no longer reproduces the assert"
        clean = subprocess.run([str(binary), str(folder / "layout.ini"), "clean"], capture_output=True, text=True)
        assert clean.returncode == 0 and "loaded" in clean.stdout, (clean.returncode, clean.stderr)
    print("Docking ini regression passed: a stale window dock reference to a split node is dropped (the raw "
          "layout asserts in ImGui, the cleaned one loads), healthy references and the dock tree are kept.")


if __name__ == "__main__":
    main()
