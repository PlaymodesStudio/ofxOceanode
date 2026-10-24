#!/usr/bin/env python3
"""Drag real ImGui dock splitters through Oceanode's proportional resize policy.

Uses the sibling ofxImGui sources without an OF host or graphics backend.
Requires Python 3 and clang++; ASan and UBSan are enabled.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
IMGUI = Path(os.environ.get("OCEANODE_TEST_IMGUI_SRC", str(ROOT.parent / "ofxImGui/libs/imgui/src")))

CASES = r'''
#include "ofxOceanodeProportionalDocking.h"
#include <cassert>
#include <cstdio>
#include <string>

ofxOceanodeProportionalDocking policy;
ImGuiID rootID, leftID, topID, bottomID, laserID, mapID, canvasID;
bool build = true, deep = false, outerAxisY = false;
const char* names[] = {"BLOOBER", "Laser Simulator", "MiniMap", "Canvas", "Laser extra", "Map extra", "Canvas extra"};

void frame() {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0,0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    ImGui::Begin("Host", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar();
    rootID = ImGui::GetID("MainDockSpace");
    if(build) {
        ImGui::DockBuilderAddNode(rootID, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(rootID, ImGui::GetContentRegionAvail());
        ImGui::DockBuilderSplitNode(rootID, outerAxisY ? ImGuiDir_Up : ImGuiDir_Left, 0.45f, &leftID, &canvasID);
        ImGui::DockBuilderSplitNode(leftID, ImGuiDir_Up, 0.55f, &topID, &bottomID);
        ImGui::DockBuilderSplitNode(bottomID, ImGuiDir_Left, 0.4f, &laserID, &mapID);
        ImGuiID laserExtra = 0, mapExtra = 0, canvasExtra = 0;
        if(deep) {
            ImGui::DockBuilderSplitNode(laserID, ImGuiDir_Up, 0.5f, &laserExtra, &laserID);
            ImGui::DockBuilderSplitNode(mapID, ImGuiDir_Left, 0.3f, &mapExtra, &mapID);
            ImGui::DockBuilderSplitNode(canvasID, ImGuiDir_Up, 0.6f, &canvasExtra, &canvasID);
            ImGui::DockBuilderDockWindow(names[4], laserExtra);
            ImGui::DockBuilderDockWindow(names[5], mapExtra);
            ImGui::DockBuilderDockWindow(names[6], canvasExtra);
        }
        ImGui::DockBuilderDockWindow(names[0], topID);
        ImGui::DockBuilderDockWindow(names[1], laserID);
        ImGui::DockBuilderDockWindow(names[2], mapID);
        ImGui::DockBuilderDockWindow(names[3], canvasID);
        ImGui::DockBuilderFinish(rootID);
        build = false;
    }
    policy.dockSpace(rootID, ImVec2(0,0), ImGuiDockNodeFlags_None);
    ImGui::End();
    for(int i=0; i<(deep ? 7 : 4); ++i) {
        ImGui::Begin(names[i]);
        ImGui::TextUnformatted(names[i]);
        ImGui::End();
    }
    ImGui::Render();
}

ImGuiDockNode* node(ImGuiID id) { auto* n=ImGui::DockBuilderGetNode(id); assert(n); return n; }
float width(ImGuiID id) { return node(id)->Size.x; }
float splitRatio(ImGuiID id) {
    auto* n=node(id); int axis=n->SplitAxis;
    return n->ChildNodes[0]->Size[axis] / (n->ChildNodes[0]->Size[axis]+n->ChildNodes[1]->Size[axis]);
}
void near(float a, float b, float tolerance=1.1f) {
    if(std::abs(a-b)>tolerance) { std::fprintf(stderr,"expected %f, got %f\n",b,a); std::abort(); }
}
void checkGeometry(ImGuiDockNode* n) {
    if(!n->IsSplitNode()) {
        for(auto* w:n->Windows) {
            near(w->Pos.x,n->Pos.x); near(w->Pos.y,n->Pos.y);
            near(w->Size.x,n->Size.x); near(w->Size.y,n->Size.y);
        }
        return;
    }
    auto* a=n->ChildNodes[0]; auto* b=n->ChildNodes[1]; int axis=n->SplitAxis;
    if(a->IsVisible && b->IsVisible) {
        near(a->Size[axis]+b->Size[axis]+ImGui::GetStyle().DockingSeparatorSize,n->Size[axis]);
        near(b->Pos[axis],a->Pos[axis]+a->Size[axis]+ImGui::GetStyle().DockingSeparatorSize);
        near(a->Pos[axis^1],n->Pos[axis^1]); near(b->Pos[axis^1],n->Pos[axis^1]);
        assert(a->Size.x>=ImGui::GetStyle().WindowMinSize.x);
        assert(a->Size.y>=ImGui::GetStyle().WindowMinSize.y);
        assert(b->Size.x>=ImGui::GetStyle().WindowMinSize.x);
        assert(b->Size.y>=ImGui::GetStyle().WindowMinSize.y);
    }
    if(a->IsVisible) checkGeometry(a);
    if(b->IsVisible) checkGeometry(b);
}
ImVec2 dragStart;
void press(ImGuiID id) {
    auto* n=node(id); auto* a=n->ChildNodes[0];
    dragStart=ImVec2(n->Pos.x+n->Size.x*0.5f,n->Pos.y+n->Size.y*0.5f);
    dragStart[n->SplitAxis]=a->Pos[n->SplitAxis]+a->Size[n->SplitAxis]+ImGui::GetStyle().DockingSeparatorSize*0.5f;
    auto& io=ImGui::GetIO(); io.AddMousePosEvent(dragStart.x,dragStart.y); frame(); frame();
    io.AddMouseButtonEvent(0,true); frame();
    ImGuiID seed=ImHashData(&n->ID,sizeof(n->ID),n->HostWindow->ID);
    assert(GImGui->ActiveId==ImHashStr("##Splitter",0,seed));
}
void move(ImGuiAxis axis, float delta) {
    ImVec2 p=dragStart; p[axis]+=delta;
    ImGui::GetIO().AddMousePosEvent(p.x,p.y); frame(); checkGeometry(node(rootID));
}
void release() { ImGui::GetIO().AddMouseButtonEvent(0,false); frame(); frame(); }
void idle(int count=5) { for(int i=0;i<count;++i) frame(); }
void checkRatio(ImGuiID id,float expected) {
    auto* n=node(id); int axis=n->SplitAxis;
    float available=n->Size[axis]-ImGui::GetStyle().DockingSeparatorSize;
    near(n->ChildNodes[0]->Size[axis],available*expected);
}
int main() {
    ImGui::CreateContext(); auto& io=ImGui::GetIO();
    io.ConfigFlags|=ImGuiConfigFlags_DockingEnable; io.IniFilename=nullptr;
    io.DisplaySize=ImVec2(1600,1000); io.DeltaTime=1.0f/60;
    unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    idle();
    float original=splitRatio(bottomID), laser=width(laserID), map=width(mapID);
    press(rootID); move(ImGuiAxis_X,-180);
    assert(width(laserID)<laser && width(mapID)<map); checkRatio(bottomID,original);
    move(ImGuiAxis_X,150);
    assert(width(laserID)>laser && width(mapID)>map); checkRatio(bottomID,original);
    // Hundreds of movements within one drag must not accumulate rounding drift.
    for(int i=0;i<300;++i) { move(ImGuiAxis_X,(i%2 ? -170 : 80)+i%11); checkRatio(bottomID,original); }
    move(ImGuiAxis_X,-10000); checkRatio(bottomID,original);
    float limit=width(leftID); move(ImGuiAxis_X,-11000); near(width(leftID),limit);
    move(ImGuiAxis_X,0); checkRatio(bottomID,original); release();
    // Changing the inner divider establishes a new ratio for later outer drags.
    press(bottomID); move(ImGuiAxis_X,100); release();
    float changed=splitRatio(bottomID); assert(changed>original+0.1f);
    press(rootID); move(ImGuiAxis_X,-140); checkRatio(bottomID,changed); release();
    // Height resizing works too, including descendants containing the central node.
    build=true; deep=true; policy.reset(); idle();
    ImGuiID laserParent=node(laserID)->ParentNode->ID;
    ImGuiID mapParent=node(mapID)->ParentNode->ID;
    ImGuiID canvasParent=node(canvasID)->ParentNode->ID;
    float lowerRatio=splitRatio(bottomID), mapRatio=splitRatio(mapParent);
    float laserRatio=splitRatio(laserParent);
    press(rootID); move(ImGuiAxis_X,-170);
    checkRatio(bottomID,lowerRatio); checkRatio(mapParent,mapRatio); release();
    press(leftID); move(ImGuiAxis_Y,-140);
    checkRatio(laserParent,laserRatio); release();
    // Tabbed windows share the same proportionally resized leaf.
    ImGui::DockBuilderDockWindow(names[4],mapID); ImGui::DockBuilderFinish(rootID); idle();
    float tabRatio=splitRatio(bottomID);
    press(rootID); move(ImGuiAxis_X,-80); checkRatio(bottomID,tabRatio); release();
    // A saved layout round-trip preserves the resulting sizes and ratios.
    size_t length; std::string ini=ImGui::SaveIniSettingsToMemory(&length);
    policy.reset(); ImGui::LoadIniSettingsFromMemory(ini.c_str(),ini.size()); idle();
    float savedRatio=splitRatio(bottomID); idle(20); checkRatio(bottomID,savedRatio);
    press(rootID); move(ImGuiAxis_X,90); checkRatio(bottomID,savedRatio); release();
    // A disabled divider must remain disabled.
    node(leftID)->SetLocalFlags(node(leftID)->LocalFlags | ImGuiDockNodeFlags_NoResizeX); idle();
    float disabledWidth=width(leftID);
    auto* a=node(leftID); ImVec2 p(a->Pos.x+a->Size.x+2,a->Pos.y+100);
    io.AddMousePosEvent(p.x,p.y); frame();io.AddMouseButtonEvent(0,true);frame();
    io.AddMousePosEvent(p.x-100,p.y);frame();near(width(leftID),disabledWidth);release();
    // Layout changes during a held drag invalidate snapshots safely.
    node(leftID)->SetLocalFlags(node(leftID)->LocalFlags & ~ImGuiDockNodeFlags_NoResizeX);idle();
    press(rootID);move(ImGuiAxis_X,-30);build=true;policy.reset();frame();release();idle();
    checkGeometry(node(rootID));
    // A merge during a drag invalidates the cached descendant ratios by topology.
    press(rootID);move(ImGuiAxis_X,-20);
    ImGui::DockContextQueueUndockWindow(GImGui,ImGui::FindWindowByName(names[5]));
    move(ImGuiAxis_X,-40);release();idle();checkGeometry(node(rootID));
    // An outer horizontal divider resizes nested horizontal splits on both
    // sides, including the subtree that contains ImGui's central node.
    build=true;outerAxisY=true;policy.reset();idle();
    laserParent=node(laserID)->ParentNode->ID;
    canvasParent=node(canvasID)->ParentNode->ID;
    laserRatio=splitRatio(laserParent);float canvasRatio=splitRatio(canvasParent);
    float topRatio=splitRatio(leftID);
    press(rootID);move(ImGuiAxis_Y,90);
    checkRatio(leftID,topRatio);checkRatio(laserParent,laserRatio);checkRatio(canvasParent,canvasRatio);
    move(ImGuiAxis_Y,-10000);
    checkRatio(leftID,topRatio);checkRatio(laserParent,laserRatio);checkRatio(canvasParent,canvasRatio);
    release();
    std::printf("PASS: proportional shrink/grow, 300 drag frames, minimum limits, inner divider, nested X/Y splits, tabs, saved layout, disabled divider, held layout replacement/merge, central subtree.\n");
    ImGui::DestroyContext();
}
'''


def main():
    with tempfile.TemporaryDirectory(prefix="oceanode-proportional-docking-") as work:
        work = Path(work)
        (work / "imconfig.h").write_text("// Standalone ImGui without OF adapters.\n")
        cpp = work / "regressions.cpp"
        cpp.write_text(CASES)
        binary = work / "regressions"
        subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++17", "-O0", "-g",
                        "-fsanitize=address,undefined", "-I", str(IMGUI), "-I", str(work),
                        "-I", str(ROOT / "src"), "-I", str(ROOT / "src/Gui"), str(cpp),
                        *[str(ROOT / "src/Gui" / name) for name in ("ofxOceanodeProportionalDocking.cpp", "ofxOceanodeDockCollapse.cpp", "ofxOceanodeFloatingWindowDecor.cpp")],
                        *[str(IMGUI / name) for name in
                          ("imgui.cpp", "imgui_draw.cpp", "imgui_tables.cpp", "imgui_widgets.cpp")],
                        "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
