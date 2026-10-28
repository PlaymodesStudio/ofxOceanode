#!/usr/bin/env python3
"""Exercise production node nudging with real ImGui focus and keyboard events."""
from pathlib import Path
import os
import subprocess
import tempfile

from canvas_navigation_regression import IMGUI, ROOT, block

FIXTURE = r'''
#include "imgui.h"
#include "imgui_internal.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
namespace glm {
struct vec2 {float x=0,y=0;};
}
struct Gui {
    glm::vec2 position; bool selected=true;
    auto getPosition(){return position;}
    void setPosition(glm::vec2 p){position=p;}
};
struct Node {Gui gui; auto& getNodeGui(){return gui;}};
struct Container {
    std::vector<Node> nodes{{{{100,150},true}}, {{{-50,0},true}}, {{{25,25},false}}};
    std::vector<glm::vec2> before;
    int edits=0;
    auto getSelectedModules(){
        std::vector<Node*> out;
        for(auto& n:nodes)if(n.gui.selected)out.push_back(&n);
        return out;
    }
    void beginCanvasEdit(){
        ++edits;before.clear();for(auto& n:nodes)before.push_back(n.gui.position);
    }
};
struct ofxOceanodeCanvas {
    Container storage; Container* container=&storage;
    int GRID_SIZE=50;
    bool isSelecting=false,isCreatingConnection=false;
    void handleNodeNudge();
};
'''

CASES = r'''
ofxOceanodeCanvas root,macro;
bool editing=false,textInput=false,popup=false;
bool docked=false;
ImGuiID dock;
void frame(){
    ImGui::NewFrame();
    if(docked)ImGui::DockSpaceOverViewport(dock,ImGui::GetMainViewport());
    for(auto* canvas:{&root,&macro}){
        if(docked)ImGui::SetNextWindowDockID(dock,ImGuiCond_Always);
        ImGui::Begin(canvas==&root?"Canvas":"Macro",nullptr,ImGuiWindowFlags_NoFocusOnAppearing);
        if(editing)ImGui::SetActiveID(ImGui::GetID("Editor"),ImGui::GetCurrentWindow());
        if(textInput)ImGui::GetIO().WantTextInput=true;
        if(popup){ImGui::OpenPopup("Menu");if(ImGui::BeginPopup("Menu")){ImGui::TextUnformatted("Menu");ImGui::EndPopup();}}
        canvas->handleNodeNudge();
        ImGui::BeginChild("scrolling_region");ImGui::EndChild();
        ImGui::End();
    }
    ImGui::Render();
}
void focus(bool useMacro=false,bool child=false){
    auto* w=ImGui::FindWindowByName(useMacro?"Macro":"Canvas");assert(w);
    if(child)for(auto* candidate:GImGui->Windows)if(candidate->ParentWindow==w){w=candidate;break;}
    ImGui::FocusWindow(w);
}
void key(ImGuiKey k,bool down){ImGui::GetIO().AddKeyEvent(k,down);frame();}
void tap(ImGuiKey k){key(k,true);key(k,false);}
void expect(Node& n,float x,float y){assert(n.gui.position.x==x && n.gui.position.y==y);}
int main(){
    for(bool useDock:{false,true}){
        ImGui::CreateContext();auto& io=ImGui::GetIO();
        io.DisplaySize={1280,900};io.DeltaTime=1.f/60;io.IniFilename=nullptr;
        io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;io.ConfigInputTrickleEventQueue=false;
        unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
        docked=useDock;dock=ImHashStr("NudgeDock");
        root.storage=Container{};macro.storage=Container{};
        frame();frame();frame();focus(false,true);
        auto& a=root.storage.nodes[0];auto& b=root.storage.nodes[1];auto& untouched=root.storage.nodes[2];
        tap(ImGuiKey_RightArrow);expect(a,150,150);expect(b,0,0);expect(untouched,25,25);
        assert(root.storage.before[0].x==100 && root.storage.before[1].x==-50);
        assert(root.storage.edits==1 && macro.storage.edits==0);
        tap(ImGuiKey_LeftArrow);tap(ImGuiKey_UpArrow);tap(ImGuiKey_DownArrow);
        expect(a,100,150);expect(b,-50,0);

        // Off-grid and negative coordinates advance to the next directional
        // grid line, without moving the other axis. No snap toggle is required.
        a.gui.position={12,-12};b.gui.position={-12,12};
        tap(ImGuiKey_RightArrow);expect(a,50,-12);expect(b,0,12);
        tap(ImGuiKey_UpArrow);expect(a,50,-50);expect(b,0,0);
        a.gui.position={12,-12};b.gui.position={-12,12};
        tap(ImGuiKey_LeftArrow);expect(a,0,-12);expect(b,-50,12);
        tap(ImGuiKey_DownArrow);expect(a,0,0);expect(b,-50,50);

        // Root and macro selections are independent even in a shared dock.
        focus(true,true);tap(ImGuiKey_RightArrow);
        expect(macro.storage.nodes[0],150,150);expect(a,0,0);
        assert(macro.storage.edits==1);
        focus(false);
        const int start=root.storage.edits;
        key(ImGuiKey_RightArrow,true);
        for(int i=0;i<45;++i)frame();
        key(ImGuiKey_RightArrow,false);
        assert(root.storage.edits>start+1 && a.gui.position.x==50*(root.storage.edits-start));

        // Editing, menus, mouse gestures, and navigation modifiers own arrows.
        const int guarded=root.storage.edits;
        editing=true;tap(ImGuiKey_LeftArrow);editing=false;ImGui::ClearActiveID();frame();
        textInput=true;tap(ImGuiKey_LeftArrow);textInput=false;frame();
        popup=true;tap(ImGuiKey_LeftArrow);popup=false;GImGui->OpenPopupStack.clear();frame();focus();
        for(auto mod:{ImGuiMod_Ctrl,ImGuiMod_Super,ImGuiMod_Alt,ImGuiMod_Shift}){
            io.AddKeyEvent(mod,true);frame();tap(ImGuiKey_LeftArrow);
            io.AddKeyEvent(mod,false);frame();
        }
        for(int button:{0,1,2}){
            io.AddMouseButtonEvent(button,true);frame();tap(ImGuiKey_LeftArrow);
            io.AddMouseButtonEvent(button,false);frame();focus();
        }
        root.isSelecting=true;tap(ImGuiKey_LeftArrow);root.isSelecting=false;
        root.isCreatingConnection=true;tap(ImGuiKey_LeftArrow);root.isCreatingConnection=false;
        root.GRID_SIZE=0;tap(ImGuiKey_LeftArrow);root.GRID_SIZE=50;
        assert(root.storage.edits==guarded);
        ImGui::NewFrame();ImGui::Begin("Inspector");ImGui::SetWindowFocus();ImGui::End();ImGui::Render();
        tap(ImGuiKey_LeftArrow);assert(root.storage.edits==guarded);
        focus();a.gui.selected=false;b.gui.selected=false;
        tap(ImGuiKey_LeftArrow);assert(root.storage.edits==guarded);
        ImGui::DestroyContext();
    }
    puts("Canvas nudge regression checks passed.");
}
'''


def main():
    source = (ROOT / "src/Managers/ofxOceanodeCanvas.cpp").read_text()
    handler = block(source, "void ofxOceanodeCanvas::handleNodeNudge()")
    assert source.index("        handleNodeNudge();") < source.index("for(auto nodePair : nodesVisibleInThisFrame)")
    with tempfile.TemporaryDirectory(prefix="oceanode-canvas-nudge-") as directory:
        work = Path(directory)
        (work / "imconfig.h").write_text("// Standalone ImGui without OF adapters.\n")
        cpp = work / "regression.cpp"
        cpp.write_text(FIXTURE + handler + CASES)
        executable = work / "regression"
        subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++17", "-O1", "-g",
                        "-fsanitize=address,undefined", "-I", str(IMGUI), "-I", str(work), str(cpp),
                        *[str(IMGUI / name) for name in
                          ("imgui.cpp", "imgui_draw.cpp", "imgui_widgets.cpp", "imgui_tables.cpp")],
                        "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
