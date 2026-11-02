#!/usr/bin/env python3
"""Layouts whose window settings dock a window into a stale dock node.

ImGui keeps settings of windows that are not open, with their last dock node.
Two such references break a preset layout, and ofxOceanodeDropStaleDockIds
drops them while keeping healthy ones:

- A node since split (here: a mixer's window from before it was encapsulated).
  Loading the preset while the window existed made ImGui assert "split node
  has no tab bar".
- An empty leaf that hidden windows keep alive, with no size along its
  parent's split (here: the Timeline, saved hidden, in a leftover bottom node
  of BLOOBS_BOLETES). Showing the Timeline docked it into a zero-height strip
  on the divider above the AnalogBox dock, and ImGui auto-fitted it to its
  content, so both windows drew over each other.

Builds plain Dear ImGui (the copy ofxImGui ships) headless: each raw layout
must still reproduce its failure, so the test keeps proving the cause, and
the cleaned layout must behave.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
IMGUI = ROOT.parent / "ofxImGui/libs/imgui/src"

SPLIT_LAYOUT = """[Window][DockSpace]
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

# Reduced from BLOOBS_BOLETES' layout.json. Node 0x10 is an empty bottom leaf
# of split 0x17 with SizeRef 0,0, referenced only by the hidden Timeline and a
# node GUI from an earlier state of the patch. Node 0x03 has SizeRef 0 across
# its parent's split axis, which is a healthy reference (Hierarchy).
ZERO_LAYOUT = """[Window][DockSpace]
Pos=0,0
Size=1728,1051
Collapsed=0

[Window][Nodes]
Pos=0,20
Size=445,1007
Collapsed=0
DockId=0x00000025,0

[Window][Hierarchy]
Pos=0,20
Size=445,401
Collapsed=0
DockId=0x00000003,0

[Window][###Canvas]
Pos=449,20
Size=1279,1007
Collapsed=0
DockId=0x0000001D,0

[Window][Timeline]
Pos=684,917
Size=960,20
Collapsed=0
DockId=0x00000010,0

[Window][GrainBox 1]
Pos=0,1031
Size=1728,20
Collapsed=0
DockId=0x00000010,0

[Window][AnalogBox 1]
Pos=0,1031
Size=1728,20
Collapsed=0
DockId=0x00000008,0

[Docking][Data]
DockSpace         ID=0x14621557 Window=0x3DA2F1DE Pos=0,86 Size=1728,1031 Split=Y
  DockNode        ID=0x00000007 Parent=0x14621557 SizeRef=1728,1007 Split=Y
    DockNode      ID=0x00000017 Parent=0x00000007 SizeRef=974,1007 Split=Y
      DockNode    ID=0x0000000F Parent=0x00000017 SizeRef=974,1007 Split=X
        DockNode  ID=0x00000001 Parent=0x0000000F SizeRef=445,1007 Split=Y Selected=0xA88144EA
          DockNode ID=0x00000003 Parent=0x00000001 SizeRef=0,401 Selected=0xBABDAE5E
          DockNode ID=0x00000025 Parent=0x00000001 SizeRef=445,602 Selected=0x3816D57C
        DockNode  ID=0x0000001D Parent=0x0000000F SizeRef=1205,1007 CentralNode=1 Selected=0x30A3673C
      DockNode    ID=0x00000010 Parent=0x00000017 SizeRef=0,0 Selected=0x4F89F0DC
    DockNode      ID=0x0000001A Parent=0x00000007 SizeRef=1728,0 Selected=0xA21A3E6E
  DockNode        ID=0x00000008 Parent=0x14621557 SizeRef=1728,20 Selected=0x1289FDA4
"""

CASES = r'''
#include "imgui.h"
#include "imgui_internal.h"
#include "Gui/ofxOceanodeDockingIni.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

static std::string readFile(const char* path) { std::ifstream file(path); std::stringstream text; text << file.rdbuf(); return text.str(); }
static void createContext() {
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr; io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.DisplaySize = ImVec2(1728, 1117); io.DeltaTime = 1.f / 60;
    unsigned char* pixels; int w, h; io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
}
static void hostFrame() {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0)); ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("DockSpace", nullptr, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar);
    ImGui::Dummy(ImVec2(0, 86));
    ImGui::DockSpace(0x14621557, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();
}
static bool overlap(const ImGuiWindow* a, const ImGuiWindow* b) {
    return a->Pos.x < b->Pos.x + b->Size.x && b->Pos.x < a->Pos.x + a->Size.x
        && a->Pos.y < b->Pos.y + b->Size.y && b->Pos.y < a->Pos.y + a->Size.y;
}

// A window docked into a node that was split since: the raw layout asserts.
static int splitCase(const std::string& text, bool clean) {
    const std::string ini = clean ? ofxOceanodeDropStaleDockIds(text) : text;
    if(clean) {
        // Only the reference to the split node goes.
        if(ini.find("DockId=0x00000005") != std::string::npos) return 2;
        for(const char* kept : {"[Window][SC Mixer Pro 1]", "DockId=0x00000001,0", "DockId=0x00000002,0", "DockId=0x00000006,0", "Split=X"})
            if(ini.find(kept) == std::string::npos) return 3;
        if(ofxOceanodeDropStaleDockIds(ini) != ini) return 4; // already clean: unchanged
    }
    createContext();
    // As a preset loaded while the app runs: the windows exist, then the layout loads.
    for(int frame = 0; frame < 8; ++frame) {
        if(frame == 4) ImGui::LoadIniSettingsFromMemory(ini.c_str(), ini.size());
        hostFrame();
        for(const char* name : {"Nodes", "###Canvas", "SC Mixer Pro 1", "Macro 2/SC Mixer Pro 1"}) { ImGui::Begin(name); ImGui::End(); }
        ImGui::Render();
    }
    ImGui::DestroyContext();
    std::puts("loaded");
    return 0;
}

// A hidden controller docked into an empty zero-size leaf, shown after load.
static int zeroCase(const std::string& text, bool clean) {
    const std::string ini = clean ? ofxOceanodeDropStaleDockIds(text) : text;
    if(clean) {
        // The blocks of the windows in the zero-size leaf go, with all their lines.
        for(const char* gone : {"[Window][Timeline]", "[Window][GrainBox 1]", "DockId=0x00000010", "Pos=684,917"})
            if(ini.find(gone) != std::string::npos) { std::fprintf(stderr, "kept %s\n", gone); return 2; }
        // Healthy references stay: SizeRef 0 across the split axis (0x03), the
        // AnalogBox strip (0x08), and the dock tree itself, including node 0x10.
        for(const char* kept : {"[Window][Hierarchy]\nPos=0,20\nSize=445,401\nCollapsed=0\nDockId=0x00000003,0\n",
                                "[Window][AnalogBox 1]\nPos=0,1031\nSize=1728,20\nCollapsed=0\nDockId=0x00000008,0\n",
                                "DockId=0x00000025,0", "DockId=0x0000001D,0", "ID=0x00000010 Parent=0x00000017 SizeRef=0,0"})
            if(ini.find(kept) == std::string::npos) { std::fprintf(stderr, "lost %s\n", kept); return 3; }
        if(ofxOceanodeDropStaleDockIds(ini) != ini) return 4;
    }
    createContext();
    ImGui::LoadIniSettingsFromMemory(ini.c_str(), ini.size());
    const ImGuiID leftNode = 0x00000025;
    for(int frame = 0; frame < 30; ++frame) {
        hostFrame();
        // Controls::draw: controllers default next to Nodes; the Timeline is shown at frame 10.
        for(const char* name : {"Nodes", "Hierarchy"}) {
            ImGui::SetNextWindowDockID(leftNode, ImGuiCond_FirstUseEver);
            ImGui::Begin(name); ImGui::TextUnformatted(name); ImGui::End();
        }
        if(frame >= 10) {
            ImGui::SetNextWindowDockID(leftNode, ImGuiCond_FirstUseEver);
            ImGui::Begin("Timeline");
            for(int row = 0; row < 12; ++row) ImGui::Text("toolbar row %d", row);
            ImGui::End();
        }
        // Canvas and node GUIs dock into the dockspace by default.
        for(const char* name : {"###Canvas", "AnalogBox 1"}) {
            ImGui::SetNextWindowDockID(0x14621557, ImGuiCond_FirstUseEver);
            ImGui::Begin(name); ImGui::TextUnformatted(name); ImGui::End();
        }
        ImGui::Render();
    }
    ImGuiWindow* timeline = ImGui::FindWindowByName("Timeline");
    ImGuiWindow* analog = ImGui::FindWindowByName("AnalogBox 1");
    ImGuiWindow* nodes = ImGui::FindWindowByName("Nodes");
    const ImGuiDockNode* node = timeline->DockNode;
    std::printf("Timeline pos=(%.0f,%.0f) size=(%.0f,%.0f) node=%08X nodeSize=(%.0f,%.0f) analog pos=(%.0f,%.0f) size=(%.0f,%.0f)\n",
        timeline->Pos.x, timeline->Pos.y, timeline->Size.x, timeline->Size.y, node ? node->ID : 0,
        node ? node->Size.x : -1.f, node ? node->Size.y : -1.f, analog->Pos.x, analog->Pos.y, analog->Size.x, analog->Size.y);
    int result = 0;
    if(!clean) {
        // The cause: a zero-height node on the divider, the window auto-fitted over the AnalogBox dock.
        if(!node || node->ID != 0x00000010 || node->Size.y != 0.0f) result = 10;
        else if(timeline->Size.y < 100.0f || !overlap(timeline, analog)) result = 11;
    } else {
        // The Timeline is a new window: docked beside Nodes, in a sized node, clear of the AnalogBox dock.
        if(!node || node != nodes->DockNode) result = 20;
        else if(node->Size.y < 100.0f || timeline->Size.y < 100.0f) result = 21;
        else if(overlap(timeline, analog)) result = 22;
        else if(analog->DockNode == nullptr || analog->DockNode->ID != 0x00000008) result = 23;
    }
    ImGui::DestroyContext();
    return result;
}

int main(int argc, char** argv) {
    const std::string text = readFile(argv[2]);
    const bool clean = argc > 3 && std::strcmp(argv[3], "clean") == 0;
    if(std::strcmp(argv[1], "split") == 0) return splitCase(text, clean);
    return zeroCase(text, clean);
}
'''


def main():
    with tempfile.TemporaryDirectory(prefix="oceanode-docking-ini-") as folder:
        folder = Path(folder)
        (folder / "imconfig.h").write_text("#pragma once\n")  # plain Dear ImGui, no OF
        (folder / "main.cpp").write_text(CASES)
        (folder / "split.ini").write_text(SPLIT_LAYOUT)
        (folder / "zero.ini").write_text(ZERO_LAYOUT)
        binary = folder / "regression"
        subprocess.run(["clang++", "-std=c++17", "-O0", "-g", "-w", "-I" + str(folder), "-I" + str(IMGUI),
                        "-I" + str(ROOT / "src"), str(folder / "main.cpp"),
                        *[str(IMGUI / name) for name in ("imgui.cpp", "imgui_draw.cpp", "imgui_tables.cpp", "imgui_widgets.cpp")],
                        "-o", str(binary)], check=True)

        raw = subprocess.run([str(binary), "split", str(folder / "split.ini")], capture_output=True, text=True)
        assert raw.returncode != 0 and "TabBar" in raw.stderr, "the raw split layout no longer reproduces the assert"
        clean = subprocess.run([str(binary), "split", str(folder / "split.ini"), "clean"], capture_output=True, text=True)
        assert clean.returncode == 0 and "loaded" in clean.stdout, (clean.returncode, clean.stderr)

        raw = subprocess.run([str(binary), "zero", str(folder / "zero.ini")], capture_output=True, text=True)
        assert raw.returncode == 0, ("the raw zero-size layout no longer reproduces the overlap", raw.returncode, raw.stdout, raw.stderr)
        clean = subprocess.run([str(binary), "zero", str(folder / "zero.ini"), "clean"], capture_output=True, text=True)
        assert clean.returncode == 0, (clean.returncode, clean.stdout, clean.stderr)
    print("Docking ini regression passed: a stale dock reference to a split node loses its DockId (the raw layout "
          "asserts in ImGui, the cleaned one loads); a window saved in an empty zero-size leaf loses its block (raw: "
          "docked into a zero-height strip and auto-fitted over the neighbouring dock; clean: docked beside Nodes); "
          "healthy references and the dock tree are kept.")


if __name__ == "__main__":
    main()
