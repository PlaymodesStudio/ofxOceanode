#!/usr/bin/env python3
"""Exercise production New Node focus/Escape handling with real ImGui input."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
IMGUI = Path(os.environ.get('OCEANODE_TEST_IMGUI_SRC', ROOT.parent / 'ofxImGui/libs/imgui/src'))

FIXTURE = r'''
#include "imgui.h"
#include "imgui_internal.h"
#include <cassert>
#include <iostream>
#include <string>
using namespace std;
string searchField, lastSearchField;
int selectedSearchResultIndex = -1;
bool searchActive = false, modalOpen = false, requestOpen = false;
bool openMenus = false, menuFocused = false;
ImGuiID searchID = 0;
ImVec2 resultPoint;
void frame(){
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({800, 600});
    ImGui::Begin("Canvas");
    if(requestOpen){
        searchField.clear(); lastSearchField.clear(); selectedSearchResultIndex = -1;
        ImGui::OpenPopup("New Node"); requestOpen = false;
    }
    searchActive = false; menuFocused = false;
PRODUCTION_OPEN_AND_SEARCH
        searchActive = ImGui::IsItemActive();
        searchID = ImGui::GetID("Search");
        ImGui::Selectable("Result", false, ImGuiSelectableFlags_NoAutoClosePopups);
        auto a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        resultPoint = {(a.x + b.x) / 2, (a.y + b.y) / 2};
        if(openMenus) ImGui::OpenPopup("Modules");
        if(ImGui::BeginMenu("Modules")){
            if(openMenus) ImGui::OpenPopup("Generators");
            if(ImGui::BeginMenu("Generators")){
                ImGui::MenuItem("Oscillator");
                ImGui::ClearActiveID();
                ImGui::FocusWindow(ImGui::GetCurrentWindow());
                menuFocused = true;
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
    modalOpen = ImGui::IsPopupOpen("New Node");
    ImGui::End();
    ImGui::Render();
}
void idle(int n = 2){while(n--) frame();}
void open(){requestOpen = true; frame();}
void escape(){
    auto& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiKey_Escape, true); frame();
    assert(!modalOpen);
    assert(GImGui->OpenPopupStack.empty());
    io.AddKeyEvent(ImGuiKey_Escape, false); frame();
}
int main(){
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr; io.DisplaySize = {1000, 700}; io.DeltaTime = 1.f / 60;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    unsigned char* pixels; int w, h; io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    idle();
    // Open on the same right-button event used by the canvas. ImGui applies
    // its focus request on the next frame, before any further click is needed.
    io.AddMouseButtonEvent(1, true); open();
    io.AddMouseButtonEvent(1, false); frame();
    assert(modalOpen && searchActive && ImGui::GetActiveID() == searchID);
    io.AddInputCharactersUTF8("osc"); frame();
    assert(searchField == "osc" && lastSearchField == "osc");
    escape();
    // Reopening must restore Search focus even after a previous edited session.
    open(); frame(); assert(searchActive); io.AddInputCharactersUTF8("macro"); frame();
    assert(searchActive && searchField == "macro");
    escape();
    // Empty Search also closes on a single Escape press.
    open(); idle(); assert(searchActive && searchField.empty()); escape();
    // Moving focus away from Search must not immediately pull it back.
    open(); idle();
    io.AddMousePosEvent(resultPoint.x, resultPoint.y); idle();
    io.AddMouseButtonEvent(0, true); frame();
    io.AddMouseButtonEvent(0, false); frame();
    assert(modalOpen && !searchActive); escape();
    // One Escape closes the entire modal, not just the deepest category menu.
    open(); idle(); openMenus = true; idle(3);
    assert(modalOpen && menuFocused && GImGui->OpenPopupStack.Size == 3);
    escape(); openMenus = false;
    // Esc on the opening frame must dismiss the modal before drawing controls.
    io.AddKeyEvent(ImGuiKey_Escape, true); open(); assert(!modalOpen);
    io.AddKeyEvent(ImGuiKey_Escape, false); frame();
    ImGui::DestroyContext();
    cout << "PASS: immediate search typing, reopen focus, single-Escape dismissal from empty/edited Search, results and nested menus\n";
}
'''

source = (ROOT / 'src/Managers/ofxOceanodeCanvas.cpp').read_text()
start = source.index('        // Draw New Node menu')
end = source.index('            bool isEnterPressed', start)
production = source[start:end]
assert 'numTimesPopup' not in source
with tempfile.TemporaryDirectory(prefix='oceanode-new-node-menu-') as temp:
    work = Path(temp)
    (work / 'imconfig.h').write_text('')
    cpp = work / 'new_node_menu.cpp'
    cpp.write_text(FIXTURE.replace('PRODUCTION_OPEN_AND_SEARCH', production))
    binary = work / 'new_node_menu'
    subprocess.run([os.environ.get('CXX', 'clang++'), '-std=c++17', '-Wall', '-Wextra',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    '-I' + str(IMGUI), '-I' + temp, str(cpp),
                    *[str(IMGUI / name) for name in ('imgui.cpp', 'imgui_draw.cpp', 'imgui_widgets.cpp', 'imgui_tables.cpp')],
                    '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
