#!/usr/bin/env python3
"""Exercise production New-preset and teardown code with isolated dependencies.

ASan/UBSan cover nested ownership, both node collections, MIDI raw listeners,
scope/timeline cleanup, repeated New, queued loads, and deferred canvas centering.
Does not link or launch an openFrameworks host or exercise hardware MIDI.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def block(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


FIXTURE = r'''
#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
using namespace std;
struct ofxOceanodeContainer;
struct ofxOceanodeNode;
int destroyedNodes=0, notifiedNodes=0, destroyedBindings=0, destroyedConnections=0;
struct Parameter { bool scoped=true, timelined=true; };
struct Listeners { int count=1; void unsubscribeAll(){count=0;} };
struct ofxOceanodeScope {
    vector<Parameter*> parameters; function<void()> callback=[]{}; int clears=0;
    static auto getInstance(){static ofxOceanodeScope scope; return &scope;}
    void setScopeChangedCallback(function<void()> fn){callback=fn;}
    void clearScopedParameters(){for(auto* p:parameters) p->scoped=false; parameters.clear(); ++clears;}
};
struct MidiBinding {
    Parameter* parameter; set<MidiBinding*>* registered;
    ~MidiBinding(){assert(!registered->count(this)); assert(!parameter->scoped); ++destroyedBindings;}
};
struct MidiInput {
    set<MidiBinding*> registered;
    void removeListener(MidiBinding* p){registered.erase(p);}
};
struct Connection {
    bool prepared=false;
    void prepareForDestruction(){prepared=true;}
    ~Connection(){assert(prepared); ++destroyedConnections;}
};
struct Timeline {
    vector<Parameter*> parameters;
    void clear(){for(auto* p:parameters) p->timelined=false; parameters.clear();}
};
struct ofxOceanodeNode {
    Parameter parameter; ofxOceanodeContainer* owner; bool persistent;
    unique_ptr<ofxOceanodeContainer> nested;
    ofxOceanodeNode(ofxOceanodeContainer* c,bool p):owner(c),persistent(p){}
    ~ofxOceanodeNode(); void deleteSelf();
};
struct ofxOceanodeContainer {
    using Nodes=unordered_map<string,unordered_map<int,shared_ptr<ofxOceanodeNode>>>;
    Nodes dynamicNodes,persistentNodes;
    unordered_map<string,ofxOceanodeNode*> parameterGroupNodesMap;
    vector<unique_ptr<Connection>> connections;
    Listeners destroyConnectionListeners,destroyNodeListeners,midiUnregisterlisteners,midiSenderListeners;
    map<string,vector<shared_ptr<MidiBinding>>> midiBindings,persistentMidiBindings;
    map<string,MidiInput> midiIns;
    unique_ptr<Timeline> timelineManager;
    vector<int> customGuiPanels{1},customGuiPanelsData{1},customGuiSnapshotBanks{1},comments{1};
    map<int,int> customGuiPanelParameterIndex{{1,1}},customGuiPublishedParameterIndex{{1,1}},userEditedValues{{1,1}};
    string customGuiStoragePath="SavedPreset",pendingCustomGuiName="Old",pendingDeletedCustomGuiPanelId="Old",pendingCustomGuiParameterPath="Old";
    bool customGuisDirty=true,customGuiSnapshotsDirty=true,customGuiParametersNeedPruning=true,customGuiCreateModalOpen=true;
    bool scopeSavePending=true,clockEnabled=true,pathCacheCleared=false,membershipDirty=false;
    unsigned nodesRevision=0;
    string id="Canvas";
    string pendingScopeSavePath;
    int resets=0,scopeSaves=0;
    ~ofxOceanodeContainer(){clearContainer();}
    string getCanvasID(){return id;}
    void flushPendingScopeSave(){
        if(scopeSavePending){for(auto* p:ofxOceanodeScope::getInstance()->parameters) assert(p->scoped); ++scopeSaves; scopeSavePending=false;}
    }
    void invalidateCustomGuiParameterPathCache(){pathCacheCleared=true;}
    void invalidateCustomGuiMembershipIndex(){membershipDirty=true;}
    void setMidiClockSyncEnabled(bool enabled){clockEnabled=enabled;}
    void scheduleScopeSave(string path){pendingScopeSavePath=path;}
    void resetPhase(){assert(parameterGroupNodesMap.empty()); ++resets;}
    void attachSavedScope(string presetFolderPath);
    void clearContainer(); static bool isClearingContainers();
};
ofxOceanodeNode::~ofxOceanodeNode(){
    assert(ofxOceanodeContainer::isClearingContainers());
    assert(!parameter.scoped && !parameter.timelined);
    assert(owner->connections.empty());
    ++destroyedNodes;
}
void ofxOceanodeNode::deleteSelf(){
    assert(owner->destroyNodeListeners.count);
    assert(ofxOceanodeContainer::isClearingContainers()); ++notifiedNodes;
    auto& nodes=persistent?owner->persistentNodes:owner->dynamicNodes;
    owner->parameterGroupNodesMap.erase(persistent?"Persistent":"Dynamic");
    nodes[persistent?"Persistent":"Dynamic"].erase(1);
}
namespace glm {
struct vec2 {
    float x,y; vec2(float v=0):x(v),y(v){} vec2(float a,float b):x(a),y(b){}
    vec2 operator/(float v)const{return {x/v,y/v};}
};
}
struct ofRectangle {ofRectangle(int,int,int,int){}};
struct ofxOceanodeCanvas {
    bool isSelecting=true,someDragAppliedToSelection=true,canvasHasScolled=true,returnToOldScrolling=true;
    bool isCreatingConnection=true,portalizeSelectionRequested=true,autoLayoutSelectionRequested=true,autoLayoutCanvasRequested=true;
    string lastSelectedNode="Old",someSelectedModuleMove="Old",node_selected="Old",searchField="Old",lastSearchField="Old";
    glm::vec2 moveSelectedModulesWithDrag{42},scrolling{42},contentRegionSize{800,600}; float zoomLevel=2;
    ofRectangle selectedRect{1,1,1,1};
    int itemHovered=1,pendingCenterFrames=2,selectedSearchResultIndex=1;
    void* tempSourceParameter=this; void* tempSinkParameter=this; void* customGuiContextNode=this;
    void* portalizeSourceParameter=this; void* encapsulateSinkParameter=this; void* pendingCenterNode=this;
    char portalizeNameBuffer[256]="Old";
    vector<int> nodesDrawingOrder{1},pendingPortalAlignments{1},keyboardSlots{1},commentToSlot{1},filteredSearchResults{1};
    vector<bool> commentCheckboxStates{true};
    bool centerOriginPending=false,recenterCanvas=true,focused=false;
    string layoutIniPath="OldPreset/ImGuiLayout.ini";
    void setLayoutIniPath(string s){layoutIniPath=s;} string getUniqueID(){return "Canvas";}
    void requestFocus(){focused=true;}
    void resetForNewPreset(); void center();
};
struct ofxOceanodeShared {
    inline static string path="SavedPreset",bank="Bank",name="Saved",active="Macro",savePath="Macro.ini",loadPath="Macro.ini",layoutPath="Macro.ini";
    inline static bool loading=false; inline static function<void()> loaded;
    static void startedLoadingPreset(){assert(!loading); loading=true;}
    static void finishedLoadingPreset(){assert(loading); loading=false; loaded();}
    static void nodeSelectedInCanvas(ofxOceanodeNode* p){assert(!p);}
    static void setCurrentPresetPath(string s){path=s;} static string getCurrentPresetPath(){return path;}
    static void setCurrentBankName(string s){bank=s;} static void setCurrentPresetName(string s){name=s;}
    static string& getPendingLayoutSavePath(){return savePath;} static string& getPendingLayoutLoadPath(){return loadPath;}
    static string& getActiveCanvasLayoutPath(){return layoutPath;} static void setActiveCanvasUniqueID(string s){active=s;}
    static auto& getLayoutContentCache(){static map<string,string> cache;return cache;}
    static void updateMacrosStructure(){}
};
struct App {
    ofxOceanodeCanvas canvas; string pendingIniLoad="OldPreset.ini"; bool hasActivePreset=true;
    void onLoaded(); void onSaved(string iniPath);
};
namespace ImGui {
vector<string> items; bool clickNew=true;
string savedLayoutPath;
void SaveIniSettingsToDisk(const char* path){savedLayoutPath=path;}
const char* SaveIniSettingsToMemory(size_t* size){*size=6;return "Layout";}
bool MenuItem(const char* name,const char* =nullptr,bool=false,bool=true){items.push_back(name);return string(name)=="New" && exchange(clickNew,false);}
void Separator(){} void TextUnformatted(const char*){} void SameLine(){}
template<class... Args> void TextColored(Args...){}
}
int bankTextColor=0,presetItemTextColor=0;
struct ofxOceanodePresetsController {
    shared_ptr<ofxOceanodeContainer> container;
    bool newPresetRequested=false,newPresetCreated=true; int loadPresetInNextUpdate=1,currentBank=0,loads=0;
    map<string,string> currentPreset{{"Bank","Saved"}}; vector<string> banks{"Bank"};
    map<string,vector<string>> bankPresets{{"Bank",{"Saved","Other"}}};
    enum class PopupRequest{None,SaveAs,Delete,NewBank}; PopupRequest popupRequest=PopupRequest::SaveAs;
    unique_ptr<int> pendingSave=make_unique<int>(1);
    string saveResultText="Old",deleteBankName="Bank",deletePresetName="Saved";
    void savePreset(string,string){} void drawPresetList(){}
    void loadPreset(string,string){++loads;}
    void draw(); void update(); void newPreset();
};
'''

CASES = r'''
static void populate(ofxOceanodeContainer& c,int depth=2){
    c.timelineManager=make_unique<Timeline>();
    for(bool persistent:{false,true}){
        auto n=make_shared<ofxOceanodeNode>(&c,persistent);
        const string key=persistent?"Persistent":"Dynamic";
        (persistent?c.persistentNodes:c.dynamicNodes)[key][1]=n;
        c.parameterGroupNodesMap[key]=n.get();
        ofxOceanodeScope::getInstance()->parameters.push_back(&n->parameter);
        c.timelineManager->parameters.push_back(&n->parameter);
#ifdef OFXOCEANODE_USE_MIDI
        auto binding=make_shared<MidiBinding>(); binding->parameter=&n->parameter;
        binding->registered=&c.midiIns["Port"].registered;
        binding->registered->insert(binding.get());
        (persistent?c.persistentMidiBindings:c.midiBindings)[key].push_back(binding);
#endif
        if(!persistent && depth>0){n->nested=make_unique<ofxOceanodeContainer>();n->nested->id="Macro";populate(*n->nested,depth-1);}
    }
    c.connections.push_back(make_unique<Connection>());
    c.connections.push_back(make_unique<Connection>());
}
int main(){
    App app; ofxOceanodeShared::loaded=[&]{app.onLoaded();};
    ofxOceanodePresetsController presets; presets.container=make_shared<ofxOceanodeContainer>();
    auto& c=*presets.container; populate(c);
    presets.draw();
    assert(ImGui::items.front()=="New" && presets.newPresetRequested);
    assert(destroyedNodes==0 && !c.dynamicNodes.empty()); // no mutation inside menu draw
    presets.update();
    assert(destroyedNodes==6 && notifiedNodes==6 && destroyedConnections==6);
#ifdef OFXOCEANODE_USE_MIDI
    assert(destroyedBindings==6 && !c.clockEnabled && c.midiIns["Port"].registered.empty());
#endif
    assert(c.dynamicNodes.empty() && c.persistentNodes.empty() && c.parameterGroupNodesMap.empty());
    assert(c.customGuiPanels.empty() && c.customGuiPanelsData.empty() && c.customGuiSnapshotBanks.empty());
    assert(c.comments.empty() && c.userEditedValues.empty() && c.customGuiStoragePath.empty());
    assert(c.pathCacheCleared && c.membershipDirty && !c.customGuiCreateModalOpen && !c.customGuisDirty && !c.customGuiSnapshotsDirty);
    assert(!c.destroyNodeListeners.count && !c.destroyConnectionListeners.count);
    assert(c.scopeSaves==1 && !ofxOceanodeScope::getInstance()->callback && ofxOceanodeScope::getInstance()->parameters.empty());
    assert(!ofxOceanodeContainer::isClearingContainers() && !ofxOceanodeShared::loading);
    assert(presets.currentBank==-1 && presets.currentPreset["Bank"].empty() && !presets.pendingSave);
    assert(presets.loads==0 && presets.loadPresetInNextUpdate==0);
    assert(presets.banks==vector<string>{"Bank"} && presets.bankPresets["Bank"]==vector<string>({"Saved","Other"}));
    assert(ofxOceanodeShared::path.empty() && ofxOceanodeShared::name.empty() && ofxOceanodeShared::bank.empty());
    assert(ofxOceanodeShared::savePath.empty() && ofxOceanodeShared::loadPath.empty() && ofxOceanodeShared::layoutPath.empty());
    assert(!app.hasActivePreset && app.pendingIniLoad.empty() && app.canvas.layoutIniPath.empty());
    assert(ofxOceanodeShared::active=="Canvas" && app.canvas.focused);
    assert(!app.canvas.pendingCenterNode && !app.canvas.tempSourceParameter && !app.canvas.customGuiContextNode);
    assert(!app.canvas.isSelecting && !app.canvas.isCreatingConnection && app.canvas.keyboardSlots.empty());
    app.canvas.contentRegionSize={0,0}; app.canvas.center(); assert(app.canvas.centerOriginPending);
    app.canvas.contentRegionSize={800,600}; app.canvas.center();
    assert(app.canvas.scrolling.x==200 && app.canvas.scrolling.y==150 && !app.canvas.centerOriginPending && !app.canvas.recenterCanvas);
    // Saving the fresh patch attaches autosaves to its new destination.
    c.attachSavedScope("FreshPreset");
    assert(ofxOceanodeScope::getInstance()->callback);
    ofxOceanodeScope::getInstance()->callback();
    assert(c.pendingScopeSavePath=="FreshPreset");
    ofxOceanodeContainer macro; macro.id="Macro"; macro.attachSavedScope("MacroPreset");
    ofxOceanodeScope::getInstance()->callback();
    assert(c.pendingScopeSavePath=="FreshPreset" && macro.pendingScopeSavePath.empty());
    app.onSaved("FreshPreset/ImGuiLayout.ini");
    assert(ImGui::savedLayoutPath=="FreshPreset/ImGuiLayout.ini");
    assert(ofxOceanodeShared::layoutPath==ImGui::savedLayoutPath);
    // Repeated New is harmless; freshly created ownership can be cleared again.
    presets.newPresetRequested=true; presets.update(); assert(destroyedNodes==6);
    c.destroyNodeListeners.count=1; populate(c); presets.newPresetRequested=true; presets.update();
    assert(destroyedNodes==12 && notifiedNodes==12 && destroyedConnections==12);
    cout<<"New preset regression passed: deferred menu action, nested teardown, persistent nodes, MIDI listeners, scopes, timeline ordering, preset/layout identity, repeated resets and canvas centering.\n";
}
'''


def main():
    container = (ROOT / "src/Managers/ofxOceanodeContainer.cpp").read_text()
    presets = (ROOT / "src/Controls/ofxOceanodePresetsController.cpp").read_text()
    canvas = (ROOT / "src/Managers/ofxOceanodeCanvas.cpp").read_text()
    app = (ROOT / "src/ofxOceanode.cpp").read_text()
    guard = container[container.index("    thread_local unsigned int containerClearDepth"):
                      container.index("    struct ConnectionFailureInfo")]
    production = "namespace {\n" + guard + "}\n"
    production += block(container, "bool ofxOceanodeContainer::isClearingContainers()")
    production += block(container, "void ofxOceanodeContainer::clearContainer()")
    for method in ("draw", "update", "newPreset"):
        production += block(presets, f"void ofxOceanodePresetsController::{method}()")
    production += block(canvas, "void ofxOceanodeCanvas::resetForNewPreset()")
    production += "void ofxOceanodeCanvas::center(){" + block(canvas, "if(centerOriginPending &&") + "}\n"
    production += "void App::onLoaded(){" + block(app, "if(ofxOceanodeShared::getCurrentPresetPath().empty())") + "}\n"
    production += "void App::onSaved(string iniPath){auto& activeLayoutPath=ofxOceanodeShared::getActiveCanvasLayoutPath();"
    production += block(app, "if(activeLayoutPath.empty() || activeLayoutPath == iniPath)") + "}\n"
    save_method = block(container, "void ofxOceanodeContainer::savePreset(string presetFolderPath)")
    production += "void ofxOceanodeContainer::attachSavedScope(string presetFolderPath){const bool ownsGlobalScope=getCanvasID().empty() || getCanvasID()==\"Canvas\" || getCanvasID()==\"0\";"
    production += block(save_method, "if(ownsGlobalScope){") + "}\n"
    with tempfile.TemporaryDirectory(prefix="oceanode-new-preset-") as tmp:
        source = Path(tmp) / "regression.cpp"
        source.write_text("#include <utility>\n" + FIXTURE + production + CASES)
        for midi in (False, True):
            binary = Path(tmp) / ("midi" if midi else "default")
            flags = ["-DOFXOCEANODE_USE_MIDI"] if midi else []
            subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++17", "-g",
                            "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                            *flags, str(source), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
