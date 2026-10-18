#!/usr/bin/env python3
"""Keep the canvas viewport flush with its pane despite overflowing toolbars.

Uses production window setup and real sibling ImGui, without an OF/GPU host.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
IMGUI = Path(os.environ.get("OCEANODE_TEST_IMGUI_SRC", str(ROOT.parent / "ofxImGui/libs/imgui/src")))

CASES = r'''
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

bool docked = false, popupOpen = false;
const char* name = "Canvas###Canvas";
float paneWidth = 520;
ImGuiWindow* parent = nullptr;
ImVec2 toolbarPoint, canvasPoint;

void frame() {
    ImGui::NewFrame();
    if(docked) {
        ImGui::SetNextWindowPos(ImVec2(20,20));
        ImGui::SetNextWindowSize(ImVec2(paneWidth,600));
        ImGui::Begin("Host", nullptr, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar);
        const ImGuiID dockID = ImGui::GetID("DockSpace");
        ImGui::DockSpace(dockID);
        ImGui::End();
        ImGui::SetNextWindowDockID(dockID, ImGuiCond_Always);
    } else {
        ImGui::SetNextWindowPos(ImVec2(20,20));
        ImGui::SetNextWindowSize(ImVec2(paneWidth,600));
    }
    PRODUCTION_WINDOW_SETUP
    ImGui::Begin(name, nullptr, windowFlags);
    parent = ImGui::GetCurrentWindow();
    ImGui::SameLine();
    ImGui::BeginGroup();
    toolbarPoint = ImGui::GetCursorScreenPos() + ImVec2(30,10);
    // The transport/comment toolbar exceeds the narrow canvas pane.
    ImGui::Button("Toolbar", ImVec2(680,20));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    PRODUCTION_CHILD_BEGIN
    auto* child = ImGui::GetCurrentWindow();
    canvasPoint = child->Pos + ImVec2(100,100);
    // The drawing region must reach the pane's right edge on this frame.
    const float expectedRight = parent->Pos.x + parent->Size.x - parent->WindowPadding.x;
    const float actualRight = child->Pos.x + child->Size.x;
    if(std::abs(expectedRight - actualRight) > 1.0f) {
        std::fprintf(stderr, "Canvas gap %.0f, outer scroll %.0f (docked=%d, %s)\n",
            expectedRight - actualRight, parent->Scroll.x, docked, name);
        std::abort();
    }
    assert(parent->Scroll.x == 0 && parent->Scroll.y == 0);
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::EndGroup();
    ImGui::End();
    ImGui::Render();
}

void idle(int count = 4) { while(count--) frame(); }

int main() {
    for(bool dock : {false, true}) for(bool macro : {false, true}) {
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(1000,800);
        io.DeltaTime = 1.0f/60;
        unsigned char* pixels; int w,h;
        io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
        docked = dock;
        name = macro ? "(Macro) Macro_1###Macro_1" : "Canvas###Canvas";
        paneWidth = 520;
        idle();
        assert(parent->ScrollMax.x > 100); // Ensure the overflow is real.
        for(ImVec2 point : {toolbarPoint, canvasPoint}) {
            io.AddMousePosEvent(point.x,point.y);
            idle();
            io.AddMouseWheelEvent(-5,0);
            idle();
            io.AddMouseWheelEvent(0,-3);
            idle();
        }
        // Recover existing offsets and pending navigation scroll requests
        // before Begin calculates the child position, without a resize.
        parent->Scroll = ImVec2(100,20);
        ImGui::SetScrollX(parent,140);
        frame();
        idle(30);
        paneWidth = 800;
        idle();
        paneWidth = 400;
        idle();
        popupOpen = true;
        frame();
        assert(parent->Flags & ImGuiWindowFlags_NoFocusOnAppearing);
        popupOpen = false;
        ImGui::DestroyContext();
    }
    puts("PASS: canvas fills floating/docked root/macro panes after wheel input, stale scroll, idle, resize and popup guards.");
}
'''


def main():
    source = (ROOT / "src/Managers/ofxOceanodeCanvas.cpp").read_text()
    start = source.index("    const ImGuiWindowFlags windowFlags =")
    end = source.index("    if(ImGui::Begin(windowName.c_str(), open, windowFlags))", start)
    child_start = source.index('ImGui::BeginChild("scrolling_region"')
    child_end = source.index(";", child_start) + 1
    fixture = CASES.replace("PRODUCTION_WINDOW_SETUP", source[start:end]).replace(
        "PRODUCTION_CHILD_BEGIN", source[child_start:child_end])
    with tempfile.TemporaryDirectory(prefix="oceanode-canvas-viewport-") as work:
        work = Path(work)
        (work / "imconfig.h").write_text("// Standalone ImGui without OF adapters.\n")
        cpp = work / "regressions.cpp"
        cpp.write_text(fixture)
        binary = work / "regressions"
        subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++17", "-O0", "-g",
                        "-fsanitize=address,undefined", "-I", str(IMGUI), "-I", str(work),
                        str(cpp), *[str(IMGUI / n) for n in
                        ("imgui.cpp", "imgui_draw.cpp", "imgui_tables.cpp", "imgui_widgets.cpp")],
                        "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
