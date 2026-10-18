#!/usr/bin/env python3
"""Check selection centering through real ImGui Inspector docking transitions.

Uses production Controls::draw and canvas anchor methods with isolated graph
types. Covers node sizes, both draw orders, zoom, dragging, switching nodes/floating layouts,
disabled auto Inspector, cancellation, and expired anchors. No GPU/OF host.
"""
from pathlib import Path
import os
import subprocess
import tempfile

from canvas_navigation_regression import ROOT, IMGUI, block
from timeline_regression import of_root


FIXTURE = r'''
#include "imgui.h"
#include "imgui_internal.h"
#include <glm/glm.hpp>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>
using namespace std;
struct ofxOceanodeNodeGui {
    bool selected=false; glm::vec2 position{750,-210};
    float width=240,height=80;
    struct Rectangle {
        float width,height;
        float getWidth() const {return width;}
        float getHeight() const {return height;}
    };
    Rectangle getRectangle() const {return {width,height};}
    bool getSelected(){return selected;}
    glm::vec2 getPosition(){return position;}
};
struct ofxOceanodeNode {
    ofxOceanodeNodeGui gui;
    auto& getNodeGui(){return gui;}
};
struct ofxOceanodeContainer {
    map<string,ofxOceanodeNode*> nodes;
    const auto& getParameterGroupNodesMap(){return nodes;}
};
struct ofxOceanodeShared {
    inline static bool automatic=true,focus=false;
    inline static ImGuiID left=0;
    static bool getAutoInspectorShowHide(){return automatic;}
    static bool consumeInspectorFocusRequest(){bool f=focus;focus=false;return f;}
    static ImGuiID getLeftNodeID(){return left;}
};
struct ofxOceanodeBaseController {
    virtual ~ofxOceanodeBaseController()=default;
    virtual string getControllerName(){return "Inspector";}
    bool isMenuController(){return false;}
    void draw(){ImGui::TextUnformatted("Large Inspector");}
};
struct ofxOceanodeInspectorController:ofxOceanodeBaseController {
    ofxOceanodeNode* node=nullptr;
    bool hasAnySelectedNode(){return node && node->gui.selected;}
};
struct ofxOceanodeTimelineController:ofxOceanodeBaseController {
    void drawPendingTrackPopup(){}
};
struct ofxOceanodeControls {
    vector<shared_ptr<ofxOceanodeBaseController>> controllers;
    map<string,bool> controllerVisible{{"Inspector",true}};
    ImGuiID inspectorPreviousTabID=0,inspectorPreviousDockID=0;
    template<class T> shared_ptr<T> get(){
        for(auto& c:controllers) if(auto out=dynamic_pointer_cast<T>(c))return out;
        return nullptr;
    }
    void draw();
};
struct ofxOceanodeCanvas {
    ofxOceanodeContainer* container;
    glm::vec2 canvasOrigin{0},contentRegionSize{0},scrolling{-400,450};
    float zoomLevel=1;
    // ANCHOR_STATE
    glm::vec2 screenToWorld(glm::vec2 p) const {return (p-canvasOrigin)/zoomLevel-scrolling;}
    glm::vec2 worldToScreen(glm::vec2 p) const {return canvasOrigin+(p+scrolling)*zoomLevel;}
    void rememberInspectorSelectionClick(const string&);
    void recenterSelectionAfterInspectorLayout();
};
'''


CASES = r'''
static void near(glm::vec2 a,glm::vec2 b){
    if(glm::distance(a,b)>.02f){
        fprintf(stderr,"expected (%f,%f), got (%f,%f)\n",b.x,b.y,a.x,a.y);abort();
    }
}
static void checkCentered(ofxOceanodeCanvas& canvas,ofxOceanodeNodeGui& gui){
    const auto topLeft=canvas.worldToScreen(gui.position);
    const auto bottomRight=canvas.worldToScreen(gui.position+glm::vec2(gui.width,gui.height));
    // Equal opposite margins verify both axes independently of the click point.
    near(topLeft-canvas.canvasOrigin,canvas.canvasOrigin+canvas.contentRegionSize-bottomRight);
}
static void scenario(bool macroFirst,float zoom,int layout,int button,float nodeWidth,float nodeHeight,bool drag=false){
    ImGui::CreateContext();auto& io=ImGui::GetIO();
    io.DisplaySize={1400,900};io.DeltaTime=1.f/60;io.IniFilename=nullptr;
    io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;io.ConfigInputTrickleEventQueue=false;
    unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    ofxOceanodeShared::automatic=true;ofxOceanodeShared::focus=false;
    ofxOceanodeNode node;ofxOceanodeContainer graph;graph.nodes["Node_1"]=&node;
    node.gui.width=nodeWidth;node.gui.height=nodeHeight;
    ofxOceanodeCanvas canvas;canvas.container=&graph;canvas.zoomLevel=zoom;
    // A second canvas must retain its own camera throughout the layout change.
    ofxOceanodeCanvas other;other.container=&graph;
    const glm::vec2 otherScroll=other.scrolling;
    ofxOceanodeControls controls;
    auto inspector=make_shared<ofxOceanodeInspectorController>();inspector->node=&node;
    controls.controllers.push_back(inspector);
    const char* name=macroFirst?"Macro Canvas":"Canvas";
    bool build=true,capture=false;
    auto frame=[&]{
        ImGui::NewFrame();
        ImGuiID root=ImHashStr("InspectorDock");
        if(build){
            ImGui::DockBuilderAddNode(root,ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(root,io.DisplaySize);
            ImGuiID canvasDock=root;
            if(layout==0 || layout==1){
                ImGui::DockBuilderSplitNode(root,layout==0?ImGuiDir_Left:ImGuiDir_Up,
                    .6f,&ofxOceanodeShared::left,&canvasDock);
                ImGui::DockBuilderDockWindow("Inspector",ofxOceanodeShared::left);
            }else{
                ofxOceanodeShared::left=0; // Floating Inspector never resizes the canvas.
            }
            ImGui::DockBuilderDockWindow(name,canvasDock);
            ImGui::DockBuilderFinish(root);build=false;
        }
        ImGui::DockSpaceOverViewport(root,ImGui::GetMainViewport());
        auto drawCanvas=[&]{
            ImGui::Begin(name,nullptr,ImGuiWindowFlags_NoFocusOnAppearing);
            ImGui::TextUnformatted("Canvas toolbar");
            ImGui::BeginChild("scrolling_region",ImVec2(0,0),true,
                ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
            canvas.contentRegionSize=glm::vec2(ImGui::GetContentRegionAvail());
            canvas.canvasOrigin=glm::vec2(ImGui::GetCursorScreenPos());
            canvas.recenterSelectionAfterInspectorLayout();
            other.recenterSelectionAfterInspectorLayout();
            if(capture){
                capture=false;node.gui.selected=true;
                // Select near the header rather than halfway down the node.
                node.gui.position=canvas.screenToWorld(glm::vec2(ImGui::GetMousePos()))-glm::vec2(35,10);
                canvas.rememberInspectorSelectionClick("Node_1");
                if(button==1)ofxOceanodeShared::focus=true;
            }
            ImGui::EndChild();ImGui::End();
        };
        if(macroFirst){drawCanvas();controls.draw();}
        else{controls.draw();drawCanvas();}
        ImGui::Render();
    };
    auto idle=[&](int count){for(int i=0;i<count;++i)frame();};
    idle(6);
    const auto oldSize=canvas.contentRegionSize;
    const auto oldScroll=canvas.scrolling;
    // An off-center click near the right/bottom edge would be hidden by the new dock.
    const glm::vec2 mouse=canvas.canvasOrigin+canvas.contentRegionSize*glm::vec2(.85f,.75f);
    io.AddMousePosEvent(mouse.x,mouse.y);frame();
    io.AddMouseButtonEvent(button,true);capture=true;frame();
    idle(drag?20:2);
    near(canvas.scrolling,oldScroll); // Never jump while a button is held.
    assert(!controls.controllerVisible["Inspector"]);
    glm::vec2 movement=drag?glm::vec2(90,-65):glm::vec2(0);
    node.gui.position+=movement;
    io.AddMouseButtonEvent(button,false);frame();
    near(canvas.scrolling,oldScroll); // Release must finish before docking changes.
    idle(4);
    assert(controls.controllerVisible["Inspector"]);
    if(layout<2)assert(glm::distance(canvas.contentRegionSize,oldSize)>100);
    checkCentered(canvas,node.gui);
    near(other.scrolling,otherScroll);

    // Once settled, future window resizing must preserve the user's camera.
    idle(6);auto settled=canvas.scrolling;
    io.DisplaySize.x+=200;idle(3);near(canvas.scrolling,settled);
    // Switching to a different node must center its bounds even when the open
    // Inspector leaves both canvas size and position unchanged.
    ofxOceanodeNode secondNode;graph.nodes["Node_2"]=&secondNode;
    const auto switchOrigin=canvas.canvasOrigin,switchSize=canvas.contentRegionSize;
    const auto secondMouse=switchOrigin+switchSize*glm::vec2(.2f,.3f);
    io.AddMousePosEvent(secondMouse.x,secondMouse.y);frame();
    io.AddMouseButtonEvent(button,true);frame();
    node.gui.selected=false;secondNode.gui.selected=true;inspector->node=&secondNode;
    secondNode.gui.height=nodeHeight*.5f+90;
    secondNode.gui.width=nodeWidth*.5f+70;
    // Select near the opposite corner to ensure neither coordinate anchors to the click.
    secondNode.gui.position=canvas.screenToWorld(glm::vec2(ImGui::GetMousePos()))-
        glm::vec2(secondNode.gui.width-10,secondNode.gui.height-15);
    canvas.rememberInspectorSelectionClick("Node_2");
    idle(2);near(canvas.scrolling,settled);
    io.AddMouseButtonEvent(button,false);frame();near(canvas.scrolling,settled);
    idle(4);
    near(canvas.canvasOrigin,switchOrigin);near(canvas.contentRegionSize,switchSize);
    checkCentered(canvas,secondNode.gui);
    assert(glm::distance(canvas.scrolling,settled)>1);
    near(other.scrolling,otherScroll);
    idle(6);settled=canvas.scrolling;
    secondNode.gui.selected=false;node.gui.selected=true;inspector->node=&node;

    // Cancellation prevents stale recentering after deselection/removal or opting out.
    auto cancelled=[&](int reason){
        canvas.rememberInspectorSelectionClick("Node_1");
        if(reason==0)node.gui.selected=false;
        if(reason==1)graph.nodes.clear();
        if(reason==2)ofxOceanodeShared::automatic=false;
        if(reason==3)io.AddMouseWheelEvent(0,1);
        frame();assert(canvas.inspectorSelectionClick.nodeID.empty());
        near(canvas.scrolling,settled);
        node.gui.selected=true;graph.nodes["Node_1"]=&node;ofxOceanodeShared::automatic=true;
        idle(6);
    };
    for(int reason=0;reason<4;++reason)cancelled(reason);
    // A position-only change also centers the node, even with identical size.
    canvas.rememberInspectorSelectionClick("Node_1");
    canvas.canvasOrigin+=glm::vec2(40,25);
    canvas.recenterSelectionAfterInspectorLayout();
    checkCentered(canvas,node.gui);
    canvas.inspectorSelectionClick={};
    ofxOceanodeShared::automatic=false;
    canvas.rememberInspectorSelectionClick("Node_1");
    assert(canvas.inspectorSelectionClick.nodeID.empty());
    ImGui::DestroyContext();
}
int main(){
    for(bool macro:{false,true})for(float zoom:{.25f,1.f,2.f})
    for(float width:{80.f,320.f,900.f})for(float height:{60.f,600.f,1600.f}){
        scenario(macro,zoom,0,0,width,height);
        scenario(macro,zoom,1,1,width,height,true);
        scenario(macro,zoom,2,0,width,height);
    }
    puts("Inspector selection centering passed: 162 size/dock/zoom/draw-order scenarios plus cancellation checks.");
}
'''


def main():
    canvas = (ROOT / "src/Managers/ofxOceanodeCanvas.cpp").read_text()
    header = (ROOT / "src/Managers/ofxOceanodeCanvas.h").read_text()
    controls = (ROOT / "src/Controls/ofxOceanodeControls.cpp").read_text()
    state = block(header, "struct InspectorSelectionClick") + " inspectorSelectionClick;"
    with tempfile.TemporaryDirectory(prefix="oceanode-inspector-selection-") as directory:
        work = Path(directory)
        (work / "imconfig.h").write_text(
            '#include <glm/glm.hpp>\n'
            '#define IM_VEC2_CLASS_EXTRA '
            'ImVec2(const glm::vec2& v):x(v.x),y(v.y){} '
            'operator glm::vec2() const {return {x,y};}\n')
        cpp = work / "regression.cpp"
        cpp.write_text(FIXTURE.replace("// ANCHOR_STATE", state) +
                       block(controls, "void ofxOceanodeControls::draw()") +
                       block(canvas, "void ofxOceanodeCanvas::rememberInspectorSelectionClick(") +
                       block(canvas, "void ofxOceanodeCanvas::recenterSelectionAfterInspectorLayout()") + CASES)
        binary = work / "regression"
        subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++17", "-O1", "-g",
                        "-fsanitize=address,undefined", "-I", str(IMGUI), "-I", str(work),
                        "-I", str(of_root() / "libs/glm/include"), str(cpp),
                        *[str(IMGUI / name) for name in
                          ("imgui.cpp", "imgui_draw.cpp", "imgui_widgets.cpp", "imgui_tables.cpp")],
                        "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
