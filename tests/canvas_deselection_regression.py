#!/usr/bin/env python3
"""Exercise production global deselection and background clicks with real ImGui.

Covers main, nested and sibling macros, local hidden-canvas cleanup, and guards
for widgets, node clicks, panning, dragging, and releases outside the canvas.
"""
from pathlib import Path
import sys, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'tests'))
from canvas_navigation_regression import block, IMGUI
source=(root/'src/Managers/ofxOceanodeCanvas.cpp').read_text()
fixture=r'''
#include "imgui.h"
#include <cassert>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <string>
using namespace std;
struct Gui {bool selected=false;bool getSelected(){return selected;}void setSelected(bool b){selected=b;}};
struct Model {virtual ~Model()=default;};
struct Node {Gui gui;Model model;Model* actual=&model;Gui& getNodeGui(){return gui;}Model& getNodeModel(){return *actual;}};
struct ofxOceanodeContainer {map<string,Node*> nodes;bool comment=false;auto& getParameterGroupNodesMap(){return nodes;}void deselectAllComments(){comment=false;}};
struct ofxOceanodeNodeMacro:Model {shared_ptr<ofxOceanodeContainer> container=make_shared<ofxOceanodeContainer>();auto getContainer(){return container;}};
struct ofxOceanodeShared {inline static ofxOceanodeContainer* root=nullptr;inline static int notifications=0;static auto getRootContainer(){return root;}static void nodeSelectedInCanvas(Node* n){assert(n==nullptr);++notifications;}};
struct ofxOceanodeCanvas {shared_ptr<ofxOceanodeContainer> container;bool canvasHasScolled=false,someDragAppliedToSelection=false;void deselectAllNodes(bool allCanvases=false);void background(bool isAnyNodeHovered){PRODUCTION_BACKGROUND}};
PRODUCTION_DESELECT
int main(){
    auto root=make_shared<ofxOceanodeContainer>();
    ofxOceanodeNodeMacro macro, nested, sibling;
    Node mainNode,macroNode,nestedNode,siblingNode,leaf,deepLeaf,peerLeaf;
    macroNode.actual=&macro;nestedNode.actual=&nested;siblingNode.actual=&sibling;
    root->nodes={{"main",&mainNode},{"macro",&macroNode},{"sibling",&siblingNode}};
    macro.container->nodes={{"leaf",&leaf},{"nested",&nestedNode}};
    nested.container->nodes={{"deep",&deepLeaf}};sibling.container->nodes={{"peer",&peerLeaf}};
    ofxOceanodeShared::root=root.get();
    ofxOceanodeCanvas main{root}, child{macro.container};
    auto select=[&]{for(auto* n:{&mainNode,&macroNode,&nestedNode,&siblingNode,&leaf,&deepLeaf,&peerLeaf})n->gui.selected=true;root->comment=macro.container->comment=nested.container->comment=sibling.container->comment=true;};
    auto allClear=[&]{for(auto* n:{&mainNode,&macroNode,&nestedNode,&siblingNode,&leaf,&deepLeaf,&peerLeaf})assert(!n->gui.selected);assert(!root->comment&&!macro.container->comment&&!nested.container->comment&&!sibling.container->comment);};
    // Hidden canvases must still clear only their own graph.
    select();child.deselectAllNodes();assert(mainNode.gui.selected&&deepLeaf.gui.selected&&peerLeaf.gui.selected&&!leaf.gui.selected);
    // Global clearing works from any canvas, including a deeply nested macro.
    select();child.deselectAllNodes(true);allClear();int count=ofxOceanodeShared::notifications;
    child.deselectAllNodes(true);assert(count==ofxOceanodeShared::notifications);
    select();main.deselectAllNodes(true);allClear();
    // A canvas also works before the global root is registered.
    ofxOceanodeShared::root=nullptr;select();child.deselectAllNodes(true);assert(!leaf.gui.selected&&!deepLeaf.gui.selected&&mainNode.gui.selected);ofxOceanodeShared::root=root.get();
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.DisplaySize={900,600};io.DeltaTime=1.f/60;io.IniFilename=nullptr;
    unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    auto frame=[&](bool hoverNode=false,bool pan=false,bool drag=false){
        ImGui::NewFrame();
        for(int i=0;i<2;++i){
            auto& canvas=i==0?main:child;
            ImGui::SetNextWindowPos(ImVec2(20+i*430,20));ImGui::SetNextWindowSize(ImVec2(400,500));
            ImGui::Begin(i==0?"Main":"Macro",nullptr,ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize);
            ImGui::Button("Widget");
            if(ImGui::IsWindowFocused()){canvas.canvasHasScolled=pan;canvas.someDragAppliedToSelection=drag;canvas.background(hoverNode);}
            ImGui::End();
        }
        ImGui::Render();
    };
    auto click=[&](float x,float y,bool hoverNode=false,bool pan=false,bool drag=false){
        io.AddMousePosEvent(x,y);frame();frame();io.AddMouseButtonEvent(0,true);frame(hoverNode,pan,drag);io.AddMouseButtonEvent(0,false);frame(hoverNode,pan,drag);
    };
    frame();frame();
    select();click(150,300);allClear();
    select();click(600,300);allClear();
    select();click(600,300,true);assert(mainNode.gui.selected&&deepLeaf.gui.selected);
    select();click(600,300,false,true);assert(mainNode.gui.selected&&deepLeaf.gui.selected);
    select();click(600,300,false,false,true);assert(mainNode.gui.selected&&deepLeaf.gui.selected);
    select();click(480,55);assert(mainNode.gui.selected&&deepLeaf.gui.selected);
    // Releasing outside the focused canvas is not a background click there.
    select();io.AddMousePosEvent(600,300);frame();io.AddMouseButtonEvent(0,true);frame();io.AddMousePosEvent(870,550);frame();io.AddMouseButtonEvent(0,false);frame();assert(mainNode.gui.selected&&deepLeaf.gui.selected);
    ImGui::DestroyContext();puts("PASS: global background deselection in main/macro canvases, nested/sibling/hidden graphs, local cleanup, one notification, root fallback, widget/node clicks, panning, dragging and release outside canvas.");
}
'''
release=block(source,'if(ImGui::IsMouseReleased(0)){\n                if(canvasHasScolled)')
release=release[:release.index('                // Apply snap-to-grid')]+'}\n'
fixture=fixture.replace('PRODUCTION_BACKGROUND',release).replace('PRODUCTION_DESELECT',block(source,'void ofxOceanodeCanvas::deselectAllNodes('))
with tempfile.TemporaryDirectory(prefix='oceanode-deselect-') as directory:
    work=Path(directory);(work/'imconfig.h').write_text('');cpp=work/'check.cpp';cpp.write_text(fixture)
    binary=work/'check'
    subprocess.run(['clang++','-std=c++17','-O0','-g','-fsanitize=address,undefined','-I',str(IMGUI),'-I',str(work),str(cpp),*[str(IMGUI/n) for n in ('imgui.cpp','imgui_draw.cpp','imgui_widgets.cpp','imgui_tables.cpp')],'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
