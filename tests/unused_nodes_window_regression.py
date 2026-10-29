#!/usr/bin/env python3
"""Exercise the non-modal cleanup review and candidate navigation with real ImGui.

Uses the production review window, showNodeInCanvas and deferred centering with
isolated graph services. Covers group ticking, kept-node reasons, the window staying
open after Remove and ticks surviving a refresh. Runs mouse events under ASan/UBSan;
no OF/GPU host.
"""
import os
from pathlib import Path
import subprocess
import tempfile
from canvas_navigation_regression import ROOT, IMGUI, FIXTURE, block

EXTRA = r'''
#include <set>
string ofToString(size_t value){return to_string(value);}
struct ofxOceanode {
    ofxOceanodeContainer::UnusedNodePlan unusedNodePlan;
    weak_ptr<ofxOceanodeContainer> unusedNodeContainer;
    bool showUnusedNodesWindow=false,focusUnusedNodesWindow=false,unusedNodesPlanChanged=false;
    string unusedNodesSelectedNode;
    int unusedNodesSelectedGroup=-1,unusedNodesOpenGroup=-1;bool unusedNodesScrollToSelection=false;
    void drawUnusedNodesWindow();
    void open(shared_ptr<ofxOceanodeContainer> target){
        unusedNodeContainer=target;unusedNodePlan=target->findUnusedNodes();
        showUnusedNodesWindow=focusUnusedNodesWindow=true;
        unusedNodesSelectedNode.clear();unusedNodesSelectedGroup=-1;unusedNodesPlanChanged=false;
    }
};
map<string,ImVec2> rowPoints,buttonPoints;
bool trackedSelectable(const char* name,bool selected){
    bool clicked=ImGui::Selectable(name,selected);
    auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
    rowPoints[name]={(a.x+b.x)*.5f,(a.y+b.y)*.5f};return clicked;
}
bool trackedCheckbox(const char* name,bool* value){
    bool changed=ImGui::Checkbox(name,value);
    auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
    rowPoints[name]={(a.x+b.x)*.5f,(a.y+b.y)*.5f};return changed;
}
bool trackedTree(const char* name){
    bool open=ImGui::TreeNode(name);
    auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
    rowPoints[name]={a.x+60.f,(a.y+b.y)*.5f};return open;
}
bool trackedTreeEx(const char* name,ImGuiTreeNodeFlags flags){
    bool open=ImGui::TreeNodeEx(name,flags);
    auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
    rowPoints[name]={a.x+60.f,(a.y+b.y)*.5f};rowPoints[string(name)+"#arrow"]={a.x+9.f,(a.y+b.y)*.5f};return open;
}
bool trackedButton(const char* name,ImVec2 size){
    bool clicked=ImGui::Button(name,size);
    auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
    buttonPoints[name]={(a.x+b.x)*.5f,(a.y+b.y)*.5f};return clicked;
}
'''

CASES = r'''
using Group=ofxOceanodeContainer::UnusedNodePlan::Group;
using Kept=ofxOceanodeContainer::UnusedNodePlan::Kept;
static ImVec2 ptAt(map<string,ImVec2>& points,const string& key,int line){
    auto found=points.find(key);
    if(found==points.end()){fprintf(stderr,"Line %d: no widget named '%s' on screen\n",line,key.c_str());abort();}
    return found->second;
}
#define pt(points,key) ptAt(points,key,__LINE__)
static size_t pt_count(const string& key){return rowPoints.count(key);}
static ofxOceanode app;
static int underlyingClicks=0;
static ImVec2 underlying;
static vector<ofxOceanodeCanvas*> views;
static void frame(){
    ImGui::NewFrame();
    for(auto* c:views){
        if(c->pendingCenterFrames>0)--c->pendingCenterFrames;
        c->center();
        if(c->focusPending||c->onTop){ImGui::SetNextWindowFocus();c->focusPending=c->onTop=false;}
        ImGui::SetNextWindowPos({20,20});ImGui::SetNextWindowSize({300,600});
        ImGui::Begin(c->uniqueID.c_str());
        if(ImGui::Button("Canvas action"))++underlyingClicks;
        auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();underlying={(a.x+b.x)*.5f,(a.y+b.y)*.5f};
        ImGui::End();
    }
    if(app.showUnusedNodesWindow)ImGui::SetNextWindowPos({400,20});
    rowPoints.clear();buttonPoints.clear();app.drawUnusedNodesWindow();
    assert(GImGui->OpenPopupStack.empty());
    ImGui::Render();
}
static void idle(int count=3){while(count--)frame();}
static void click(ImVec2 p){
    auto& io=ImGui::GetIO();io.AddMousePosEvent(p.x,p.y);idle(2);
    io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();idle();
}
static ofxOceanodeNode* add(ofxOceanodeContainer& host,int id,bool isMacro=false){
    auto node=make_unique<ofxOceanodeNode>();
    if(isMacro)node->model=make_unique<ofxOceanodeNodeMacro>();else node->model=make_unique<Model>();
    auto* out=node.get();host.nodes[id]=std::move(node);return out;
}
// Groups start closed; this opens group i the way the Left key does.
static void openGroup(int i){app.unusedNodesOpenGroup=i;idle();}
static void centeredAt(ofxOceanodeCanvas& c,float x,float y){
    assert(!c.hasPendingCenterPoint&&!c.pendingCenterNode);
    assert(abs(c.scrolling.x+x-c.contentRegionSize.x/(2*c.zoomLevel))<.01f);
    assert(abs(c.scrolling.y+y-c.contentRegionSize.y/(2*c.zoomLevel))<.01f);
}
static void centered(ofxOceanodeCanvas& c,ofxOceanodeNode* node){
    assert(!c.pendingCenterNode);
    const auto p=node->getNodeGui().getPosition();
    assert(abs(c.scrolling.x+p.x+60-c.contentRegionSize.x/(2*c.zoomLevel))<.01f);
    assert(abs(c.scrolling.y+p.y+40-c.contentRegionSize.y/(2*c.zoomLevel))<.01f);
}
int main(){
    ImGui::CreateContext();auto& io=ImGui::GetIO();
    io.DisplaySize={1400,1000};io.DeltaTime=1.f/60;io.IniFilename=nullptr;
    unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    auto root=make_shared<ofxOceanodeContainer>();ofxOceanodeCanvas rootView;rootView.uniqueID="Canvas";
    auto* candidate=add(*root,1);auto* outer=dynamic_cast<ofxOceanodeNodeMacro*>(&add(*root,2,true)->getNodeModel());
    auto* inner=dynamic_cast<ofxOceanodeNodeMacro*>(&add(*outer->container,3,true)->getNodeModel());
    auto* nested=add(*inner->container,4);auto* protectedNode=add(*inner->container,5);
    inner->canvas.uniqueID="Nested Macro";inner->canvas.zoomLevel=.5f;
    outer->visible=inner->visible=false;
    ofxOceanodeShared::root=root.get();ofxOceanodeShared::canvas=&rootView;views={&rootView};
    root->plan.groups={Group{{"1"},0,true}};app.open(root);idle();
    assert(pt_count("1")==0);openGroup(0);
    auto* review=ImGui::FindWindowByName("Remove unused nodes");assert(review);
    assert(!(review->Flags&(ImGuiWindowFlags_Modal|ImGuiWindowFlags_Popup)));
    click(underlying);assert(underlyingClicks==1&&app.showUnusedNodesWindow);
    click(pt(rowPoints,"1"));
    assert(ofxOceanodeShared::selected==candidate&&candidate->gui.selected);
    assert(app.unusedNodesSelectedNode=="1"&&app.showUnusedNodesWindow);centered(rootView,candidate);

    // The reviewed Macro may have been hidden since opening the review.
    inner->container->plan.groups={Group{{"4"},0,true}};
    inner->container->plan.kept={Kept{"5",{"sink: declared"}}};
    app.open(inner->container);idle();openGroup(0);click(pt(rowPoints,"4"));
    assert(inner->visible&&inner->activations==1&&ofxOceanodeShared::selected==nested);
    assert(ofxOceanodeShared::active==inner->canvas.uniqueID&&app.showUnusedNodesWindow);
    views={&inner->canvas};idle();centered(inner->canvas,nested);
    // Kept nodes start collapsed; opening one shows its row for navigation.
    assert(rowPoints.count("5")==0&&rowPoints.count("Kept: 5")==1);
    click(pt(rowPoints,"Kept: 5"));assert(rowPoints.count("5")==1);
    click(pt(rowPoints,"5"));
    assert(protectedNode->gui.selected&&!nested->gui.selected&&app.unusedNodesSelectedNode=="5");
    centered(inner->canvas,protectedNode);

    // Graph changes must not navigate to replacement nodes with the same name.
    inner->container->nodes.erase(4);add(*inner->container,4);++inner->container->revision;
    auto* selected=ofxOceanodeShared::selected;idle();click(pt(rowPoints,"4"));
    assert(ofxOceanodeShared::selected==selected);
    click(pt(buttonPoints,"Remove"));assert(inner->container->removals==0&&app.showUnusedNodesWindow);
    click(pt(buttonPoints,"Refresh"));assert(app.unusedNodePlan.revision==inner->container->revision);
    click(pt(rowPoints,"4"));assert(ofxOceanodeShared::selected==inner->container->nodes[4].get());

    // Usage-only changes are revalidated on Remove; the revised proposal stays open.
    inner->container->removalAllowed=false;click(pt(buttonPoints,"Remove"));
    assert(app.showUnusedNodesWindow&&app.unusedNodesPlanChanged&&inner->container->removals==1);
    inner->container->removalAllowed=true;click(pt(buttonPoints,"Remove"));
    assert(app.showUnusedNodesWindow&&!app.unusedNodesPlanChanged&&inner->container->removals==2);

    // Ticks: only ticked groups reach removal, Remove needs one, and ticks survive the refresh after it.
    inner->container->plan.groups={Group{{"4"},2,true},Group{{"6"},1,true}};
    inner->container->plan.kept.clear();inner->container->lastSelected.clear();
    app.open(inner->container);idle();
    assert(rowPoints.count("##group0")==1&&rowPoints.count("##group1")==1);
    click(pt(rowPoints,"##group1"));assert(app.unusedNodePlan.groups[0].selected&&!app.unusedNodePlan.groups[1].selected);
    click(pt(buttonPoints,"Remove"));
    assert(inner->container->removals==3&&inner->container->lastSelected==vector<bool>({true,false}));
    assert(app.showUnusedNodesWindow&&!app.unusedNodePlan.groups[1].selected&&app.unusedNodePlan.groups[0].selected);
    click(pt(rowPoints,"##group0"));assert(!app.unusedNodePlan.groups[0].selected);
    click(pt(buttonPoints,"Remove"));assert(inner->container->removals==3);
    click(pt(rowPoints,"##group0"));assert(app.unusedNodePlan.groups[0].selected);
    click(pt(buttonPoints,"Refresh"));assert(app.unusedNodePlan.groups[0].selected&&!app.unusedNodePlan.groups[1].selected);

    // Clicking a group header selects all its nodes and centers on the one nearest their middle.
    {
        auto* first=inner->container->nodes[4].get();auto* second=inner->container->nodes[5].get();
        auto* third=add(*inner->container,6);
        first->gui.position={0,0};second->gui.position={100,0};third->gui.position={500,0};
        inner->container->plan.groups={Group{{"4","5","6"},0,true}};inner->container->plan.kept.clear();
        app.open(inner->container);views={&inner->canvas};idle();
        const string header="Group 1: 4 (3 nodes, 0 connections)";
        assert(rowPoints.count(header)==1&&app.unusedNodesSelectedGroup==-1);
        assert(rowPoints.count("4")==0);
        click(pt(rowPoints,header));
        assert(first->gui.selected&&second->gui.selected&&third->gui.selected);
        assert(app.unusedNodesSelectedGroup==0&&app.unusedNodesSelectedNode.empty());
        assert(ofxOceanodeShared::selected==first&&app.showUnusedNodesWindow);
        // Clicking the label selects without opening; the view centers on the middle of all three nodes.
        assert(rowPoints.count("4")==0);
        idle();centeredAt(inner->canvas,310,40);
        // Picking one node afterwards replaces the group selection.
        openGroup(0);click(pt(rowPoints,"4"));
        assert(first->gui.selected&&!second->gui.selected&&!third->gui.selected);
        assert(app.unusedNodesSelectedGroup==-1&&app.unusedNodesSelectedNode=="4");
        // A changed canvas must not navigate: selection stays as it was.
        first->gui.selected=false;++inner->container->revision;idle();
        click(pt(rowPoints,header));
        assert(!first->gui.selected&&!second->gui.selected&&!third->gui.selected&&app.unusedNodesSelectedGroup==-1);
        click(pt(buttonPoints,"Refresh"));click(pt(rowPoints,header));
        assert(first->gui.selected&&second->gui.selected&&third->gui.selected&&app.unusedNodesSelectedGroup==0);
        // Unticking a group does not select its nodes.
        first->gui.selected=second->gui.selected=third->gui.selected=false;
        click(pt(rowPoints,"##group0"));
        assert(!first->gui.selected&&!second->gui.selected&&!third->gui.selected&&!app.unusedNodePlan.groups[0].selected);
    }

    // Selecting a group zooms out, never in, until all its nodes fit the view; single nodes keep their zoom.
    {
        auto* a=inner->container->nodes[4].get();auto* b=inner->container->nodes[5].get();auto* c=inner->container->nodes[6].get();
        auto select=[&](const string& title){
            inner->container->plan.groups={Group{{"4","5","6"},0,true}};inner->container->plan.kept.clear();
            app.open(inner->container);views={&inner->canvas};idle();click(pt(rowPoints,title));idle();
        };
        const string title="Group 1: 4 (3 nodes, 0 connections)";
        // 1,400 wide at zoom 1 in an 800 wide view: needs 0.514, so the next lower step, 0.5.
        a->gui.position={0,0};b->gui.position={640,0};c->gui.position={1280,0};
        inner->canvas.zoomLevel=1.0f;select(title);
        assert(abs(inner->canvas.zoomLevel-0.5f)<.001f);centeredAt(inner->canvas,700,40);
        // Already fitting: the zoom stays, even when it is lower than needed.
        a->gui.position={0,0};b->gui.position={100,0};c->gui.position={200,0};
        inner->canvas.zoomLevel=0.5f;select(title);
        assert(abs(inner->canvas.zoomLevel-0.5f)<.001f);centeredAt(inner->canvas,160,40);
        // Too spread out for the zoom limit: stops at the smallest zoom.
        a->gui.position={0,0};b->gui.position={3000,0};c->gui.position={6000,0};
        inner->canvas.zoomLevel=1.0f;select(title);
        assert(abs(inner->canvas.zoomLevel-0.25f)<.001f);centeredAt(inner->canvas,3060,40);
        // Tall groups fit on the vertical axis too: 1,000 high at zoom 1 in a 600 high view.
        a->gui.position={0,0};b->gui.position={0,500};c->gui.position={0,920};
        inner->canvas.zoomLevel=1.0f;select(title);
        assert(abs(inner->canvas.zoomLevel-0.5f)<.001f);centeredAt(inner->canvas,60,500);
        // A single node never changes the zoom.
        inner->canvas.zoomLevel=1.0f;inner->container->plan.groups={Group{{"6"},0,true}};
        app.open(inner->container);idle();openGroup(0);click(pt(rowPoints,"6"));idle();
        assert(abs(inner->canvas.zoomLevel-1.0f)<.001f);
        c->gui.position=a->gui.position=b->gui.position={350,-210};
        inner->canvas.zoomLevel=0.5f;
    }

    // Keyboard: Up/Down walk the list, Left on a group opens it on its first node, Right goes back to the group.
    {
        auto press=[&](ImGuiKey key){
            auto& keys=ImGui::GetIO();keys.AddKeyEvent(key,true);frame();keys.AddKeyEvent(key,false);frame();idle();
        };
        auto where=[&]()->string{
            return app.unusedNodesSelectedGroup>=0?"group"+to_string(app.unusedNodesSelectedGroup):app.unusedNodesSelectedNode;
        };
        auto* n4=inner->container->nodes[4].get();auto* n5=inner->container->nodes[5].get();
        auto* n6=inner->container->nodes[6].get();add(*inner->container,7);
        inner->container->plan.groups={Group{{"4","5"},0,true},Group{{"6"},0,true}};
        inner->container->plan.kept={Kept{"7",{"sink: declared"}}};
        app.open(inner->container);views={&inner->canvas};idle();
        const int focusBefore=inner->canvas.focusRequests,topBefore=inner->canvas.topRequests;
        auto* review=ImGui::FindWindowByName("Remove unused nodes");
        const string arrow0="Group 1: 4 (2 nodes, 0 connections)#arrow";
        assert(where()==""&&GImGui->NavWindow&&GImGui->NavWindow->RootWindow==review);
        // Both groups start closed, so Up/Down only meet headers and the kept node.
        assert(rowPoints.count("4")==0&&rowPoints.count("5")==0&&rowPoints.count("6")==0);
        press(ImGuiKey_DownArrow);assert(where()=="group0"&&n4->gui.selected&&n5->gui.selected&&!n6->gui.selected);
        press(ImGuiKey_RightArrow);assert(where()=="group0");
        // Space ticks and unticks the selected group, like its checkbox.
        assert(app.unusedNodePlan.groups[0].selected&&app.unusedNodePlan.groups[1].selected);
        press(ImGuiKey_Space);assert(!app.unusedNodePlan.groups[0].selected&&app.unusedNodePlan.groups[1].selected&&where()=="group0");
        press(ImGuiKey_Space);assert(app.unusedNodePlan.groups[0].selected&&where()=="group0");
        press(ImGuiKey_DownArrow);assert(where()=="group1"&&n6->gui.selected&&!n5->gui.selected);
        press(ImGuiKey_DownArrow);assert(where()=="7");
        press(ImGuiKey_DownArrow);assert(where()=="7");
        press(ImGuiKey_UpArrow);assert(where()=="group1");
        press(ImGuiKey_UpArrow);assert(where()=="group0");
        // Left opens the group and goes to its first node.
        press(ImGuiKey_LeftArrow);assert(where()=="4"&&n4->gui.selected&&!n5->gui.selected);
        // On a node, Space leaves every group's tick alone.
        press(ImGuiKey_Space);assert(app.unusedNodePlan.groups[0].selected&&app.unusedNodePlan.groups[1].selected&&where()=="4");
        assert(rowPoints.count("4")==1&&rowPoints.count("5")==1&&rowPoints.count("6")==0);
        press(ImGuiKey_LeftArrow);assert(where()=="4");
        press(ImGuiKey_DownArrow);assert(where()=="5"&&n5->gui.selected&&!n4->gui.selected);
        press(ImGuiKey_DownArrow);assert(where()=="group1");
        press(ImGuiKey_LeftArrow);assert(where()=="6"&&n6->gui.selected&&rowPoints.count("6")==1);
        press(ImGuiKey_RightArrow);assert(where()=="group1"&&n6->gui.selected);
        press(ImGuiKey_LeftArrow);press(ImGuiKey_UpArrow);assert(where()=="group1");
        press(ImGuiKey_UpArrow);assert(where()=="5");
        press(ImGuiKey_RightArrow);assert(where()=="group0"&&n4->gui.selected&&n5->gui.selected);
        // Navigating never moved focus to a canvas, so a later arrow cannot nudge its nodes.
        assert(inner->canvas.focusRequests==focusBefore&&inner->canvas.topRequests==topBefore);
        assert(GImGui->NavWindow&&GImGui->NavWindow->RootWindow==review);
        // A folded group is skipped by Up/Down, and Left on it opens it on its first node.
        click(pt(rowPoints,arrow0));
        assert(rowPoints.count("4")==0&&rowPoints.count("5")==0&&where()=="group0");
        press(ImGuiKey_DownArrow);assert(where()=="group1");
        press(ImGuiKey_UpArrow);assert(where()=="group0");
        press(ImGuiKey_LeftArrow);assert(where()=="4"&&rowPoints.count("4")==1&&rowPoints.count("5")==1);
        // A selected node inside a folded group: Up/Down stand on its group header, Right goes to the group.
        click(pt(rowPoints,arrow0));
        assert(rowPoints.count("4")==0&&where()=="4");
        press(ImGuiKey_DownArrow);assert(where()=="group1");
        press(ImGuiKey_UpArrow);assert(where()=="group0");
        press(ImGuiKey_LeftArrow);assert(where()=="4"&&rowPoints.count("4")==1);
        click(pt(rowPoints,arrow0));
        assert(rowPoints.count("4")==0&&where()=="4");
        press(ImGuiKey_RightArrow);assert(where()=="group0");
        // Keys do nothing once the canvas changed, or when another window has focus.
        const string before=where();++inner->container->revision;idle();
        press(ImGuiKey_DownArrow);assert(where()==before);
        click(pt(buttonPoints,"Refresh"));
        assert(where()=="");
        press(ImGuiKey_DownArrow);assert(where()=="group0");
        const string settled=where();
        click(underlying);press(ImGuiKey_DownArrow);assert(where()==settled);
    }

    // Expired canvases keep a readable, closable window with disabled actions.
    auto temporary=make_shared<ofxOceanodeContainer>();temporary->plan.groups={Group{{"7"},0,true}};add(*temporary,7);
    app.open(temporary);idle();openGroup(0);temporary.reset();idle();click(pt(rowPoints,"7"));
    assert(app.showUnusedNodesWindow);click(pt(buttonPoints,"Remove"));assert(app.showUnusedNodesWindow);
    click(pt(buttonPoints,"Close"));assert(!app.showUnusedNodesWindow);
    ImGui::DestroyContext();
    puts("Cleanup review passed: non-modal interaction, root/nested/kept navigation, zoom, stale/replaced nodes, refresh, usage revalidation, group ticks, group selection and zoom-to-fit on the canvas, arrow-key navigation, Space to tick, persistent window and expired canvases.");
}
'''

def main():
    header=(ROOT/'src/Managers/ofxOceanodeContainer.h').read_text()
    fixture=FIXTURE.replace('    map<int,unique_ptr<ofxOceanodeNode>> nodes;',block(header,'    struct UnusedNodePlan')+r''';
    UnusedNodePlan plan;uint64_t revision=0;int removals=0;bool removalAllowed=true;vector<bool> lastSelected;
    uint64_t getNodesRevision(){return revision;}
    string getCanvasID(){return "Reviewed canvas";}
    UnusedNodePlan findUnusedNodes(){plan.revision=revision;return plan;}
    bool removeUnusedNodes(const UnusedNodePlan& p){++removals;lastSelected.clear();for(const auto& g:p.groups)lastSelected.push_back(g.selected);return removalAllowed;}
    map<int,unique_ptr<ofxOceanodeNode>> nodes;''')
    fixture=fixture.replace('map<int,ofxOceanodeNode*>','map<string,ofxOceanodeNode*>').replace('out[p.first]=p.second.get()','out[to_string(p.first)]=p.second.get()')
    app=(ROOT/'src/ofxOceanode.cpp').read_text()
    window=block(app,'void ofxOceanode::drawUnusedNodesWindow()')
    window=window.replace('ImGui::Selectable(', 'trackedSelectable(').replace('ImGui::Button(', 'trackedButton(').replace('ImGui::Checkbox(', 'trackedCheckbox(').replace('ImGui::TreeNode(', 'trackedTree(').replace('ImGui::TreeNodeEx(', 'trackedTreeEx(')
    container=(ROOT/'src/Managers/ofxOceanodeContainer.cpp').read_text()
    canvas=(ROOT/'src/Managers/ofxOceanodeCanvas.cpp').read_text()
    code=fixture+EXTRA+window+block(container,'bool ofxOceanodeContainer::showNodeInCanvas(')+'\n'+block(container,'bool ofxOceanodeContainer::showNodesInCanvas(')
    code+='\nvoid ofxOceanodeCanvas::center(){'+block(canvas,'if(pendingCenterNode != nullptr &&\n')+'\n'+block(canvas,'if(hasPendingCenterPoint &&\n')+'}\n'+CASES
    with tempfile.TemporaryDirectory(prefix='oceanode-unused-review-') as directory:
        work=Path(directory);(work/'imconfig.h').write_text('')
        cpp=work/'test.cpp';cpp.write_text(code);binary=work/'test'
        if os.environ.get('OCEANODE_KEEP_TEST_SOURCE'): Path(os.environ['OCEANODE_KEEP_TEST_SOURCE']).write_text(code)
        subprocess.run([os.environ.get('CXX','clang++'),'-std=c++17','-O1','-g','-fsanitize=address,undefined',
                        '-I',str(IMGUI),'-I',str(work),str(cpp),*[str(IMGUI/name) for name in
                        ('imgui.cpp','imgui_draw.cpp','imgui_widgets.cpp','imgui_tables.cpp')],'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)

if __name__=='__main__':main()
