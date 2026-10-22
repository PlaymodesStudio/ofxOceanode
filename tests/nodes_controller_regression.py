#!/usr/bin/env python3
"""Run the production Nodes controller against real sibling ImGui.

Synthetic OF graph/canvas dependencies isolate tree layout and interaction.
Checks clipping, complete offscreen navigation, filtering, live names, topology
changes and deferred-scroll lifetime safety. Default build uses ASan/UBSan;
--benchmark builds with -O2 for indicative CPU timings. No GPU backend is run.
"""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
IMGUI = Path(os.environ.get("OCEANODE_TEST_IMGUI_SRC", str(ROOT.parent / "ofxImGui/libs/imgui/src")))

FIXTURE_PREFIX = r'''
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <numeric>
#include <cctype>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <chrono>
#include <cassert>
#include <cstdint>
#include <cstdio>
using namespace std;
struct Metrics { long long moduleCalls=0, copiedNodes=0; };
static Metrics metrics;
namespace glm {
struct vec2 { float x,y; vec2(float a=0,float b=0):x(a),y(b){} };
vec2 operator-(vec2 a){return {-a.x,-a.y};}
vec2 operator-(vec2 a,vec2 b){return {a.x-b.x,a.y-b.y};}
vec2 operator+(vec2 a,vec2 b){return {a.x+b.x,a.y+b.y};}
vec2 operator/(vec2 a,float b){return {a.x/b,a.y/b};}
}
struct ofColor { int r=128,g=180,b=220; };
struct Rectangle {float getWidth(){return 100;} float getHeight(){return 100;}};
struct ofEventListener {};
struct NodeEvent {
    function<void(class ofxOceanodeNode*)> callback;
    template<class F> ofEventListener newListener(F f){callback=f; return {};}
};
template<class T> struct Param { T value{}; T get(){return value;} };
string ofToString(int n){return to_string(n);}
string ofToDataPath(const string& p){return p;}
int ofStringTimesInString(string hay,string needle){
    int count=0; size_t pos=0;
    while(!needle.empty() && (pos=hay.find(needle,pos))!=string::npos){++count;pos+=needle.size();}
    return count;
}
struct ofxOceanodeNodeGui {
    bool selected=false;
    Rectangle getRectangle(){return {};}
    glm::vec2 getPosition(){return {};}
    void setSelected(bool value){selected=value;}
};
struct ofxOceanodeCanvas {
    float getZoomLevel(){return 1;}
    glm::vec2 getContentRegionSize(){return {400,600};}
    glm::vec2 scrolling; int focusRequests=0;
    void setScrolling(glm::vec2 value){scrolling=value;} void requestFocus(){++focusRequests;} void bringOnTop(){}
    int getUniqueID(){return 1;}
};
struct ofxOceanodeNodeModel {
    virtual ~ofxOceanodeNodeModel()=default;
    string name="Oscillator"; int id=0;
    string nodeName(){return name;} int getNumIdentifier(){return id;}
    template<class T> Param<T> getInspectorParameter(const string&){return {};}
};
struct Parameters {string name; string getName(){return name;}};
struct ofxOceanodeNode {
    unique_ptr<ofxOceanodeNodeModel> model;
    Parameters params; ofxOceanodeNodeGui gui;
    ofxOceanodeNodeModel& getNodeModel(){return *model;}
    Parameters& getParameters(){return params;}
    ofxOceanodeNodeGui& getNodeGui(){return gui;}
    ofColor getColor(){return {};}
};
struct ofxOceanodeContainer {
    map<int,unique_ptr<ofxOceanodeNode>> nodes;
    uint64_t revision=0;
    uint64_t getNodesRevision() const{return revision;}
    vector<ofxOceanodeNode*> getAllModules(){
        ++metrics.moduleCalls;vector<ofxOceanodeNode*> v;
        for(auto& p:nodes) v.push_back(p.second.get());
        metrics.copiedNodes+=v.size();return v;
    }
    auto& getParameterGroupNodesMap(){return nodes;}
};
struct ofxOceanodeNodeMacro:ofxOceanodeNodeModel {
    shared_ptr<ofxOceanodeContainer> container=make_shared<ofxOceanodeContainer>();
    ofxOceanodeCanvas canvas;
    bool local=false,active=true; string macroName="Test",localName="Local";
    bool isLocal(){return local;} bool isActive(){return active;} string getLocalMacroName(){return localName;}
    string getCurrentMacroName(){return macroName;}
    auto getContainer(){return container;} auto getCanvas(){return &canvas;}
    int activations=0; void activateWindow(){++activations;}
    struct WindowTarget {ofxOceanodeNodeMacro* macro; ofxOceanodeNode* node;};
    ofxOceanodeNodeMacro& windowInstance(){return *this;}
    WindowTarget windowTargetFor(ofxOceanodeNode* node){return {this,node};}
};
struct abstractPortal:ofxOceanodeNodeModel {
    string portalName; string getName(){return portalName;}
    template<class T> Param<T> getParameter(const string&){return {};}
};
struct abstractRouter:ofxOceanodeNodeModel {Param<string> nameParam; Param<string>& getNameParam(){return nameParam;}};
struct ofxOceanodeBaseController {ofxOceanodeBaseController(string){} virtual ~ofxOceanodeBaseController()=default;};
struct ofxOceanodeShared {
    static NodeEvent& getNodeSelectedInCanvasEvent(){static NodeEvent e;return e;}
    static void nodeSelectedInCanvas(ofxOceanodeNode* n){auto& e=getNodeSelectedInCanvasEvent();if(e.callback)e.callback(n);}
    static void setActiveCanvasUniqueID(int){}
};
struct OceanodeColors {
    inline static ImVec4 SelectedNodeText={1,.55,0,1}, ZebraEven={1,1,1,.031}, ZebraOdd={0,0,0,.031}, SearchMatchHighlight={1,.863,0,.176};
    static ImU32 U32(ImVec4 c){return ImGui::ColorConvertFloat4ToU32(c);}
};

static int fixtureRows=0,fixtureSorts=0,fixtureRowRebuilds=0;
static map<ofxOceanodeNode*,ImVec2> fixturePositions;
'''

FIXTURE_TESTS = r'''
static ofxOceanodeNode* add(shared_ptr<ofxOceanodeContainer> c,int id,int type=3){
    auto n=make_unique<ofxOceanodeNode>();
    if(type==0)n->model=make_unique<ofxOceanodeNodeMacro>();
    else if(type==1)n->model=make_unique<abstractPortal>();
    else if(type==2)n->model=make_unique<abstractRouter>();
    else n->model=make_unique<ofxOceanodeNodeModel>();
    auto& m=*n->model;m.id=id;
    m.name=type==0?"Macro":type==1?"Portal":type==2?"Router":"Oscillator";
    n->params.name=m.name+" "+to_string(id);
    auto* result=n.get();c->nodes[id]=std::move(n);++c->revision;return result;
}

static ImGuiWindow* childWindow(){
    for(auto* w:GImGui->Windows) if(strstr(w->Name,"nodesListChild"))return w;
    assert(false);return nullptr;
}
static double frame(ofxOceanodeNodesController& ctrl){
    ImGui::NewFrame();ImGui::SetNextWindowPos({10,10});ImGui::SetNextWindowSize({400,600});
    ImGui::Begin("Nodes");ImGui::SetWindowFocus();
    fixtureRows=0;fixturePositions.clear();
    auto start=chrono::steady_clock::now();ctrl.draw();auto end=chrono::steady_clock::now();
    ImGui::End();ImGui::Render();
    return chrono::duration<double,micro>(end-start).count();
}
static void idle(ofxOceanodeNodesController& c,int n=4){while(n--)frame(c);}
static void click(ofxOceanodeNodesController& c,ImVec2 pos){
    auto& io=ImGui::GetIO();io.AddMousePosEvent(pos.x,pos.y);frame(c);
    io.AddMouseButtonEvent(0,true);frame(c);io.AddMouseButtonEvent(0,false);frame(c);
    io.AddMousePosEvent(-100,-100);idle(c);
}
static void key(ofxOceanodeNodesController& c,ImGuiKey key){
    auto& io=ImGui::GetIO();io.AddKeyEvent(key,true);frame(c);io.AddKeyEvent(key,false);idle(c);
}
static ofxOceanodeNodeMacro* macro(ofxOceanodeNode* n){return dynamic_cast<ofxOceanodeNodeMacro*>(&n->getNodeModel());}
static void erase(shared_ptr<ofxOceanodeContainer> host,int id){host->nodes.erase(id);++host->revision;}
static void reset(){
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;
    io.DisplaySize={1200,900};io.DeltaTime=1.f/60;
    io.BackendFlags|=ImGuiBackendFlags_RendererHasVtxOffset;
    unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
}
static void flatTests(){
    reset();auto graph=make_shared<ofxOceanodeContainer>();ofxOceanodeCanvas canvas;
    for(int i=0;i<1000;++i)add(graph,i);
    ofxOceanodeNodesController ctrl(graph,&canvas);idle(ctrl);
    assert(ctrl.navigableNodes.size()==1000 && ctrl.visibleRows.size()==1000);
    assert(fixtureRows>=25 && fixtureRows<=40);
    assert(ImGui::GetDrawData()->TotalVtxCount<3000);
    // Leaf clicks retain selection and deferred canvas centering.
    auto firstPosition=fixturePositions.at(ctrl.navigableNodes.front().node);
    firstPosition.x+=40;firstPosition.y+=6;click(ctrl,firstPosition);
    assert(ctrl.selectedNode==ctrl.navigableNodes.front().node);
    assert(canvas.scrolling.x==150 && canvas.scrolling.y==250);
    int sorts=fixtureSorts,rebuilds=fixtureRowRebuilds;metrics={};idle(ctrl,20);
    assert(metrics.moduleCalls==0 && fixtureSorts==sorts && fixtureRowRebuilds==rebuilds);
    // Clipping must not limit keyboard navigation, including wraparound.
    ctrl.selectedNode=ctrl.navigableNodes[899].node;key(ctrl,ImGuiKey_DownArrow);
    assert(ctrl.selectedNode==ctrl.navigableNodes[900].node);
    assert(childWindow()->Scroll.y>10000);
    ctrl.selectedNode=ctrl.navigableNodes.back().node;key(ctrl,ImGuiKey_DownArrow);
    assert(ctrl.selectedNode==ctrl.navigableNodes.front().node);
    key(ctrl,ImGuiKey_UpArrow);assert(ctrl.selectedNode==ctrl.navigableNodes.back().node);
    // Offscreen selected row is explicitly included, then scrolled into view.
    ctrl.selectedNode=ctrl.navigableNodes[500].node;ctrl.scrollTreeToSelected=true;idle(ctrl);
    assert(!ctrl.scrollTreeToSelected && fixturePositions.count(ctrl.selectedNode));
    const auto pos=fixturePositions.at(ctrl.selectedNode);
    assert(pos.y>=childWindow()->ClipRect.Min.y && pos.y<=childWindow()->ClipRect.Max.y);
    // Silent/live group renames re-sort immediately; large numeric names don't overflow.
    graph->nodes[999]->params.name="A 99999999999999999999999999999999";frame(ctrl);
    assert(ctrl.navigableNodes.front().node==graph->nodes[999].get());
    graph->nodes[998]->params.name="A 2";frame(ctrl);
    assert(ctrl.navigableNodes.front().node==graph->nodes[998].get());
    ctrl.searchFieldMyNodes="oScIlLaToR 99";frame(ctrl);
    assert(ctrl.navigableNodes.size()==9);
    for(const auto& n:ctrl.navigableNodes)assert(n.matchesSearch);
    ctrl.searchFieldMyNodes="nothing-matches";frame(ctrl);assert(ctrl.visibleRows.empty());
    ctrl.searchFieldMyNodes.clear();frame(ctrl);
    // A queued scroll may outlive the selected node.
    ctrl.navigateTo(ctrl.navigableNodes.front());auto* deleted=ctrl.selectedNode;
    erase(graph,998);idle(ctrl);assert(ctrl.selectedNode==nullptr && ctrl.pendingScrollNode==nullptr);
    for(const auto& n:ctrl.navigableNodes)assert(n.node!=deleted);
    auto* added=add(graph,1001);frame(ctrl);assert(ctrl.navigableNodes.size()==1000);
    assert(any_of(ctrl.navigableNodes.begin(),ctrl.navigableNodes.end(),[&](auto& n){return n.node==added;}));
    // Variable-height labels use a safe layout fallback.
    added->params.name="Multiple\nlines";frame(ctrl);
    assert(!ctrl.uniformRowHeight && fixtureRows==1000);
    graph->nodes.clear();++graph->revision;idle(ctrl);assert(ctrl.visibleRows.empty());
    add(graph,0);idle(ctrl);assert(ctrl.navigableNodes.size()==1);
    ImGui::DestroyContext();
}
static void nestedTests(){
    reset();auto root=make_shared<ofxOceanodeContainer>();ofxOceanodeCanvas canvas;
    auto* outerNode=add(root,0,0);auto* outer=macro(outerNode);
    auto* innerNode=add(outer->container,0,0);auto* inner=macro(innerNode);
    for(int i=0;i<1000;++i)add(inner->container,i);
    auto* portal=add(outer->container,1,1);dynamic_cast<abstractPortal*>(&portal->getNodeModel())->portalName="Zed";
    auto* router=add(outer->container,2,2);dynamic_cast<abstractRouter*>(&router->getNodeModel())->nameParam.value="Output";
    ofxOceanodeNodesController ctrl(root,&canvas);idle(ctrl);
    assert(ctrl.visibleRows.size()==1 && ctrl.nodeCaches.size()==1);
    // A real arrow click toggles expansion without selecting/navigating.
    auto p=fixturePositions.at(outerNode);p.x+=9+ImGui::GetStyle().FramePadding.x+ImGui::GetFontSize()*0.5f;p.y+=6;
    click(ctrl,p);assert(ctrl.visibleRows.size()==4 && ctrl.selectedNode==nullptr);
    // Label double-click enters the macro's own canvas and cancels parent scroll.
    p=fixturePositions.at(outerNode);p.x+=70;p.y+=6;
    click(ctrl,p);click(ctrl,p);
    assert(outer->activations>0 && ctrl.selectedNode==nullptr && ctrl.pendingScrollNode==nullptr);
    ctrl.forceExpandAll=true;idle(ctrl);assert(ctrl.visibleRows.size()==1004 && fixtureRows<=40);
    // Offscreen macro child remains navigable and focuses its own containing canvas.
    auto target=ctrl.navigableNodes[700];assert(target.macro==inner);
    ctrl.navigateTo(target);idle(ctrl);assert(inner->activations>0 && target.node->getNodeGui().selected);
    assert(inner->canvas.scrolling.x==150 && inner->canvas.scrolling.y==250);
    ctrl.forceExpandAll=false;ctrl.forceCollapseAll=true;idle(ctrl);assert(ctrl.visibleRows.size()==1);
    metrics={};idle(ctrl,20);assert(metrics.moduleCalls==0);
    // Canvas selection reveals a node whose parent macros were collapsed.
    ofxOceanodeShared::nodeSelectedInCanvas(inner->container->nodes[999].get());idle(ctrl);
    assert(ctrl.visibleRows.size()==1004 && fixturePositions.count(ctrl.selectedNode));
    ctrl.forceCollapseAll=true;idle(ctrl);assert(ctrl.visibleRows.size()==1);
    // Search expands ancestors and counts each subtree once without repeated vector copies.
    ctrl.searchFieldMyNodes="oscillator 999";idle(ctrl);
    assert(ctrl.visibleRows.size()==3 && ctrl.navigableNodes.back().matchesSearch);
    assert(ctrl.navigableNodes.back().macro==inner && ctrl.visibleRows.back().depth==2);
    int sorts=fixtureSorts,rebuilds=fixtureRowRebuilds;metrics={};idle(ctrl,20);
    assert(metrics.moduleCalls==0 && fixtureSorts==sorts && fixtureRowRebuilds==rebuilds);
    outer->active=false;frame(ctrl);assert(!ctrl.visibleRows.back().parentActive);
    outer->active=true;frame(ctrl);assert(ctrl.visibleRows.back().parentActive);
    // Macro and portal/router names must update without a topology revision.
    outer->local=true;outer->localName="Changed Local";ctrl.searchFieldMyNodes="changed local";frame(ctrl);
    assert(ctrl.visibleRows.size()==1 && ctrl.visibleRows.front().entry->matchesSearch);
    outer->local=false;outer->macroName="Changed Global";ctrl.searchFieldMyNodes="changed global";frame(ctrl);
    assert(ctrl.visibleRows.size()==1);
    ctrl.searchFieldMyNodes.clear();ctrl.nodeTypeFilter=2;frame(ctrl);
    assert(ctrl.navigableNodes.size()==2 && ctrl.navigableNodes.back().node==portal);
    dynamic_cast<abstractPortal*>(&portal->getNodeModel())->portalName="Renamed Portal";
    ctrl.searchFieldMyNodes="renamed portal";frame(ctrl);assert(ctrl.navigableNodes.size()==2);
    // Search uses the displayed brackets consistently for descendants.
    ctrl.searchFieldMyNodes="[renamed portal]";frame(ctrl);assert(ctrl.navigableNodes.size()==2);
    ctrl.searchFieldMyNodes.clear();ctrl.nodeTypeFilter=3;frame(ctrl);
    assert(ctrl.navigableNodes.size()==2 && ctrl.navigableNodes.back().node==router);
    dynamic_cast<abstractRouter*>(&router->getNodeModel())->nameParam.value="Renamed Router";
    ctrl.searchFieldMyNodes="renamed router";frame(ctrl);assert(ctrl.navigableNodes.size()==2);
    ctrl.searchFieldMyNodes.clear();ctrl.nodeTypeFilter=0;ctrl.forceExpandAll=true;idle(ctrl);
    ctrl.navigateTo(ctrl.navigableNodes[700]);
    // Destroy both the selected node and its canvas before the deferred scroll.
    erase(root,0);idle(ctrl);assert(ctrl.navigableNodes.empty() && ctrl.pendingScrollNode==nullptr);
    // Repeated preset-style graph replacement exercises stale cached row lifetimes.
    for(int iteration=0;iteration<60;++iteration){
        auto* m=macro(add(root,0,0));for(int i=0;i<20;++i)add(m->container,i);
        ctrl.forceExpandAll=true;idle(ctrl,2);assert(ctrl.navigableNodes.size()==21);
        erase(root,0);idle(ctrl,2);assert(ctrl.navigableNodes.empty());
    }
    ImGui::DestroyContext();
}
static void benchmark(){
    for(int n:{100,1000,5000}){
        reset();auto root=make_shared<ofxOceanodeContainer>();ofxOceanodeCanvas canvas;
        for(int i=0;i<n;++i)add(root,i);
        ofxOceanodeNodesController ctrl(root,&canvas);idle(ctrl,30);
        vector<double> times;for(int i=0;i<300;++i)times.push_back(frame(ctrl));sort(times.begin(),times.end());
        printf("flat %d: median %.3f us, p95 %.3f us, %d rows submitted, %d vertices\n",n,times[150],times[285],fixtureRows,ImGui::GetDrawData()->TotalVtxCount);
        ImGui::DestroyContext();
    }
}
int main(){flatTests();nestedTests();puts("Nodes controller regression checks passed.");benchmark();}
'''

def main():
    header = (ROOT / "src/Controls/ofxOceanodeNodesController.h").read_text()
    header = re.sub(r"^#include.*$", "", header, flags=re.M).replace("private:", "public:")
    source = (ROOT / "src/Controls/ofxOceanodeNodesController.cpp").read_text()
    source = re.sub(r"^#include.*$", "", source, flags=re.M)
    source = source.replace("if(sortDirty) {", "if(sortDirty) { ++fixtureSorts;")
    source = source.replace("if(rowsDirty) {", "if(rowsDirty) { ++fixtureRowRebuilds;")
    source = source.replace("const ImVec2 swatchPos = ImGui::GetCursorScreenPos();", "const ImVec2 swatchPos = ImGui::GetCursorScreenPos(); ++fixtureRows; fixturePositions[entry.node]=swatchPos;")
    with tempfile.TemporaryDirectory(prefix="oceanode-nodes-regression-") as work_dir:
        work = Path(work_dir)
        (work / "imconfig.h").write_text("// Standalone ImGui without OF adapters.\n")
        cpp = work / "regression.cpp"
        cpp.write_text(FIXTURE_PREFIX + header + source + FIXTURE_TESTS)
        flags = ["-O2"] if "--benchmark" in sys.argv else ["-O1", "-g", "-fsanitize=address,undefined"]
        exe = work / "regression"
        subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++17", *flags, "-I", str(IMGUI), "-I", str(work), str(cpp),
                        *[str(IMGUI / name) for name in ("imgui.cpp", "imgui_draw.cpp", "imgui_widgets.cpp", "imgui_tables.cpp")],
                        "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)

if __name__ == "__main__":
    main()
