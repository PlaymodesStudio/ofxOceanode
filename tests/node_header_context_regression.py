#!/usr/bin/env python3
"""Exercise production header selection/context handling with real ImGui events."""
from pathlib import Path
import os
import subprocess
import tempfile
from new_preset_regression import block
ROOT=Path(__file__).resolve().parents[1]
IMGUI=ROOT.parent/'ofxImGui/libs/imgui/src'
FIXTURE=r'''
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cassert>
#include <map>
#include <string>
#include <vector>
#include <cstdio>
using namespace std;
constexpr int ofxOceanodeNodeModelFlags_TransparentNode=1<<3;
struct ofxOceanodeTimelineInstrument {int* gate=nullptr;int* pitch=nullptr;};
struct Model {virtual ~Model()=default;int getFlags(){return 0;} string getNodeTitleSuffix(){return "Macro Name";}bool getTimelineInstrument(ofxOceanodeTimelineInstrument&){return false;}};
struct ofxOceanodeNodeMacro : Model {bool isActive(){return true;}};
struct Gui {bool selected=false,expanded=true,headerControlHovered=false;bool isHeaderControlHovered(){return headerControlHovered;}bool getSelected(){return selected;}void setSelected(bool v){selected=v;}};
struct Group {string getEscapedName(){return "Node_1";}};
struct Node {int index=0;Model model;Gui gui;Group group;auto& getNodeModel(){return model;}auto& getParameters(){return group;}};
Node nodes[2];
int notifications=0,inspectorAnchors=0;
struct ofxOceanodeShared {
    static ImFont* getCurrentBoldFont(){return nullptr;}
    static void nodeSelectedInCanvas(Node*){++notifications;}
};
struct OceanodeColors {inline static ImVec4 TransparentButton{0,0,0,0};};
struct Container {
    struct Timeline {void requestNoteGroupSetup(int*){}} timeline;
    auto& getTimelineManager(){return timeline;}
    vector<Node*> getSelectedModules(){vector<Node*> out;for(auto& n:nodes)if(n.gui.selected)out.push_back(&n);return out;}
} containerStorage;
auto* container=&containerStorage;
void deselectAllNodes(){for(auto& n:nodes)n.gui.selected=false;}
void rememberInspectorSelectionClick(const string&){++inspectorAnchors;}
void rememberInspectorSelectionClick(const string& id,Gui&){rememberInspectorSelectionClick(id);}
vector<int*> getPortalizableOutputs(const vector<Node*>&){return {};}
vector<int*> getPortalizableInputConnections(Node*){return {};}
void portalizeOutput(int*){} void portalizeInput(int*){} void autoLayoutNodes(const vector<Node*>&){}
map<string,int> nodesDrawingOrder{{"Node_1",1},{"Node_2",0}};
string lastSelectedNode,macroExtractionKey;
bool someDragAppliedToSelection=false,popup=false,overlap=false;
Node* customGuiContextNode=nullptr;
ImVec2 titlePoints[2],arrowPoint,closePoint,bodyPoint,cancelPoint;
float zoomLevel=1, nodeWidthText=150,nodeWidthWidget=150;
void header(Node& node){
    bool& expanded=node.gui.expanded;bool& headerControlHovered=node.gui.headerControlHovered;headerControlHovered=false;
    bool deleteModule=false,isTransparent=false;string moduleName="Node "+to_string(node.index+1);
    PRODUCTION_HEADER
}
void frame(){
    ImGui::NewFrame();ImGui::SetNextWindowPos(ImVec2(20,20));ImGui::SetNextWindowSize(ImVec2(1600,900));
    ImGui::Begin("Canvas");ImGui::SetWindowFontScale(zoomLevel);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(2*zoomLevel,2*zoomLevel));
    bool nodeHoverClaimed=false,open_context_menu=false,isAnyNodeHovered=false;
    string node_hovered_in_scene;
    for(int i=0;i<2;++i){
        auto* node=&nodes[i];auto& nodeGui=node->gui;string nodeId="Node_"+to_string(i+1);
        ImGui::PushID(nodeId.c_str());
        const ImVec2 node_rect_min(60+(overlap?0:i*700),80);
        ImGui::SetCursorScreenPos(node_rect_min);
        if(i==0){
            float h=ImGui::GetFrameHeight();arrowPoint=node_rect_min+ImVec2(h*.5f,h*.5f);
            closePoint=node_rect_min+ImVec2(nodeWidthText+nodeWidthWidget-h*.5f,h*.5f);
            bodyPoint=node_rect_min+ImVec2(80*zoomLevel,90*zoomLevel);
        }
        const bool old_any_active=ImGui::IsAnyItemActive();
        ImGui::BeginGroup();header(*node);ImGui::Dummy(ImVec2(300*zoomLevel,110*zoomLevel));ImGui::EndGroup();
        const bool node_widgets_active=!old_any_active&&ImGui::IsAnyItemActive();
        const ImVec2 node_rect_max=node_rect_min+ImVec2(316*zoomLevel,160*zoomLevel);
        const ImVec2 node_rect_header(node_rect_max.x,node_rect_min.y+29*zoomLevel);
        PRODUCTION_HIT_TEST
        ImGui::SetCursorScreenPos(node_rect_min);ImGui::InvisibleButton("node",node_rect_max-node_rect_min);
        PRODUCTION_SELECTION
        ImGui::PopID();
    }
    PRODUCTION_POPUP_OPEN
    PRODUCTION_POPUP
    popup=ImGui::IsPopupOpen("Node Context");
    ImGui::PopStyleVar();ImGui::End();ImGui::Render();
}
void idle(int n=3){while(n--)frame();}
void click(ImVec2 point,int button=0){
    auto& io=ImGui::GetIO();io.AddMousePosEvent(point.x,point.y);idle();
    io.AddMouseButtonEvent(button,true);frame();io.AddMouseButtonEvent(button,false);frame();idle();
}
void dismiss(){click(ImVec2(1700,950));assert(!popup);}
void rightClickWithoutSelectionChange(ImVec2 point,Node* expected){
    auto before=container->getSelectedModules();int count=notifications,anchors=inspectorAnchors;
    click(point,1);assert(popup&&customGuiContextNode==expected);
    assert(before==container->getSelectedModules()&&count==notifications&&anchors==inspectorAnchors);dismiss();
}
int main(){
    nodes[0].index=0;nodes[1].index=1;
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={1800,1000};io.DeltaTime=1.f/60;
    unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    for(float zoom:{.25f,1.f,2.f}){
        zoomLevel=zoom;nodeWidthText=nodeWidthWidget=150*zoom;lastSelectedNode.clear();
        deselectAllNodes();nodes[0].gui.expanded=true;idle();
        rightClickWithoutSelectionChange(titlePoints[0],&nodes[0]);assert(!nodes[0].gui.selected);
        nodes[1].gui.selected=true;
        rightClickWithoutSelectionChange(titlePoints[0],&nodes[0]);assert(nodes[1].gui.selected);
        click(titlePoints[0]);assert(!popup&&nodes[0].gui.selected&&!nodes[1].gui.selected);
        io.AddKeyEvent(ImGuiMod_Shift,true);idle();click(titlePoints[1]);io.AddKeyEvent(ImGuiMod_Shift,false);idle();
        assert(nodes[0].gui.selected&&nodes[1].gui.selected);
        rightClickWithoutSelectionChange(titlePoints[0],&nodes[0]);
        rightClickWithoutSelectionChange(titlePoints[1],&nodes[1]);
        click(arrowPoint);assert(!nodes[0].gui.expanded&&nodes[0].gui.selected&&nodes[1].gui.selected);
        click(arrowPoint);assert(nodes[0].gui.expanded&&nodes[0].gui.selected&&nodes[1].gui.selected);
        click(closePoint);click(cancelPoint);assert(nodes[0].gui.selected&&nodes[1].gui.selected);
        deselectAllNodes();click(arrowPoint);assert(!nodes[0].gui.expanded&&!popup&&!nodes[0].gui.selected);
        click(closePoint);assert(!popup&&!nodes[0].gui.selected);click(cancelPoint);
        click(bodyPoint);assert(nodes[0].gui.selected);
        overlap=true;deselectAllNodes();idle();
        rightClickWithoutSelectionChange(titlePoints[0],&nodes[0]);
        click(titlePoints[0]);assert(nodes[0].gui.selected&&!nodes[1].gui.selected);overlap=false;idle();
    }
    ImGui::DestroyContext();puts("Node header passed: left/Shift selection, right-click preserves selection and Inspector, empty context, overlapping nodes and controls at three zoom levels.");
}
'''
def main():
    source=(ROOT/'src/Nodes/ofxOceanodeNodeGui.cpp').read_text()
    canvas=(ROOT/'src/Managers/ofxOceanodeCanvas.cpp').read_text()
    header=block(source,'if (!isTransparent)')
    header=header.replace('const float defaultContentWidth = nodeWidthText + nodeWidthWidget;',
                          'const float defaultContentWidth = nodeWidthText + nodeWidthWidget; titlePoints[node.index]=ImGui::GetCursorScreenPos()+ImVec2(4,ImGui::GetFrameHeight()*.5f);')
    header=header.replace('if (ImGui::Button("Cancel", ImVec2(120,0))) { ImGui::CloseCurrentPopup(); }',
                          'if (ImGui::Button("Cancel", ImVec2(120,0))) { ImGui::CloseCurrentPopup(); } cancelPoint=(ImGui::GetItemRectMin()+ImGui::GetItemRectMax())*.5f;')
    hit_start=canvas.index('                bool nodeHeaderHovered = false;')
    hit_end=canvas.index('                // Display node box',hit_start)
    select_start=canvas.index('                bool toCheckPress =')
    select_end=canvas.index('                //MultiSelect',select_start)
    replacements={'PRODUCTION_HEADER':header,'PRODUCTION_HIT_TEST':canvas[hit_start:hit_end],
                  'PRODUCTION_SELECTION':canvas[select_start:select_end],
                  'PRODUCTION_POPUP_OPEN':block(canvas,'if(open_context_menu && customGuiContextNode != nullptr)'),
                  'PRODUCTION_POPUP':block(canvas,'if(ImGui::BeginPopup("Node Context"))')}
    code=FIXTURE
    for key,value in replacements.items():code=code.replace(key,value)
    with tempfile.TemporaryDirectory(prefix='oceanode-header-') as directory:
        (Path(directory)/'imconfig.h').write_text('')
        cpp=Path(directory)/'test.cpp';cpp.write_text(code);binary=Path(directory)/'test'
        subprocess.run([os.environ.get('CXX','clang++'),'-std=c++17','-O0','-g','-fsanitize=address,undefined',
                        '-I',str(IMGUI),'-I',directory,str(cpp),*[str(IMGUI/name) for name in ('imgui.cpp','imgui_draw.cpp','imgui_widgets.cpp','imgui_tables.cpp')],'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
if __name__=='__main__':main()
