#!/usr/bin/env python3
"""Exercise the non-modal cleanup review and candidate navigation with real ImGui.

Uses the production review window, showNodeInCanvas and deferred centering with
isolated graph services. Runs mouse events under ASan/UBSan; no OF/GPU host.
"""
import os
from pathlib import Path
import subprocess
import tempfile
from canvas_navigation_regression import ROOT, IMGUI, FIXTURE, block

EXTRA = r'''
string ofToString(size_t value){return to_string(value);}
struct ofxOceanode {
    ofxOceanodeContainer::UnusedNodePlan unusedNodePlan;
    weak_ptr<ofxOceanodeContainer> unusedNodeContainer;
    bool showUnusedNodesWindow=false,focusUnusedNodesWindow=false,unusedNodesPlanChanged=false;
    string unusedNodesSelectedNode;
    void drawUnusedNodesWindow();
    void open(shared_ptr<ofxOceanodeContainer> target){
        unusedNodeContainer=target;unusedNodePlan=target->findUnusedNodes();
        showUnusedNodesWindow=focusUnusedNodesWindow=true;
        unusedNodesSelectedNode.clear();unusedNodesPlanChanged=false;
    }
};
map<string,ImVec2> rowPoints,buttonPoints;
bool trackedSelectable(const char* name,bool selected){
    bool clicked=ImGui::Selectable(name,selected);
    auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
    rowPoints[name]={(a.x+b.x)*.5f,(a.y+b.y)*.5f};return clicked;
}
bool trackedButton(const char* name,ImVec2 size){
    bool clicked=ImGui::Button(name,size);
    auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
    buttonPoints[name]={(a.x+b.x)*.5f,(a.y+b.y)*.5f};return clicked;
}
'''

CASES = r'''
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
    root->plan.groups={{"1"}};app.open(root);idle();
    auto* review=ImGui::FindWindowByName("Remove unused nodes");assert(review);
    assert(!(review->Flags&(ImGuiWindowFlags_Modal|ImGuiWindowFlags_Popup)));
    click(underlying);assert(underlyingClicks==1&&app.showUnusedNodesWindow);
    click(rowPoints.at("1"));
    assert(ofxOceanodeShared::selected==candidate&&candidate->gui.selected);
    assert(app.unusedNodesSelectedNode=="1"&&app.showUnusedNodesWindow);centered(rootView,candidate);

    // The reviewed Macro may have been hidden since opening the review.
    inner->container->plan.groups={{"4"}};
    inner->container->plan.protectedGroups={{{"5"},{"Unknown model"},true}};
    app.open(inner->container);idle();click(rowPoints.at("4"));
    assert(inner->visible&&inner->activations==1&&ofxOceanodeShared::selected==nested);
    assert(ofxOceanodeShared::active==inner->canvas.uniqueID&&app.showUnusedNodesWindow);
    views={&inner->canvas};idle();centered(inner->canvas,nested);
    click(rowPoints.at("5"));
    assert(protectedNode->gui.selected&&!nested->gui.selected&&app.unusedNodesSelectedNode=="5");
    centered(inner->canvas,protectedNode);

    // Graph changes must not navigate to replacement nodes with the same name.
    inner->container->nodes.erase(4);add(*inner->container,4);++inner->container->revision;
    auto* selected=ofxOceanodeShared::selected;idle();click(rowPoints.at("4"));
    assert(ofxOceanodeShared::selected==selected);
    click(buttonPoints.at("Remove"));assert(inner->container->removals==0&&app.showUnusedNodesWindow);
    click(buttonPoints.at("Refresh"));assert(app.unusedNodePlan.revision==inner->container->revision);
    click(rowPoints.at("4"));assert(ofxOceanodeShared::selected==inner->container->nodes[4].get());

    // Usage-only changes are revalidated on Remove; the revised proposal stays open.
    inner->container->removalAllowed=false;click(buttonPoints.at("Remove"));
    assert(app.showUnusedNodesWindow&&app.unusedNodesPlanChanged&&inner->container->removals==1);
    inner->container->removalAllowed=true;click(buttonPoints.at("Remove"));
    assert(!app.showUnusedNodesWindow&&inner->container->removals==2);

    // Expired canvases keep a readable, closable window with disabled actions.
    auto temporary=make_shared<ofxOceanodeContainer>();temporary->plan.groups={{"7"}};add(*temporary,7);
    app.open(temporary);idle();temporary.reset();idle();click(rowPoints.at("7"));
    assert(app.showUnusedNodesWindow);click(buttonPoints.at("Remove"));assert(app.showUnusedNodesWindow);
    click(buttonPoints.at("Close"));assert(!app.showUnusedNodesWindow);
    ImGui::DestroyContext();
    puts("Cleanup review passed: non-modal interaction, root/nested/protected navigation, zoom, stale/replaced nodes, refresh, usage revalidation and expired canvases.");
}
'''

def main():
    header=(ROOT/'src/Managers/ofxOceanodeContainer.h').read_text()
    fixture=FIXTURE.replace('    map<int,unique_ptr<ofxOceanodeNode>> nodes;',block(header,'    struct UnusedNodePlan')+r''';
    UnusedNodePlan plan;uint64_t revision=0;int removals=0;bool removalAllowed=true;
    uint64_t getNodesRevision(){return revision;}
    string getCanvasID(){return "Reviewed canvas";}
    UnusedNodePlan findUnusedNodes(){plan.revision=revision;return plan;}
    bool removeUnusedNodes(const UnusedNodePlan&){++removals;return removalAllowed;}
    map<int,unique_ptr<ofxOceanodeNode>> nodes;''')
    fixture=fixture.replace('map<int,ofxOceanodeNode*>','map<string,ofxOceanodeNode*>').replace('out[p.first]=p.second.get()','out[to_string(p.first)]=p.second.get()')
    app=(ROOT/'src/ofxOceanode.cpp').read_text()
    window=block(app,'void ofxOceanode::drawUnusedNodesWindow()')
    window=window.replace('ImGui::Selectable(', 'trackedSelectable(').replace('ImGui::Button(', 'trackedButton(')
    container=(ROOT/'src/Managers/ofxOceanodeContainer.cpp').read_text()
    canvas=(ROOT/'src/Managers/ofxOceanodeCanvas.cpp').read_text()
    code=fixture+EXTRA+window+block(container,'bool ofxOceanodeContainer::showNodeInCanvas(')
    code+='\nvoid ofxOceanodeCanvas::center(){'+block(canvas,'if(pendingCenterNode != nullptr &&\n')+'}\n'+CASES
    with tempfile.TemporaryDirectory(prefix='oceanode-unused-review-') as directory:
        work=Path(directory);(work/'imconfig.h').write_text('')
        cpp=work/'test.cpp';cpp.write_text(code);binary=work/'test'
        subprocess.run([os.environ.get('CXX','clang++'),'-std=c++17','-O1','-g','-fsanitize=address,undefined',
                        '-I',str(IMGUI),'-I',str(work),str(cpp),*[str(IMGUI/name) for name in
                        ('imgui.cpp','imgui_draw.cpp','imgui_widgets.cpp','imgui_tables.cpp')],'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)

if __name__=='__main__':main()
