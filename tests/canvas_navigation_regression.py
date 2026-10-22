#!/usr/bin/env python3
"""Exercise parent navigation with real sibling ImGui and isolated OF graph types.

Uses production shortcut, node navigation, and deferred-centering code. Checks
macOS modifier mapping, nested/docked focus, one-level presses, editing guards,
and zoom-aware centering. ASan/UBSan run without a GPU or an OF host application.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
IMGUI = Path(os.environ.get("OCEANODE_TEST_IMGUI_SRC", str(ROOT.parent / "ofxImGui/libs/imgui/src")))


def block(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


FIXTURE = r'''
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
using namespace std;
namespace glm {
struct vec2 {float x,y; vec2(float a=0,float b=0):x(a),y(b){}};
vec2 operator-(vec2 a){return {-a.x,-a.y};}
vec2 operator-(vec2 a,vec2 b){return {a.x-b.x,a.y-b.y};}
vec2 operator+(vec2 a,vec2 b){return {a.x+b.x,a.y+b.y};}
vec2 operator/(vec2 a,float b){return {a.x/b,a.y/b};}
}
struct Rectangle {
    float getWidth(){return 120;} float getHeight(){return 80;}
};
struct Gui {
    bool selected=false; glm::vec2 position{350,-210};
    void setSelected(bool s){selected=s;} glm::vec2 getPosition(){return position;}
    Rectangle getRectangle(){return {};}
};
struct Model {virtual ~Model()=default;};
struct ofxOceanodeNode {
    unique_ptr<Model> model; Gui gui;
    Model& getNodeModel(){return *model;} Gui& getNodeGui(){return gui;}
};
struct ofxOceanodeCanvas {
    string uniqueID,parentID;
    bool focusPending=false,onTop=false,isFirstDraw=false;
    glm::vec2 contentRegionSize{800,600},scrolling;
    float zoomLevel=1;
    ofxOceanodeNode* pendingCenterNode=nullptr; int pendingCenterFrames=0;
    void requestFocus(){focusPending=true;} void bringOnTop(){onTop=true;}
    void requestCenterOnNode(ofxOceanodeNode* n,int frames){pendingCenterNode=n;pendingCenterFrames=max(0,frames);}
    string getUniqueID(){return uniqueID;}
    float getZoomLevel(){return zoomLevel;} glm::vec2 getContentRegionSize(){return contentRegionSize;}
    void setScrolling(glm::vec2 s){scrolling=s;}
    void handleParentNavigation(); void center();
};
struct ofxOceanodeContainer {
    map<int,unique_ptr<ofxOceanodeNode>> nodes;
    vector<ofxOceanodeNode*> getAllModules(){
        vector<ofxOceanodeNode*> out;for(auto& p:nodes)out.push_back(p.second.get());return out;
    }
    map<int,ofxOceanodeNode*> getParameterGroupNodesMap(){
        map<int,ofxOceanodeNode*> out;for(auto& p:nodes)out[p.first]=p.second.get();return out;
    }
    void deselectAllComments(){} bool showNodeInCanvas(ofxOceanodeNode& node);
};
struct ofxOceanodeNodeMacro:Model {
    shared_ptr<ofxOceanodeContainer> container=make_shared<ofxOceanodeContainer>();
    ofxOceanodeCanvas canvas; int activations=0; bool visible=true;
    auto getContainer(){return container;} auto getCanvas(){return &canvas;}
    void activateWindow(){visible=true;++activations;canvas.requestFocus();}
};
struct ofxOceanodeShared {
    inline static ofxOceanodeContainer* root=nullptr;
    inline static ofxOceanodeCanvas* canvas=nullptr;
    inline static ofxOceanodeNode* selected=nullptr;
    inline static string active;
    static auto getRootContainer(){return root;} static auto getRootCanvas(){return canvas;}
    static void setActiveCanvasUniqueID(string s){active=s;}
    static void nodeSelectedInCanvas(ofxOceanodeNode* n){selected=n;}
};
string ofToDataPath(string path){return path;}
'''

CASES = r'''
static ofxOceanodeNode* add(ofxOceanodeContainer& host,int id,bool macro=true){
    auto n=make_unique<ofxOceanodeNode>();
    if(macro)n->model=make_unique<ofxOceanodeNodeMacro>();else n->model=make_unique<Model>();
    auto* out=n.get();host.nodes[id]=std::move(n);return out;
}
static ofxOceanodeNodeMacro* macro(ofxOceanodeNode* n){
    return dynamic_cast<ofxOceanodeNodeMacro*>(&n->getNodeModel());
}
static void focus(ofxOceanodeCanvas& c,bool child=false){
    auto* w=ImGui::FindWindowByName(c.uniqueID.c_str());assert(w);
    if(child){for(auto* candidate:GImGui->Windows)if(candidate->ParentWindow==w){w=candidate;break;}}
    ImGui::FocusWindow(w);
}
static ImGuiID dock;
static ofxOceanodeCanvas* activateDuringDraw=nullptr;
static bool editing=false,popup=false,textInput=false;
static void frame(vector<ofxOceanodeCanvas*> order){
    ImGui::NewFrame();
    ImGui::DockSpaceOverViewport(dock,ImGui::GetMainViewport());
    for(auto* c:order){
        if(c->focusPending || c->onTop){ImGui::SetNextWindowFocus();c->focusPending=false;c->onTop=false;}
        ImGui::SetNextWindowDockID(dock,ImGuiCond_Always);
        ImGui::Begin(c->uniqueID.c_str(),nullptr,ImGuiWindowFlags_NoFocusOnAppearing);
        if(c==activateDuringDraw)ImGui::SetWindowFocus();
        if(editing)ImGui::SetActiveID(ImGui::GetID("Editor"),ImGui::GetCurrentWindow());
        if(popup){ImGui::OpenPopup("Menu");if(ImGui::BeginPopup("Menu")){ImGui::TextUnformatted("Menu");ImGui::EndPopup();}}
        if(textInput)ImGui::GetIO().WantTextInput=true;
        c->handleParentNavigation();
        if(c->pendingCenterNode && c->pendingCenterFrames>0)--c->pendingCenterFrames;
        c->center();
        ImGui::BeginChild("scrolling_region");ImGui::EndChild();
        ImGui::End();
    }
    ImGui::Render();
}
static void release(vector<ofxOceanodeCanvas*> order){
    ImGui::GetIO().AddKeyEvent(ImGuiKey_UpArrow,false);frame(order);
}
static void press(vector<ofxOceanodeCanvas*> order){
    ImGui::GetIO().AddKeyEvent(ImGuiKey_UpArrow,true);frame(order);
}
int main(){
    ImGui::CreateContext();auto& io=ImGui::GetIO();
    io.DisplaySize={1280,900};io.DeltaTime=1.0f/60;io.IniFilename=nullptr;
    io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;io.ConfigMacOSXBehaviors=true;
    io.ConfigInputTrickleEventQueue=false;
    unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    dock=ImHashStr("NavigationDock");
    ofxOceanodeContainer root;ofxOceanodeCanvas rootCanvas;
    rootCanvas.uniqueID="Canvas";rootCanvas.zoomLevel=2;
    ofxOceanodeShared::root=&root;ofxOceanodeShared::canvas=&rootCanvas;
    auto* outerNode=add(root,1);auto* outer=macro(outerNode);
    outer->canvas.uniqueID="Macro 1";outer->canvas.parentID="Canvas";
    outer->canvas.zoomLevel=.5f;
    auto* innerNode=add(*outer->container,1);auto* inner=macro(innerNode);
    inner->canvas.uniqueID="Macro 1 / Macro 1";inner->canvas.parentID="Macro 1";
    auto* siblingNode=add(root,2);auto* sibling=macro(siblingNode);
    sibling->canvas.uniqueID="Macro 2";sibling->canvas.parentID="Canvas";
    vector<ofxOceanodeCanvas*> order{&inner->canvas,&outer->canvas,&sibling->canvas,&rootCanvas};
    frame(order);frame(order);frame(order);

    // A focused scrolling child in a nested macro returns to its own parent.
    // A later parent draw is focused deliberately to check the per-frame guard.
    focus(inner->canvas,true);outer->visible=false;
    activateDuringDraw=&outer->canvas;
    io.AddKeyEvent(ImGuiMod_Super,true);press(order);
    assert(io.KeyCtrl && !io.KeySuper);
    assert(ofxOceanodeShared::selected==innerNode && innerNode->gui.selected);
    assert(ofxOceanodeShared::active=="Macro 1" && outer->visible && outer->activations==1);
    assert(outer->canvas.pendingCenterNode==innerNode);
    activateDuringDraw=nullptr;
    for(int i=0;i<40;++i)frame(order); // Held Up must not climb further.
    assert(ofxOceanodeShared::selected==innerNode);
    assert(outer->canvas.pendingCenterNode==nullptr);
    assert(abs(outer->canvas.scrolling.x-390)<.001 && abs(outer->canvas.scrolling.y-770)<.001);
    release(order);

    // A new press returns to the root, selecting only the containing macro.
    siblingNode->gui.selected=true;focus(outer->canvas);
    press(order);frame(order);
    assert(ofxOceanodeShared::selected==outerNode && outerNode->gui.selected && !siblingNode->gui.selected);
    assert(ofxOceanodeShared::active=="Canvas");
    assert(abs(rootCanvas.scrolling.x+210)<.001 && abs(rootCanvas.scrolling.y-320)<.001);
    release(order);focus(rootCanvas);press(order);
    assert(ofxOceanodeShared::selected==outerNode);release(order);

    // Physical macOS Ctrl and unmodified Up are not Cmd+Up.
    io.AddKeyEvent(ImGuiMod_Super,false);io.AddKeyEvent(ImGuiMod_Ctrl,true);
    focus(inner->canvas);press(order);assert(ofxOceanodeShared::selected==outerNode);release(order);
    io.AddKeyEvent(ImGuiMod_Ctrl,false);focus(inner->canvas);press(order);
    assert(ofxOceanodeShared::selected==outerNode);release(order);
    io.AddKeyEvent(ImGuiMod_Super,true);

    // Focus in another panel, active widgets, text entry, and popups block navigation.
    ImGui::NewFrame();ImGui::Begin("Inspector");ImGui::SetWindowFocus();ImGui::End();ImGui::Render();
    press(order);assert(ofxOceanodeShared::selected==outerNode);release(order);
    focus(inner->canvas);editing=true;press(order);
    assert(ofxOceanodeShared::selected==outerNode);editing=false;ImGui::ClearActiveID();release(order);
    focus(inner->canvas);textInput=true;press(order);
    assert(ofxOceanodeShared::selected==outerNode);textInput=false;release(order);
    focus(inner->canvas);popup=true;press(order);
    assert(ofxOceanodeShared::selected==outerNode);popup=false;GImGui->OpenPopupStack.clear();release(order);

    // A sibling sharing the dock must navigate through its own graph membership.
    focus(sibling->canvas);press(order);assert(ofxOceanodeShared::selected==siblingNode);release(order);
    focus(inner->canvas);ofxOceanodeShared::root=nullptr;press(order);
    assert(ofxOceanodeShared::selected==siblingNode);release(order);
    ofxOceanodeShared::root=&root;

    // The logical Ctrl shortcut also works without the macOS key swap.
    io.AddKeyEvent(ImGuiMod_Super,false);frame(order);io.ConfigMacOSXBehaviors=false;
    io.AddKeyEvent(ImGuiMod_Ctrl,true);focus(inner->canvas);press(order);
    assert(ofxOceanodeShared::selected==innerNode);release(order);
    ImGui::DestroyContext();puts("Canvas parent navigation regression checks passed.");
}
'''


def main():
    canvas = (ROOT / "src/Managers/ofxOceanodeCanvas.cpp").read_text()
    container = (ROOT / "src/Managers/ofxOceanodeContainer.cpp").read_text()
    navigation = block(canvas, "void ofxOceanodeCanvas::handleParentNavigation()")
    show_node = block(container, "bool ofxOceanodeContainer::showNodeInCanvas(")
    center = block(canvas, "if(pendingCenterNode != nullptr &&\n")
    with tempfile.TemporaryDirectory(prefix="oceanode-canvas-navigation-") as directory:
        work = Path(directory)
        (work / "imconfig.h").write_text("// Standalone ImGui without OF adapters.\n")
        cpp = work / "regression.cpp"
        cpp.write_text(FIXTURE + navigation + show_node +
                       "\nvoid ofxOceanodeCanvas::center(){" + center + "}\n" + CASES)
        executable = work / "regression"
        subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++17", "-O1", "-g",
                        "-fsanitize=address,undefined", "-I", str(IMGUI), "-I", str(work), str(cpp),
                        *[str(IMGUI / name) for name in
                          ("imgui.cpp", "imgui_draw.cpp", "imgui_widgets.cpp", "imgui_tables.cpp")],
                        "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
