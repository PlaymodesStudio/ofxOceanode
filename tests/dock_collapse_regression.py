#!/usr/bin/env python3
"""Real ImGui input and decoration checks for canvas-relative dock folding."""
import proportional_docking_regression as proportional
from pathlib import Path
import os
import subprocess
import tempfile

CASES = r'''
#include "ofxOceanodeProportionalDocking.h"
#include "ofxOceanodeFloatingWindowDecor.h"
#include <cassert>
#include <cstdio>
#include <string>

ofxOceanodeProportionalDocking policy;
ImGuiID rootID, panes[7], farLeft, nearLeft, farRight, nearRight, topSplit, bottomSplit;
bool build=true, grouped=false, showAll=true, showBottom=true;
bool showOverlay=false, overlayAfterHandles=false, overlayClicked=false;
ImVec2 overlayPos;
const char* names[]={"Outer left", "Inner left", "Canvas", "Inner right", "Outer right", "Top", "Bottom"};
ImGuiDockNode* node(ImGuiID id) { auto* n=ImGui::DockBuilderGetNode(id); assert(n); return n; }
void near(float a,float b,float tolerance=1.1f) { if(std::abs(a-b)>tolerance) { std::fprintf(stderr,"expected %f, got %f\n",b,a); std::abort(); } }
void drawOverlay() {
    ImGui::SetNextWindowPos(overlayPos); ImGui::SetNextWindowSize(ImVec2(160,120));
    ImGui::Begin("Floating overlay",nullptr,ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings);
    overlayClicked |= ImGui::InvisibleButton("Overlay content",ImGui::GetContentRegionAvail());
    ImGui::End();
}
void frame() {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0,0)); ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    ImGui::Begin("Host",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar(); rootID=ImGui::GetID("MainDockSpace");
    if(build) {
        ImGui::DockBuilderAddNode(rootID,ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(rootID,ImGui::GetContentRegionAvail());
        ImGuiID body, middle, rest, rightGroup;
        topSplit=rootID;
        ImGui::DockBuilderSplitNode(rootID,ImGuiDir_Up,0.2f,&panes[5],&body);
        bottomSplit=body;
        ImGui::DockBuilderSplitNode(body,ImGuiDir_Down,0.25f,&panes[6],&middle);
        if(grouped) {
            ImGuiID leftGroup; nearLeft=middle;
            ImGui::DockBuilderSplitNode(middle,ImGuiDir_Left,0.35f,&leftGroup,&rest);
            farLeft=leftGroup;
            ImGui::DockBuilderSplitNode(leftGroup,ImGuiDir_Left,0.4f,&panes[0],&panes[1]);
        } else {
            farLeft=middle; ImGuiID inner;
            ImGui::DockBuilderSplitNode(middle,ImGuiDir_Left,0.15f,&panes[0],&inner);
            nearLeft=inner;
            ImGui::DockBuilderSplitNode(inner,ImGuiDir_Left,0.23f,&panes[1],&rest);
        }
        farRight=rest; ImGuiID inner;
        ImGui::DockBuilderSplitNode(rest,ImGuiDir_Right,0.2f,&panes[4],&inner);
        nearRight=inner;
        ImGui::DockBuilderSplitNode(inner,ImGuiDir_Right,0.25f,&panes[3],&panes[2]);
        for(int i=0;i<7;++i) ImGui::DockBuilderDockWindow(names[i],panes[i]);
        ImGui::DockBuilderFinish(rootID); build=false;
    }
    policy.dockSpace(rootID,ImVec2(0,0),ImGuiDockNodeFlags_None);
    ImGui::End();
    for(int i=0;i<7;++i) if((showAll || i!=0) && (showBottom || i!=6)) { ImGui::Begin(names[i]); ImGui::TextUnformatted(names[i]); ImGui::End(); }
    if(showOverlay && !overlayAfterHandles) drawOverlay();
    policy.drawDockCollapseHandles();
    if(showOverlay && overlayAfterHandles) drawOverlay();
    ImGui::Render();
}
void idle(int count=5) { for(int i=0;i<count;++i) frame(); }
ImGuiWindow* handle(ImGuiID id) {
    char name[80]; ImFormatString(name,IM_ARRAYSIZE(name),"##OceanodeDockCollapse_%08X_%08X",rootID,id);
    auto* w=ImGui::FindWindowByName(name); assert(w && w->Active); return w;
}
void click(ImGuiID id) {
    auto* w=handle(id); auto& io=ImGui::GetIO();
    io.AddMousePosEvent(w->Pos.x+w->Size.x*0.5f,w->Pos.y+w->Size.y*0.5f); idle(2);
    assert(GImGui->HoveredWindow==w);
    io.AddMouseButtonEvent(0,true); frame(); assert(GImGui->ActiveIdWindow==w);
    io.AddMouseButtonEvent(0,false); frame(); idle();
}
void jitterClick(ImGuiID id,float along,float across) {
    auto* n=node(id);int axis=n->SplitAxis;
    auto* w=handle(id);ImVec2 p(w->Pos.x+w->Size.x*0.5f,w->Pos.y+w->Size.y*0.5f);
    float before=n->ChildNodes[0]->Size[axis];
    auto& io=ImGui::GetIO();io.AddMousePosEvent(p.x,p.y);idle(2);
    io.AddMouseButtonEvent(0,true);frame();assert(GImGui->ActiveIdWindow==w);
    p[axis]+=along;p[axis^1]+=across;io.AddMousePosEvent(p.x,p.y);idle(2);
    // Hand tremor must never resize or discard the expanded-size snapshot.
    near(n->ChildNodes[0]->Size[axis],before,0.01f);
    io.AddMouseButtonEvent(0,false);frame();idle();
}
void checkDragThreshold(ImGuiID id) {
    auto* n=node(id);int axis=n->SplitAxis;auto* w=handle(id);
    ImVec2 start(w->Pos.x+w->Size.x*0.5f,w->Pos.y+w->Size.y*0.5f);
    auto& io=ImGui::GetIO();io.AddMousePosEvent(start.x,start.y);idle(2);
    float first=n->ChildNodes[0]->Size[axis];
    io.AddMouseButtonEvent(0,true);frame();
    for(float delta:{11.0f,13.0f,24.0f,14.0f}) {
        ImVec2 p=start;p[axis]+=delta;io.AddMousePosEvent(p.x,p.y);idle(2);
        near(n->ChildNodes[0]->Size[axis],first+std::max(0.0f,delta-12.0f),0.01f);
    }
    io.AddMouseButtonEvent(0,false);frame();idle();
    near(n->ChildNodes[0]->Size[axis],first+2.0f,0.01f);
    // Movement delivered only with the release must still be classified as drag.
    w=handle(id);start=ImVec2(w->Pos.x+w->Size.x*0.5f,w->Pos.y+w->Size.y*0.5f);
    io.AddMousePosEvent(start.x,start.y);idle(2);first=n->ChildNodes[0]->Size[axis];
    io.AddMouseButtonEvent(0,true);frame();
    bool trickle=io.ConfigInputTrickleEventQueue;io.ConfigInputTrickleEventQueue=false;
    start[axis]+=25.0f;io.AddMousePosEvent(start.x,start.y);io.AddMouseButtonEvent(0,false);frame();idle();
    io.ConfigInputTrickleEventQueue=trickle;
    near(n->ChildNodes[0]->Size[axis],first+13.0f,0.01f);
}
void drag(ImGuiID id,float delta,bool native=false) {
    auto* n=node(id); const int axis=n->SplitAxis,cross=axis^1;
    auto* w=handle(id); ImVec2 p(w->Pos.x+w->Size.x*0.5f,w->Pos.y+w->Size.y*0.5f);
    if(native) p[cross]=n->Pos[cross]+n->Size[cross]*0.25f;
    auto& io=ImGui::GetIO(); io.AddMousePosEvent(p.x,p.y);idle(2);
    // Handle dragging consumes the first 12 pixels as a click-tolerance zone.
    io.AddMouseButtonEvent(0,true);frame();p[axis]+=delta+(native ? 0.0f : std::copysign(12.0f,delta));io.AddMousePosEvent(p.x,p.y);idle(2);
    io.AddMouseButtonEvent(0,false);frame();idle();
}
void checkGeometry(ImGuiDockNode* n) {
    if(!n->IsVisible) return;
    if(!n->IsSplitNode()) {
        for(auto* w:n->Windows) { near(w->Pos.x,n->Pos.x);near(w->Pos.y,n->Pos.y);near(w->Size.x,n->Size.x);near(w->Size.y,n->Size.y); }
        assert(n->Size.x>0 && n->Size.y>0);return;
    }
    auto* a=n->ChildNodes[0];auto* b=n->ChildNodes[1];int axis=n->SplitAxis;
    if(a->IsVisible && b->IsVisible) {
        near(a->Size[axis]+b->Size[axis]+ImGui::GetStyle().DockingSeparatorSize,n->Size[axis]);
        near(b->Pos[axis],a->Pos[axis]+a->Size[axis]+ImGui::GetStyle().DockingSeparatorSize);
    }
    checkGeometry(a);checkGeometry(b);
}
void checkHandles(ImGuiDockNode* n) {
    if(!n->IsSplitNode())return;
    auto* w=handle(n->ID);int axis=n->SplitAxis,cross=axis^1;
    const float gutter=ImGui::GetStyle().DockingSeparatorSize;
    near(w->Size[axis],gutter*2.0f,0.01f);
    near(w->Pos[axis]+w->Size[axis]*0.5f,n->ChildNodes[0]->Pos[axis]+n->ChildNodes[0]->Size[axis]+gutter*0.5f,0.01f);
    assert(w->Size[cross]<=32.0f && w->WindowBorderSize==0 && w->WindowRounding==0);
    assert(!ofxOceanodeFloatingWindowDecor::detail::isFloatingRoot(w,ImGui::GetMainViewport()));
    // With the actual decoration hook installed, no shadow/border draw commands
    // can extend beyond the enlarged handle's clipping area.
    for(const auto& cmd:w->DrawList->CmdBuffer) if(cmd.ElemCount) {
        assert((axis==0 ? cmd.ClipRect.x : cmd.ClipRect.y)>=w->Pos[axis]-0.01f);
        assert((axis==0 ? cmd.ClipRect.z : cmd.ClipRect.w)<=w->Pos[axis]+w->Size[axis]+0.01f);
    }
    checkHandles(n->ChildNodes[0]);checkHandles(n->ChildNodes[1]);
}
float width(int i) { return node(panes[i])->Size.x; }
float height(int i) { return node(panes[i])->Size.y; }
void restoreLayout() { policy.reset();build=true;showAll=true;idle(); }
void checkThemeColors(ImGuiID id) {
    auto hasColor=[&](ImU32 color) {
        for(const auto& vertex:handle(id)->DrawList->VtxBuffer) if(vertex.col==color) return true;
        return false;
    };
    auto& io=ImGui::GetIO();auto& style=ImGui::GetStyle();
    const ImVec4 oldHover=style.Colors[ImGuiCol_ButtonHovered],oldActive=style.Colors[ImGuiCol_ButtonActive];
    style.Colors[ImGuiCol_ButtonHovered]=ImVec4(0.8f,0.1f,0.6f,1);
    style.Colors[ImGuiCol_ButtonActive]=ImVec4(0.2f,0.9f,0.1f,1);
    io.AddMousePosEvent(-100,-100);idle(2);
    assert(hasColor(ImGui::GetColorU32(ImVec4(0.25f,0.25f,0.25f,1))));
    auto* w=handle(id);io.AddMousePosEvent(w->Pos.x+w->Size.x*0.5f,w->Pos.y+w->Size.y*0.5f);idle(2);
    assert(hasColor(ImGui::GetColorU32(ImGuiCol_ButtonHovered)));
    assert(hasColor(ImGui::GetColorU32(ImGuiCol_Text)));
    io.AddMouseButtonEvent(0,true);frame();assert(hasColor(ImGui::GetColorU32(ImGuiCol_ButtonActive)));
    io.AddMouseButtonEvent(0,false);frame();idle();click(id);
    style.Colors[ImGuiCol_ButtonHovered]=oldHover;style.Colors[ImGuiCol_ButtonActive]=oldActive;
}
void checkOcclusion(ImGuiID id,bool submitAfter) {
    auto* arrow=handle(id);
    ImVec2 center(arrow->Pos.x+arrow->Size.x*0.5f,arrow->Pos.y+arrow->Size.y*0.5f);
    overlayPos=ImVec2(center.x-80,center.y-60);
    showOverlay=true;overlayAfterHandles=submitAfter;overlayClicked=false;idle();
    auto* overlay=ImGui::FindWindowByName("Floating overlay");assert(overlay && overlay->Active);
    assert(ImGui::IsWindowAbove(overlay,arrow));
    // Verify actual render order as well as ImGui's window order.
    int arrowOrder=-1,overlayOrder=-1,index=0;
    for(auto* list:ImGui::GetDrawData()->CmdLists) {
        if(list==arrow->DrawList) arrowOrder=index;
        if(list==overlay->DrawList) overlayOrder=index;
        ++index;
    }
    assert(arrowOrder>=0 && overlayOrder>arrowOrder);
    float first=node(id)->ChildNodes[0]->Size[node(id)->SplitAxis];
    auto& io=ImGui::GetIO();io.AddMousePosEvent(center.x,center.y);idle(2);
    assert(GImGui->HoveredWindow==overlay);
    io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();idle();
    assert(overlayClicked);near(node(id)->ChildNodes[0]->Size[node(id)->SplitAxis],first);
    showOverlay=false;idle();
    // Once uncovered, the same arrow remains clickable.
    click(id);click(id);near(node(id)->ChildNodes[0]->Size[node(id)->SplitAxis],first);
}
void createContext() {
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;
    io.IniFilename=nullptr;io.DisplaySize=ImVec2(1600,1000);io.DeltaTime=1.0f/60;
    ImGui::GetStyle().DockingSeparatorSize=4;ImGui::GetStyle().WindowRounding=0;
    unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    ofxOceanodeFloatingWindowDecor::install();
    policy.installSettings();
}
int main() {
    createContext();auto& io=ImGui::GetIO();idle();checkHandles(node(rootID));
    checkThemeColors(nearLeft);checkThemeColors(bottomSplit);
    checkOcclusion(nearLeft,false);checkOcclusion(bottomSplit,false);
    checkOcclusion(nearLeft,true);checkOcclusion(bottomSplit,true);
    const float minX=ImGui::GetStyle().WindowMinSize.x, minY=ImGui::GetFrameHeight();
    const float globalThreshold=io.MouseDragThreshold;
    io.MouseDragThreshold=0.1f; // Handle clicks must tolerate a sensitive global drag setting.
    const ImVec2 tremors[]={ImVec2(1,0),ImVec2(5,0),ImVec2(8,0),ImVec2(0,8),ImVec2(5,8)};
    for(const auto& tremor:tremors) {
        float left=width(0),inner=width(1),bottomHeight=height(6);
        jitterClick(nearLeft,tremor.x,tremor.y);near(width(0),minX);near(width(1),minX);
        jitterClick(nearLeft,tremor.x,tremor.y);near(width(0),left);near(width(1),inner);
        jitterClick(bottomSplit,tremor.x,tremor.y);near(height(6),minY);
        jitterClick(bottomSplit,tremor.x,tremor.y);near(height(6),bottomHeight);
    }
    // Moving well outside the handle along its line still cancels a click.
    float left=width(0);jitterClick(nearLeft,0,60);near(width(0),left);
    io.MouseDragThreshold=globalThreshold;
    checkDragThreshold(nearLeft);checkDragThreshold(bottomSplit);
    for(int variant=0;variant<2;++variant) {
        grouped=variant;restoreLayout();
        float a=width(0),b=width(1),canvas=width(2),right=width(3);
        click(farLeft);near(width(0),minX);near(width(1),b);near(width(3),right);near(width(2),canvas+a-minX);
        checkGeometry(node(rootID));idle(30);near(width(1),b);
        click(farLeft);near(width(0),a);near(width(1),b);near(width(2),canvas);
        click(nearLeft);near(width(0),minX);near(width(1),minX);near(width(2),canvas+a+b-2*minX);
        click(nearLeft);near(width(0),a);near(width(1),b);near(width(2),canvas);
        // Nested handles can open just their portion of a larger folded strip.
        click(nearLeft);click(farLeft);near(width(0),a);near(width(1),minX);
        click(nearLeft);near(width(0),minX);near(width(1),minX);
        click(nearLeft);near(width(0),a);near(width(1),b);
    }
    float r0=width(4),r1=width(3),l0=width(0),l1=width(1),top=height(5),bottom=height(6);
    click(farRight);near(width(4),minX);near(width(3),r1);click(farRight);near(width(4),r0);
    click(nearRight);near(width(4),minX);near(width(3),minX);
    click(nearLeft);near(width(0),minX);near(width(1),minX);
    click(topSplit);near(height(5),minY);near(height(6),bottom);
    click(bottomSplit);near(height(6),minY);checkGeometry(node(rootID));checkHandles(node(rootID));
    idle(30);near(height(5),minY);near(height(6),minY);
    click(nearLeft);near(width(0),l0);near(width(1),l1);near(width(3),minX);
    click(topSplit);near(height(5),top);near(height(6),minY);
    click(nearRight);near(width(3),r1);near(width(4),r0);
    click(bottomSplit);near(height(6),bottom);checkGeometry(node(rootID));
    // Folded saves restore folds on both axes AND retain their expanded sizes.
    click(nearLeft);click(bottomSplit);std::string ini=ImGui::SaveIniSettingsToMemory();
    policy.reset();ImGui::LoadIniSettingsFromMemory(ini.c_str(),ini.size());idle();
    near(width(0),minX);near(width(1),minX);near(height(6),minY);
    click(nearLeft);click(bottomSplit);
    near(width(0),l0);near(width(1),l1);near(height(6),bottom);
    // Drag each orientation and retain ordinary native splitter interaction.
    float first=node(nearLeft)->ChildNodes[0]->Size.x;drag(nearLeft,45);near(node(nearLeft)->ChildNodes[0]->Size.x,first+45);
    first=node(bottomSplit)->ChildNodes[0]->Size.y;drag(bottomSplit,-70);near(node(bottomSplit)->ChildNodes[0]->Size.y,first-70);
    click(nearLeft);drag(nearLeft,100);assert(width(0)>minX || width(1)>minX);
    first=node(farRight)->ChildNodes[0]->Size.x;drag(farRight,-40,true);near(node(farRight)->ChildNodes[0]->Size.x,first-40);
    // Host resize/movement, topology changes, and no-resize flags.
    restoreLayout();click(nearLeft);click(bottomSplit);io.DisplaySize=ImVec2(1300,800);idle();
    near(width(0),minX);near(width(1),minX);near(height(6),minY);checkGeometry(node(rootID));
    click(nearLeft);click(bottomSplit);checkGeometry(node(rootID));
    click(nearLeft);showAll=false;idle();checkGeometry(node(rootID));
    restoreLayout();auto* disabled=handle(farRight);
    node(panes[4])->SetLocalFlags(node(panes[4])->LocalFlags|ImGuiDockNodeFlags_NoResizeX);idle();assert(!disabled->Active);
    // Hidden single-window tabs are temporarily revealed while folded.
    node(panes[6])->SetLocalFlags(node(panes[6])->LocalFlags|ImGuiDockNodeFlags_HiddenTabBar);idle();
    click(bottomSplit);near(height(6),minY);assert(!(node(panes[6])->VisibleWindow->Flags&ImGuiWindowFlags_NoTitleBar));
    click(bottomSplit);assert(node(panes[6])->IsHiddenTabBar());
    // Reload into a new context (application restart), with a late node window.
    restoreLayout();
    const float savedLeft=width(0),savedInner=width(1),savedBottom=height(6);
    const std::string expandedIni=ImGui::SaveIniSettingsToMemory();
    click(nearLeft);click(bottomSplit);
    const std::string foldedIni=ImGui::SaveIniSettingsToMemory();
    assert(foldedIni.find("[OceanodeDockCollapse][State]")!=std::string::npos);
    ImGui::DestroyContext();createContext();showBottom=false;
    ImGui::LoadIniSettingsFromMemory(foldedIni.c_str(),foldedIni.size());idle(15);
    // Saving while windows are still loading must retain pending folds.
    const std::string pendingIni=ImGui::SaveIniSettingsToMemory();
    assert(pendingIni.find("[OceanodeDockCollapse][State]")!=std::string::npos);
    showBottom=true;idle(15);
    near(width(0),minX);near(width(1),minX);near(height(6),minY);
    click(nearLeft);click(bottomSplit);
    near(width(0),savedLeft);near(width(1),savedInner);near(height(6),savedBottom);
    // Loading an expanded/legacy ini clears the previous layout's folds.
    ImGui::LoadIniSettingsFromMemory(foldedIni.c_str(),foldedIni.size());idle(10);
    near(height(6),minY);
    ImGui::LoadIniSettingsFromMemory(expandedIni.c_str(),expandedIni.size());idle(10);
    near(width(0),savedLeft);near(width(1),savedInner);near(height(6),savedBottom);
    assert(std::string(ImGui::SaveIniSettingsToMemory()).find("[OceanodeDockCollapse]")==std::string::npos);
    // A second ini load replaces deferred folds before a late window appears.
    showBottom=false;idle();
    ImGui::LoadIniSettingsFromMemory(foldedIni.c_str(),foldedIni.size());idle();
    ImGui::LoadIniSettingsFromMemory(expandedIni.c_str(),expandedIni.size());showBottom=true;idle(10);
    near(height(6),savedBottom);
    // Independent axes survive repeated saves/loads; reopening retains dimensions.
    for(int repeat=0;repeat<3;++repeat) {
        click(nearLeft);click(bottomSplit);
        std::string saved=ImGui::SaveIniSettingsToMemory();
        ImGui::LoadIniSettingsFromMemory(saved.c_str(),saved.size());idle(10);
        near(width(0),minX);near(height(6),minY);
        click(nearLeft);click(bottomSplit);
        near(width(0),savedLeft);near(width(1),savedInner);near(height(6),savedBottom);
    }
    std::printf("PASS: click tremor/release tolerance, axis-only drag threshold, smooth drag start and quick release, floating-window render/input occlusion, shadow-free handles, all four directions, both left-dock topologies, preserved neighbours, nested/independent folds, restore, save/load/restart, late windows, pending-save preservation, layout replacement, handler lifetime, mouse/native drag, host resize, topology changes, disabled splits and hidden tabs.\n");
    ImGui::DestroyContext();
    // Removing a policy before its context must also remove its callbacks.
    ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
    { ofxOceanodeProportionalDocking temporary;temporary.installSettings(); }
    assert(ImGui::FindSettingsHandler("OceanodeDockCollapse")==nullptr);
    ImGui::DestroyContext();
}
'''

if __name__ == "__main__":
    with tempfile.TemporaryDirectory(prefix="oceanode-dock-collapse-") as work:
        work = Path(work)
        (work / "imconfig.h").write_text("// Standalone ImGui without OF adapters.\n")
        cpp = work / "regressions.cpp"
        cpp.write_text(CASES)
        binary = work / "regressions"
        subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++17", "-O0", "-g",
                        "-fsanitize=address,undefined", "-I", str(proportional.IMGUI), "-I", str(work),
                        "-I", str(proportional.ROOT / "src"), str(cpp),
                        *[str(proportional.IMGUI / name) for name in
                          ("imgui.cpp", "imgui_draw.cpp", "imgui_tables.cpp", "imgui_widgets.cpp")],
                        "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
