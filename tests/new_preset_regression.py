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
#include <nlohmann/json.hpp>
using ofJson = nlohmann::json;
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
    unique_ptr<Timeline> automationExtension;
    struct Gui { bool cleared=false; void clear(){cleared=true;} };
    unique_ptr<Gui> customGuiExtension=make_unique<Gui>();
    vector<int> comments{1}; map<int,int> userEditedValues{{1,1}};
    map<string,string> unavailableCustomGuiFiles,unavailableNodeFiles;
    map<string,int> unavailableNodeKeys;
    ofJson unavailableModules=0,unavailableConnections=0;
    string presetStoragePath="SavedPreset";
    bool scopeSavePending=true,clockEnabled=true;
    unsigned nodesRevision=0;
    string id="Canvas";
    string pendingScopeSavePath;
    unsigned long long pendingScopeSaveDeadlineMillis=0;
    vector<string> savedScopes;
    void saveScope(const string& path){savedScopes.push_back(path);}
    void flushProductionScopeSave();
    map<string,int> canvasLiveNodes{{"Old",1}};
    void clearCanvasHistory(){}
    int resets=0,scopeSaves=0;
    ~ofxOceanodeContainer(){clearContainer();}
    string getCanvasID(){return id;}
    void flushPendingScopeSave(){
        if(scopeSavePending){for(auto* p:ofxOceanodeScope::getInstance()->parameters) assert(p->scoped); ++scopeSaves; scopeSavePending=false;}
    }
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
    struct {string nodeID;} inspectorSelectionClick{"Old"};
    void* tempSourceParameter=this; void* tempSinkParameter=this; void* customGuiContextNode=this;
    void* portalizeSourceParameter=this; void* encapsulateSinkParameter=this; void* pendingCenterNode=this;
    char portalizeNameBuffer[256]="Old";
    vector<int> nodesDrawingOrder{1},pendingPortalAlignments{1},keyboardSlots{1},commentToSlot{1},filteredSearchResults{1};
    vector<bool> commentCheckboxStates{true};
    bool centerOriginPending=false,recenterCanvas=true,focused=false;
    string getUniqueID(){return "Canvas";}
    void requestFocus(){focused=true;}
    void resetForNewPreset(); void resetViewToOrigin(); void center();
};
struct ofxOceanodeShared {
    inline static string path="SavedPreset",bank="Bank",name="Saved",active="Macro";
    inline static bool loading=false; inline static function<void()> loaded;
    static void startedLoadingPreset(){assert(!loading); loading=true;}
    static void finishedLoadingPreset(){assert(loading); loading=false; loaded();}
    static void nodeSelectedInCanvas(ofxOceanodeNode* p){assert(!p);}
    static void setCurrentPresetPath(string s){path=s;} static string getCurrentPresetPath(){return path;}
    static void setCurrentBankName(string s){bank=s;} static void setCurrentPresetName(string s){name=s;}
    static void setActiveCanvasUniqueID(string s){active=s;}
    static void updateMacrosStructure(){}
    struct DockingSaveEvent {vector<string> fired; void notify(const string& path){fired.push_back(path);}};
    static DockingSaveEvent& getDockingSaveRequestEvent(){static DockingSaveEvent event; return event;}
};
string ofToDataPath(string path){return path;}
struct ofFile {ofFile(string){} bool exists(){return false;}};
ofJson ofLoadJson(const string&){return {};}
bool validView(const ofJson&){return true;}
struct Logger {template<class T> Logger& operator<<(const T&){return *this;}};
Logger ofLogError(const string&){return {};}
struct ofxOceanodePresetLayouts {
    int entries=0;
    void fromJson(const ofJson&){} string layoutOnLoad() const {return "";}
};
struct App {
    ofxOceanodeCanvas canvas;
    string pendingIni="OldPreset ini",pendingSnapshot="Old",layoutError="Old";
    ofJson pendingView=ofJson::object(),savedView=ofJson::object();
    ofxOceanodePresetLayouts collection{1}; bool readFailed=true;
    enum class PresetLayoutPopup {None,Save}; PresetLayoutPopup presetPopup=PresetLayoutPopup::Save;
    struct Windows {int captures=0; ofJson captureView(){++captures; return ofJson::object({{"controllers",ofJson::object()}});}} windows;
    int writes=0; bool writeLayoutFile(){++writes; return true;}
    void onLoaded(); void onLayoutLoaded(); void onSaved(); void saveDocking(const string&);
};
namespace ImGui {
vector<string> items; bool clickNew=true;
bool MenuItem(const char* name,const char* =nullptr,bool=false,bool=true){items.push_back(name);return string(name)=="New Preset" && exchange(clickNew,false);}
void Separator(){} void TextUnformatted(const char*){} void SameLine(){}
template<class... Args> void TextColored(Args...){}
}
int bankTextColor=0,presetItemTextColor=0;
namespace ofxOceanodeMenu { void section(const char*){} }
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
    c.automationExtension=make_unique<Timeline>();
    for(bool persistent:{false,true}){
        auto n=make_shared<ofxOceanodeNode>(&c,persistent);
        const string key=persistent?"Persistent":"Dynamic";
        (persistent?c.persistentNodes:c.dynamicNodes)[key][1]=n;
        c.parameterGroupNodesMap[key]=n.get();
        ofxOceanodeScope::getInstance()->parameters.push_back(&n->parameter);
        c.automationExtension->parameters.push_back(&n->parameter);
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
    assert(ImGui::items.front()=="New Preset" && presets.newPresetRequested);
    assert(destroyedNodes==0 && !c.dynamicNodes.empty()); // no mutation inside menu draw
    presets.update();
    assert(destroyedNodes==6 && notifiedNodes==6 && destroyedConnections==6);
#ifdef OFXOCEANODE_USE_MIDI
    assert(destroyedBindings==6 && !c.clockEnabled && c.midiIns["Port"].registered.empty());
#endif
    assert(c.dynamicNodes.empty() && c.persistentNodes.empty() && c.parameterGroupNodesMap.empty());
    assert(c.customGuiExtension && c.customGuiExtension->cleared);
    assert(c.comments.empty() && c.userEditedValues.empty() && c.presetStoragePath.empty());
    assert(c.unavailableCustomGuiFiles.empty() && c.unavailableNodeFiles.empty());
    assert(!c.destroyNodeListeners.count && !c.destroyConnectionListeners.count);
    assert(c.scopeSaves==1 && !ofxOceanodeScope::getInstance()->callback && ofxOceanodeScope::getInstance()->parameters.empty());
    assert(!ofxOceanodeContainer::isClearingContainers() && !ofxOceanodeShared::loading);
    assert(presets.currentBank==-1 && presets.currentPreset["Bank"].empty() && !presets.pendingSave);
    assert(presets.loads==0 && presets.loadPresetInNextUpdate==0);
    assert(presets.banks==vector<string>{"Bank"} && presets.bankPresets["Bank"]==vector<string>({"Saved","Other"}));
    assert(ofxOceanodeShared::path.empty() && ofxOceanodeShared::name.empty() && ofxOceanodeShared::bank.empty());
    assert(app.pendingIni.empty() && app.pendingSnapshot.empty() && app.pendingView.is_null() && app.savedView.is_null());
    assert(!app.readFailed && app.layoutError.empty() && app.collection.entries==0);
    assert(ofxOceanodeShared::active=="Canvas" && app.canvas.focused);
    assert(!app.canvas.pendingCenterNode && !app.canvas.tempSourceParameter && !app.canvas.customGuiContextNode);
    assert(app.canvas.inspectorSelectionClick.nodeID.empty());
    assert(!app.canvas.isSelecting && !app.canvas.isCreatingConnection && app.canvas.keyboardSlots.empty());
    app.canvas.contentRegionSize={0,0}; app.canvas.center(); assert(app.canvas.centerOriginPending);
    app.canvas.contentRegionSize={800,600}; app.canvas.center();
    assert(app.canvas.scrolling.x==200 && app.canvas.scrolling.y==150 && !app.canvas.centerOriginPending && !app.canvas.recenterCanvas);
    // Loaded presets center world (0,0) using the restored viewport, at every zoom.
    // A canvas first shown later (e.g. a hidden Macro) must retain its request.
    for(float zoom:{.25f,1.f,2.f}){
        for(glm::vec2 viewport:{glm::vec2(320,180),glm::vec2(900,640)}){
            ofxOceanodeCanvas view;
            view.zoomLevel=zoom; view.scrolling={-400,700};
            view.resetViewToOrigin();
            assert(view.centerOriginPending && !view.returnToOldScrolling);
            assert(!view.pendingCenterNode && view.pendingCenterFrames==0 && view.inspectorSelectionClick.nodeID.empty());
            view.contentRegionSize={0,0}; view.center(); assert(view.centerOriginPending);
            view.contentRegionSize=viewport; view.center();
            assert(!view.centerOriginPending && view.zoomLevel==zoom);
            assert(view.scrolling.x*zoom==viewport.x*.5f && view.scrolling.y*zoom==viewport.y*.5f);
            // Once reset is complete, ordinary panning stays where the user puts it.
            view.scrolling={17,23}; view.center();
            assert(view.scrolling.x==17 && view.scrolling.y==23);
        }
    }
    // Saving the fresh patch attaches autosaves to its new destination.
    c.attachSavedScope("FreshPreset");
    assert(ofxOceanodeScope::getInstance()->callback);
    ofxOceanodeScope::getInstance()->callback();
    assert(c.pendingScopeSavePath=="FreshPreset");
    ofxOceanodeContainer macro; macro.id="Macro"; macro.attachSavedScope("MacroPreset");
    ofxOceanodeScope::getInstance()->callback();
    assert(c.pendingScopeSavePath=="FreshPreset" && macro.pendingScopeSavePath.empty());
    ofxOceanodeShared::path="FreshPreset";app.onSaved();
    assert(app.windows.captures==1 && app.writes==1 && app.savedView.contains("controllers"));
    // The debounced scope autosave asks Layouts to store the fresh preset's docking.
    c.scopeSavePending=true; c.flushProductionScopeSave();
    assert(c.savedScopes==vector<string>{"FreshPreset"} && c.pendingScopeSavePath.empty() && !c.scopeSavePending);
    assert(ofxOceanodeShared::getDockingSaveRequestEvent().fired==vector<string>{"FreshPreset"});
    app.saveDocking("OtherPreset"); assert(app.writes==1);
    app.saveDocking("FreshPreset"); assert(app.writes==2);
    // Repeated New is harmless; freshly created ownership can be cleared again.
    presets.newPresetRequested=true; presets.update(); assert(destroyedNodes==6);
    c.destroyNodeListeners.count=1; populate(c); presets.newPresetRequested=true; presets.update();
    assert(destroyedNodes==12 && notifiedNodes==12 && destroyedConnections==12);
    cout<<"New preset regression passed: deferred menu action, nested teardown, persistent nodes, MIDI listeners, scopes, timeline ordering, preset/layout identity, repeated resets and loaded-preset origin centering at multiple viewport sizes/zooms.\n";
}
'''


def main():
    container = (ROOT / "src/Managers/ofxOceanodeContainer.cpp").read_text()
    presets = (ROOT / "src/Controls/ofxOceanodePresetsController.cpp").read_text()
    canvas = (ROOT / "src/Managers/ofxOceanodeCanvas.cpp").read_text()
    app = (ROOT / "src/ofxOceanode.cpp").read_text()
    layouts = (ROOT / "src/Gui/ofxOceanodeLayouts.cpp").read_text()
    guard = container[container.index("    thread_local unsigned int containerClearDepth"):
                      container.index("    struct ConnectionFailureInfo")]
    production = "namespace {\n" + guard + "}\n"
    production += block(container, "bool ofxOceanodeContainer::isClearingContainers()")
    production += block(container, "void ofxOceanodeContainer::clearContainer()")
    for method in ("draw", "update", "newPreset"):
        production += block(presets, f"void ofxOceanodePresetsController::{method}()")
    production += block(canvas, "void ofxOceanodeCanvas::resetViewToOrigin()")
    production += block(canvas, "void ofxOceanodeCanvas::resetForNewPreset()")
    production += "void ofxOceanodeCanvas::center(){" + block(canvas, "if(centerOriginPending &&") + "}\n"
    manager_load = block(layouts, "void ofxOceanodeLayouts::onPresetLoaded()")
    production += manager_load.replace('ofxOceanodeLayouts::onPresetLoaded()', 'App::onLayoutLoaded()')
    production += 'void App::onLoaded(){onLayoutLoaded();'
    production += block(app, "if(ofxOceanodeShared::getCurrentPresetPath().empty())") + "}\n"
    production += block(layouts, "void ofxOceanodeLayouts::onPresetSaved()").replace('ofxOceanodeLayouts::onPresetSaved()', 'App::onSaved()')
    production += block(layouts, "void ofxOceanodeLayouts::saveDocking(").replace('ofxOceanodeLayouts::', 'App::')
    production += block(container, "void ofxOceanodeContainer::flushPendingScopeSave()").replace(
        'ofxOceanodeContainer::flushPendingScopeSave()', 'ofxOceanodeContainer::flushProductionScopeSave()')
    save_method = block(container, "bool ofxOceanodeContainer::savePreset(string presetFolderPath)")
    production += "void ofxOceanodeContainer::attachSavedScope(string presetFolderPath){const bool ownsGlobalScope=getCanvasID().empty() || getCanvasID()==\"Canvas\" || getCanvasID()==\"0\";"
    production += block(save_method, "if(ownsGlobalScope){") + "}\n"
    with tempfile.TemporaryDirectory(prefix="oceanode-new-preset-") as tmp:
        source = Path(tmp) / "regression.cpp"
        source.write_text("#include <utility>\n" + FIXTURE + production + CASES)
        for midi in (False, True):
            binary = Path(tmp) / ("midi" if midi else "default")
            flags = ["-DOFXOCEANODE_USE_MIDI"] if midi else []
            subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++17", "-g", "-I", str(ROOT.parents[1] / "libs/json/include"),
                            "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                            *flags, str(source), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
