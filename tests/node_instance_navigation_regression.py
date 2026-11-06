#!/usr/bin/env python3
"""Exercise production instance navigation with real ImGui keyboard events.

Graph fixtures cover cross-canvas matching, shared Macro tabs, deterministic
ordering, wraparound, editing guards, and zero graph visits on idle frames.
"""
import os
import subprocess
import tempfile
from pathlib import Path

from canvas_navigation_regression import ROOT, IMGUI, block

FIXTURE = r'''
#include "imgui.h"
#include "imgui_internal.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <string>
#include <unordered_map>
using namespace std;
struct Position {float x=0,y=0;};
struct Gui {
    bool selected=false; Position position;
    bool getSelected(){return selected;}
    void setSelected(bool value){selected=value;}
    auto getPosition(){return position;}
};
struct Model {
    string type;
    Model(string t="Wave"):type(t){}
    virtual ~Model()=default;
    string nodeName(){return type;}
};
struct ofxOceanodeNode {
    unique_ptr<Model> model; Gui gui;
    auto& getNodeModel(){return *model;}
    auto& getNodeGui(){return gui;}
};
struct ofxOceanodeCanvas {
    string uniqueID,parentID,lastSelectedNode;
    int focused=0,raised=0,centerDelay=0;
    ofxOceanodeNode* centered=nullptr;
    void requestFocus(){++focused;}
    void bringOnTop(){++raised;}
    void requestCenterOnNode(ofxOceanodeNode* node,int delay){centered=node;centerDelay=delay;}
    void handleInstanceNavigation();
};
struct ofxOceanodeContainer {
    unordered_map<string,ofxOceanodeNode*> nodes;
    int visits=0,commentDeselections=0;
    auto& getParameterGroupNodesMap(){++visits;return nodes;}
    void deselectAllComments(){++commentDeselections;}
};
struct ofxOceanodeNodeMacro:Model {
    shared_ptr<ofxOceanodeContainer> container=make_shared<ofxOceanodeContainer>();
    ofxOceanodeCanvas canvas;
    ofxOceanodeNodeMacro* shown=this;
    string library;
    int activations=0;
    ofxOceanodeNodeMacro():Model("Macro"){}
    bool isLocal(){return library.empty();}
    bool sharesGlobalMacroWith(ofxOceanodeNodeMacro& other){return !isLocal() && library==other.library;}
    auto& windowInstance(){return *shown;}
    auto getContainer(){return container;}
    auto* getCanvas(){return &canvas;}
    void activateWindow(){++activations;}
};
struct abstractPortal:Model {
    string name,scope;
    abstractPortal(string n,string s=""):Model("Portal"),name(n),scope(s){}
    bool isMatching(abstractPortal* other){return name==other->name && scope==other->scope;}
};
struct ofxOceanodeShared {
    inline static ofxOceanodeContainer* root=nullptr;
    inline static ofxOceanodeCanvas* canvas=nullptr;
    inline static string active;
    inline static ofxOceanodeNode* selected=nullptr;
    inline static int events=0;
    static auto* getRootContainer(){return root;}
    static auto* getRootCanvas(){return canvas;}
    static const string& getActiveCanvasUniqueID(){return active;}
    static void setActiveCanvasUniqueID(const string& id){active=id;}
    static void nodeSelectedInCanvas(ofxOceanodeNode* node){selected=node;++events;}
};
'''

CASES = r'''
int main(){
    ImGui::CreateContext();auto& io=ImGui::GetIO();
    io.DisplaySize={1280,900};io.DeltaTime=1.f/60;io.IniFilename=nullptr;
    io.ConfigInputTrickleEventQueue=false;
    unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    ofxOceanodeContainer root;ofxOceanodeCanvas canvas;
    canvas.uniqueID="Canvas";
    ofxOceanodeShared::root=&root;ofxOceanodeShared::canvas=&canvas;
    ofxOceanodeShared::active=canvas.uniqueID;
    ofxOceanodeNode a{make_unique<Model>()},b{make_unique<Model>()},c{make_unique<Model>()};
    ofxOceanodeNode different{make_unique<Model>("Ranger")};
    a.gui.position.x=-100;b.gui.position.x=50;c.gui.position.x=50;
    root.nodes={{"Wave_1",&a},{"Wave_2",&b},{"Wave_3",&c},{"Ranger_1",&different}};
    a.gui.selected=true;
    bool text=false,editing=false,popup=false;
    auto frame=[&]{
        ImGui::NewFrame();ImGui::Begin("Canvas");
        if(text)io.WantTextInput=true;
        if(editing)ImGui::SetActiveID(ImGui::GetID("Editor"),ImGui::GetCurrentWindow());
        if(popup){ImGui::OpenPopup("Menu");if(ImGui::BeginPopup("Menu")){ImGui::Text("Menu");ImGui::EndPopup();}}
        ImGui::End();
        canvas.handleInstanceNavigation();
        canvas.handleInstanceNavigation(); // One jump per frame even if called twice.
        ImGui::Render();
    };
    auto tap=[&](bool right=true){
        const auto key=right?ImGuiKey_RightArrow:ImGuiKey_LeftArrow;
        io.AddKeyEvent(key,true);frame();io.AddKeyEvent(key,false);frame();
    };
    auto expect=[&](ofxOceanodeNode& node){
        assert(ofxOceanodeShared::selected==&node && node.gui.selected);
        assert(canvas.centered==&node);
    };
    for(int i=0;i<100;++i)frame();
    assert(root.visits==0); // Idle navigation never touches the graph.
    tap();assert(root.visits==0); // Arrows without Alt also do no work.
    io.AddKeyEvent(ImGuiMod_Alt,true);frame();assert(root.visits==0);
    tap();expect(b);assert(!a.gui.selected && !c.gui.selected);
    assert(canvas.lastSelectedNode=="Wave_2" && canvas.centerDelay==1);
    tap();expect(c);tap();expect(a);tap(false);expect(c);tap(false);expect(b);
    assert(ofxOceanodeShared::events==5); // Exactly one notification per jump.
    int visits=root.visits;
    for(int i=0;i<100;++i)frame();assert(root.visits==visits); // Held Alt is idle too.
    text=true;tap();text=false;frame();
    editing=true;tap();editing=false;ImGui::ClearActiveID();frame();
    popup=true;tap();popup=false;GImGui->OpenPopupStack.clear();frame();
    assert(root.visits==visits && ofxOceanodeShared::events==5);

    // Active nested canvas wins over stale root selections. Opening another
    // canvas queues centering until its layout exists. Repeated jumps keep working.
    ofxOceanodeNode outer{make_unique<ofxOceanodeNodeMacro>()};
    auto& macro=static_cast<ofxOceanodeNodeMacro&>(*outer.model);
    macro.canvas.uniqueID="Macro_1";macro.canvas.parentID="Canvas";
    root.nodes["Macro_1"]=&outer;
    ofxOceanodeNode inner{make_unique<Model>()};inner.gui.position.x=200;
    macro.container->nodes["Wave_1"]=&inner;
    inner.gui.selected=true;ofxOceanodeShared::active=macro.canvas.uniqueID;
    tap(false);expect(c);assert(!inner.gui.selected); // Active inner node, not root's stale b.
    tap();assert(ofxOceanodeShared::selected==&inner && macro.canvas.centered==&inner);
    assert(macro.activations==1 && macro.canvas.centerDelay==2);
    assert(ofxOceanodeShared::active==macro.canvas.uniqueID);
    tap();expect(a);

    // Portals still require the same name and scope, not just their node type.
    ofxOceanodeNode p{make_unique<abstractPortal>("X")},q{make_unique<abstractPortal>("X")};
    ofxOceanodeNode otherName{make_unique<abstractPortal>("Y")},otherScope{make_unique<abstractPortal>("X","Local")};
    p.gui.position.x=0;q.gui.position.x=100;
    root.nodes={{"P1",&p},{"P2",&q},{"P3",&otherName},{"P4",&otherScope}};
    p.gui.selected=true;tap();expect(q);tap();expect(p);

    // Global Macros match their library identity. Shared tab internals are
    // visited once; local Macros cycle only through other local Macros.
    ofxOceanodeNode globalA{make_unique<ofxOceanodeNodeMacro>()},globalB{make_unique<ofxOceanodeNodeMacro>()};
    ofxOceanodeNode globalOther{make_unique<ofxOceanodeNodeMacro>()},local{make_unique<ofxOceanodeNodeMacro>()};
    auto& ga=static_cast<ofxOceanodeNodeMacro&>(*globalA.model);
    auto& gb=static_cast<ofxOceanodeNodeMacro&>(*globalB.model);
    auto& go=static_cast<ofxOceanodeNodeMacro&>(*globalOther.model);
    ga.library=gb.library="Effects/Blur";go.library="Other/Blur";gb.shown=&ga;
    root.nodes={{"M1",&globalA},{"M2",&globalB},{"M3",&globalOther},{"M4",&local},{"M5",&outer}};
    globalA.gui.selected=true;tap();expect(globalB);tap();expect(globalA);
    assert(gb.container->visits==0);
    globalA.gui.selected=false;local.gui.selected=true;tap();expect(outer);tap();expect(local);

    // A singleton and an empty graph do nothing. Removed nodes aren't cached.
    root.nodes={{"Wave",&a}};a.gui.selected=true;
    const int events=ofxOceanodeShared::events;tap();assert(ofxOceanodeShared::events==events);
    root.nodes.clear();tap();assert(ofxOceanodeShared::events==events);
    ofxOceanodeShared::root=nullptr;tap();assert(ofxOceanodeShared::events==events);
    ImGui::DestroyContext();puts("Node instance navigation regression checks passed.");
}
'''


def main():
    source = (ROOT / "src/Managers/ofxOceanodeCanvas.cpp").read_text()
    navigation = block(source, "void ofxOceanodeCanvas::handleInstanceNavigation()")
    with tempfile.TemporaryDirectory(prefix="oceanode-instance-navigation-") as directory:
        work = Path(directory)
        (work / "imconfig.h").write_text("// Standalone ImGui without OF adapters.\n")
        cpp = work / "regression.cpp"
        cpp.write_text(FIXTURE + navigation + CASES)
        executable = work / "regression"
        subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++17", "-O1", "-g",
                        "-fsanitize=address,undefined", "-I", str(IMGUI), "-I", str(work), str(cpp),
                        *[str(IMGUI / name) for name in
                          ("imgui.cpp", "imgui_draw.cpp", "imgui_widgets.cpp", "imgui_tables.cpp")],
                        "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
