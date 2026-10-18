#!/usr/bin/env python3
"""Exercise Scopes docking against the sibling ofxImGui source, without an OF app.

Runs the production docking helpers and draw flow with text-only scope contents.
The fixture replaces parameter/rendering dependencies; ImGui and its queued dock
operations are real. Requires Python 3 and clang++. ASan/UBSan are enabled.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
IMGUI = Path(os.environ.get("OCEANODE_TEST_IMGUI_SRC", str(ROOT.parent / "ofxImGui/libs/imgui/src")))

FIXTURE_PREFIX = r'''
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <functional>
#include <string>
#include <vector>
#include <cstdio>
#include <cassert>
#include <random>
struct ofxOceanodeScopeItem { std::string windowName; void* parameter=nullptr; };
struct Config { bool hasConfig=false; float posX=40,posY=40,width=800,height=600; };
class ofxOceanodeScope {
public:
    std::vector<ofxOceanodeScopeItem> scopedParameters;
    struct Pending {std::string windowName; size_t scopeCountAtAdd;};
    std::vector<Pending> pendingDockWindows;
    Config windowConfig, lastWindowConfig;
    bool dockMaintenancePending=true;
    unsigned long long lastSettledDockLayoutSignature=0;
    bool isLoadingFromPreset=false;
    unsigned int lastCentralScopeWindowID=0;
    ImGuiID lastDockspace=0;
    static ofxOceanodeScope* instance;
    static ofxOceanodeScope* getInstance(){return instance;}
    void removeParameter(void*){}
    int notifications=0;
    void notifyScopeChanged(){++notifications;}
    void draw();
};
ofxOceanodeScope* ofxOceanodeScope::instance=nullptr;
'''
FIXTURE_TESTS = r'''
static ImGuiDockNode* queuedTarget = nullptr;
static std::function<void()> beforeDraw;
static void frame(ofxOceanodeScope& s) {ImGui::NewFrame(); if(beforeDraw) beforeDraw(); s.draw(); ImGui::Render();}
static void checkSettled(ofxOceanodeScope& s) {
    assert(!s.dockMaintenancePending);
    auto* root=ImGui::DockBuilderGetNode(s.lastDockspace);
    auto* central=FindLiveScopeCentralNode(root);
    assert(central && FindCurrentScopeWindowInNode(s.scopedParameters,central));
    for(auto& item:s.scopedParameters) assert(IsScopeWindowContained(ImGui::FindWindowByName(item.windowName.c_str()),s.lastDockspace));
}
static void idle(ofxOceanodeScope& s, int count=6){for(int i=0;i<count;i++)frame(s);checkSettled(s);}
int main(){
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;io.IniFilename=nullptr;io.DisplaySize=ImVec2(1200,900);io.DeltaTime=1.0f/60;
    unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    ofxOceanodeScope s;ofxOceanodeScope::instance=&s;s.windowConfig.hasConfig=true;
    s.scopedParameters={{"A###Scope_A"},{"B###Scope_B"},{"C###Scope_C"},{"D###Scope_D"}};
    for(size_t i=0;i<s.scopedParameters.size();i++)s.pendingDockWindows.push_back({s.scopedParameters[i].windowName,i+1});
    idle(s,10);
    int notifications=s.notifications;
    auto initialSignature=s.lastSettledDockLayoutSignature;
    idle(s,20);assert(s.notifications==notifications);assert(initialSignature==s.lastSettledDockLayoutSignature);
    // No mouse button/hover tracking is needed to recover an outside release.
    ImGui::DockContextQueueUndockWindow(GImGui,ImGui::FindWindowByName("A###Scope_A"));idle(s);
    // Do not interfere while the mouse is still held, even without hover flags.
    ImGui::DockContextQueueUndockWindow(GImGui,ImGui::FindWindowByName("B###Scope_B"));
    io.AddMouseButtonEvent(0,true);frame(s);assert(!IsScopeWindowContained(ImGui::FindWindowByName("B###Scope_B"),s.lastDockspace));
    io.AddMouseButtonEvent(0,false);idle(s);
    // Simultaneous escaped scopes and full-node (grouped-tab) undocking.
    auto* a=ImGui::FindWindowByName("A###Scope_A");auto* b=ImGui::FindWindowByName("B###Scope_B");
    ImGui::DockContextQueueUndockWindow(GImGui,a);ImGui::DockContextQueueUndockWindow(GImGui,b);idle(s);
    ImGui::DockBuilderDockWindow(a->Name,b->DockId);ImGui::DockBuilderFinish(s.lastDockspace);idle(s);
    assert(a->DockNode==b->DockNode);ImGui::DockContextQueueUndockNode(GImGui,a->DockNode);idle(s);
    // Controlled drop delivery precedes drawing. Repair must preserve its target.
    ImGui::DockContextQueueUndockWindow(GImGui,a);
    beforeDraw=[&](){b=ImGui::FindWindowByName("B###Scope_B");queuedTarget=b->DockNode;ImGui::DockContextQueueDock(GImGui,b,b->DockNode,a,ImGuiDir_Left,0.5f,false);beforeDraw=nullptr;};
    frame(s);assert(ImGui::DockBuilderGetNode(b->DockId)==queuedTarget);idle(s);
    // Hundreds of queued split/tab drops, including central-node moves.
    std::mt19937 random(42);
    for(int n=0;n<400;n++){
        size_t source=random()%s.scopedParameters.size(),target=(source+1+random()%3)%s.scopedParameters.size();
        auto* payload=ImGui::FindWindowByName(s.scopedParameters[source].windowName.c_str());
        ImGui::DockContextQueueUndockWindow(GImGui,payload);
        beforeDraw=[&,target,payload,n](){auto* dst=ImGui::FindWindowByName(s.scopedParameters[target].windowName.c_str());assert(dst->DockNode);ImGui::DockContextQueueDock(GImGui,dst,dst->DockNode,payload,n%5==4?ImGuiDir_None:ImGuiDir(n%4),0.4f,false);beforeDraw=nullptr;};
        frame(s);idle(s);
    }
    // Saved layout survives restoration and healthy idle frames unchanged.
    size_t iniSize;std::string ini(ImGui::SaveIniSettingsToMemory(&iniSize));
    ImGui::LoadIniSettingsFromMemory(ini.c_str(),ini.size());idle(s);
    auto restored=s.lastSettledDockLayoutSignature;idle(s,20);assert(restored==s.lastSettledDockLayoutSignature);
    // Container resize and removal of the central scope.
    s.windowConfig.hasConfig=true;s.windowConfig.width=1000;s.windowConfig.height=750;idle(s);
    auto* central=FindLiveScopeCentralNode(ImGui::DockBuilderGetNode(s.lastDockspace));auto* removed=FindCurrentScopeWindowInNode(s.scopedParameters,central);
    s.scopedParameters.erase(std::remove_if(s.scopedParameters.begin(),s.scopedParameters.end(),[&](const auto& item){return item.windowName==removed->Name;}),s.scopedParameters.end());s.dockMaintenancePending=true;idle(s);
    printf("PASS: idle stability, outside release, held drag, multiple floats, grouped tabs, queued drops, 400 rearrangements, saved layout, resize, central removal.\n");
    ImGui::DestroyContext();
}
'''


def main():
    source = (ROOT / "src/Managers/ofxOceanodeScope.cpp").read_text()
    helpers = source[source.index("static void RescaleDockNodeProportional"):
                     source.index("void ofxOceanodeScope::addScopeRenderer")]
    start = helpers.index("static std::string BuildScopeWindowName")
    end = helpers.index("static ImGuiWindow* FindScopeWindowToFillCentralNode", start)
    helpers = helpers[:start] + helpers[end:]
    draw = source[source.index("void ofxOceanodeScope::draw()"):
                  source.index("bool ofxOceanodeScope::addParameter(")]
    start = draw.index("            ImGui::PushStyleColor(")
    end = draw.index("            ImGui::PopStyleColor(5);") + len("            ImGui::PopStyleColor(5);")
    draw = draw[:start] + "            ImGui::TextUnformatted(p.windowName.c_str());" + draw[end:]
    draw = draw.replace('const ImGuiID dockspace_id = ImGui::GetID("ScopesDockSpace");',
                        'const ImGuiID dockspace_id = ImGui::GetID("ScopesDockSpace"); lastDockspace=dockspace_id;')
    with tempfile.TemporaryDirectory(prefix="oceanode-scope-docking-") as work:
        work = Path(work)
        (work / "imconfig.h").write_text("// Standalone ImGui: no openFrameworks adapters.\n")
        cpp = work / "regressions.cpp"
        cpp.write_text(FIXTURE_PREFIX + helpers + draw + FIXTURE_TESTS)
        binary = work / "regressions"
        subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++17", "-O0", "-g",
                        "-fsanitize=address,undefined", "-I", str(IMGUI), "-I", str(work),
                        str(cpp), *[str(IMGUI / name) for name in
                                    ("imgui.cpp", "imgui_draw.cpp", "imgui_tables.cpp", "imgui_widgets.cpp")],
                        "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
