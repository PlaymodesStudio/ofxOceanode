#!/usr/bin/env python3
"""Exercise production layout capture/recall with real ImGui docking, ASan/UBSan.

Graph/OF types are isolated; window toggles, nested traversal, layout application,
and layout operations are production code. Does not launch a graphics host.
"""
from pathlib import Path
import os
import subprocess
import tempfile
from new_preset_regression import block

ROOT = Path(__file__).resolve().parents[1]
IMGUI = Path(os.environ.get('OCEANODE_TEST_IMGUI_SRC', str(ROOT.parent / 'ofxImGui/libs/imgui/src')))
JSON = ROOT.parents[1] / 'libs/json/include'

FIXTURE = r'''
#include "Layouts/ofxOceanodePresetLayouts.h"
#include "ofxOceanodeWindowVisibility.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <functional>
#include <filesystem>
#include <memory>
#include <sstream>
#include <vector>
using namespace std;
using ofJson = nlohmann::json;
struct Abstract { string name; virtual ~Abstract()=default; string getName(){return name;} };
template<class T> struct ofParameter:Abstract {
    T value; ofParameter(string n,T v):value(v){name=n;}
    T get(){return value;} void set(T v){value=v;}
};
template<class T> struct ofxOceanodeParameter:Abstract {
    ofParameter<T> parameter;
    ofxOceanodeParameter(string n,T v):parameter(n,v){name=n;}
    ofParameter<T>& getParameter(){return parameter;}
};
using ofParameterGroup = vector<shared_ptr<Abstract>>;
struct ofxOceanodeContainer;
struct ofxOceanodeNodeModel {
    virtual ~ofxOceanodeNodeModel()=default;
    string name="Synth";int number=1;
    string nodeName(){return name;}int getNumIdentifier(){return number;}
};
struct ofxOceanodeNode {
    bool persistent=false;
    bool getIsPersistent(){return persistent;}
    shared_ptr<ofxOceanodeNodeModel> model=make_shared<ofxOceanodeNodeModel>();
    ofParameterGroup parameters,inspector;
    auto& getNodeModel(){return *model;}
    auto& getParameters(){return parameters;}auto& getInspectorParameters(){return inspector;}
};
struct WindowState {bool isOpen=false;};
struct ofxOceanodeScope {
    bool visible=true;
    static ofxOceanodeScope* getInstance(){static ofxOceanodeScope s;return &s;}
    bool isVisible(){return visible;} void setVisible(bool v){visible=v;}
};
struct Panel {string id;WindowState windowState;};
struct ofxOceanodeContainer {
    vector<ofxOceanodeNode*> nodes; vector<Panel> panels;
    bool scopeSavePending=false; string pendingScopeSavePath,lastScopeSave;
    unsigned long long pendingScopeSaveDeadlineMillis=0;
    void saveScope(const string& path){lastScopeSave=path;}
    void flushPendingScopeSave();
    auto getAllModules(){return nodes;}
    auto* getCustomGuiExtension(){return this;}
    void windowState(ofJson& state,bool restore){
        for(auto& p:panels){
            if(!restore)state["panels"][p.id]=p.windowState.isOpen;
            else if(state.contains("panels")&&state["panels"].contains(p.id))p.windowState.isOpen=state["panels"][p.id].get<bool>();
        }
    }
    Panel* getCustomGuiPanelData(string id){for(auto& p:panels)if(p.id==id)return &p;return nullptr;}
};
struct ofxOceanodeNodeMacro:ofxOceanodeNodeModel {
    shared_ptr<ofxOceanodeContainer> child=make_shared<ofxOceanodeContainer>();
    bool visible=true;int forcedDock=0;
    bool isWindowVisible(){return visible;}
    void setWindowVisible(bool b,bool focus=true){visible=b;if(focus)++forcedDock;}
    auto getContainer(){return child;}
};
struct Controller {string name;string getControllerName(){return name;}bool isMenuController(){return false;}};
struct Controls {
    map<string,bool> visibility{{"Nodes",true},{"Timeline",true}};
    vector<shared_ptr<Controller>> controllers{make_shared<Controller>(Controller{"Nodes"}),make_shared<Controller>(Controller{"Timeline"})};
    auto& getControllersVisibility(){return visibility;} auto& getControllers(){return controllers;}
};
struct ofxOceanodeShared {
    inline static string preset;
    static string getCurrentPresetPath(){return preset;}
    static void setLeftNodeID(ImGuiID){} static void setCentralNodeID(ImGuiID){}
};
struct Policy {void reset(){}};
vector<string> ofSplitString(const string& text,const string&,bool,bool){
    vector<string> out;istringstream s(text);string line;while(getline(s,line))out.push_back(line);return out;
}
string ofTrim(string s){auto a=s.find_first_not_of(" \r\t");if(a==string::npos)return {};return s.substr(a,s.find_last_not_of(" \r\t")-a+1);}
string ofToDataPath(const string& path,bool absolute=false){return path;}
struct ofFile {
    string value;ofFile(string p):value(p){}bool exists(){return filesystem::exists(value);}
    string path(){return value;}
};
struct ofBuffer {
    string text;string getText(){return text;}void set(const char* data,size_t n){text.assign(data,n);}
};
ofBuffer ofBufferFromFile(string path){ifstream input(path);return {string(istreambuf_iterator<char>(input),{})};}
bool ofBufferToFile(string path,ofBuffer data){ofstream output(path);output<<data.text;return bool(output);}

ofJson ofLoadJson(const string& path){ifstream input(path);return ofJson::parse(input);}
struct Logger {template<class T>Logger& operator<<(const T&){return *this;}};
Logger ofLogError(const string&){return {};}
Logger ofLogNotice(const string&){return {};}
namespace oceanodePresetSave {
    bool failWrite=false;
    bool saveJson(const string& path,const ofJson& json){
        if(failWrite)return false;
        filesystem::create_directories(filesystem::path(path).parent_path());
        ofstream output(path);output<<json.dump();return bool(output);
    }
}
using ofxOceanodeControls=Controls;
namespace oceanodeLayouts {
    ofJson captureWindows(ofxOceanodeContainer*,ofxOceanodeControls&);
    void restoreWindows(ofxOceanodeContainer*,ofxOceanodeControls&,ofJson);
}
struct ofxOceanodeLayoutManager {
    shared_ptr<ofxOceanodeContainer> container=make_shared<ofxOceanodeContainer>();
    shared_ptr<Controls> controls=make_shared<Controls>();
    unique_ptr<Policy> proportionalDocking=make_unique<Policy>();
    ofxOceanodePresetLayouts presetLayouts;string globalLayoutName;string pendingIniLoad,loadedGUILayoutData;
    ImGuiID loadedGUILayoutLeftDockId=0;bool pendingControllerDockReconciliation=false;
    ofJson capturePresetLayout();void recallPresetLayout(const string&);void applyGUILayout(const string&);
    enum class PresetLayoutPopup {None,Save,Rename,Delete};
    PresetLayoutPopup presetPopup=PresetLayoutPopup::None;
    string pendingPresetLayout,layoutError;bool presetReadFailed=false;
    bool savePresetLayouts();void readPresetLayouts();
    void queueGlobalLayout(const string&, const string&);
    void onPresetLoaded();void onPresetSaved();void beforeFrame();void saveCurrentPresetLayout();void loadDefaultGlobalLayout();
    bool hasActivePreset=false;
    ofJson pendingPresetView;
    struct Callbacks {
        function<ofJson()> captureWindows;
        function<void(const ofJson&)> restoreWindows;
        function<void(const string&)> applyDocking;
    } callbacks{
        [this](){return oceanodeLayouts::captureWindows(container.get(),*controls);},
        [this](const ofJson& state){oceanodeLayouts::restoreWindows(container.get(),*controls,state);},
        [this](const string& data){applyGUILayout(data);}
    };
};
PRODUCTION
ImGuiID rootID,leftID,centerID;
void frame(bool build=false,bool laser=false){
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({1200,800});
    ImGui::Begin("Host",nullptr,ImGuiWindowFlags_NoDocking);
    rootID=ImGui::GetID("MainDock");
    if(build){
        ImGui::DockBuilderRemoveNode(rootID);
        ImGui::DockBuilderAddNode(rootID,ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(rootID,{1200,800});
        ImGui::DockBuilderSplitNode(rootID,laser?ImGuiDir_Right:ImGuiDir_Down,.3f,&leftID,&centerID);
        ImGui::DockBuilderDockWindow("Canvas",centerID);
        ImGui::DockBuilderDockWindow(laser?"Laser":"Mixer",leftID);
        ImGui::DockBuilderDockWindow("Synth",centerID);
        ImGui::DockBuilderFinish(rootID);
    }
    ImGui::DockSpace(rootID);ImGui::End();
    for(const char* name:{"Canvas","Mixer","Synth","Laser"}){ImGui::Begin(name);ImGui::TextUnformatted(name);ImGui::End();}
    ImGui::Render();
}
void settle(){for(int i=0;i<4;++i)frame();}
template<class F> void rejects(F fn){bool rejected=false;try{fn();}catch(const exception&){rejected=true;}assert(rejected);}
int main(){
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;
    io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;io.DisplaySize={1200,800};io.DeltaTime=1.f/60;
    unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    ofxOceanodeLayoutManager app;
    ofxOceanodeNode synth,macroNode,inner,newNode;
    auto show=make_shared<ofxOceanodeParameter<bool>>("Show Window",true);
    auto showNames=make_shared<ofParameter<bool>>("Show Names",true);
    auto innerShow=make_shared<ofParameter<bool>>("Show GUI",true);
    synth.parameters={show,showNames};inner.inspector={innerShow};
    auto macro=make_shared<ofxOceanodeNodeMacro>();macro->name="Macro";macroNode.model=macro;
    macro->child->nodes={&inner};macro->child->panels={{"nested",{true}}};
    app.container->nodes={&synth,&macroNode};app.container->panels={{"root",{true}}};
    frame(true);settle();auto sound=app.capturePresetLayout();
    const auto soundDock=ImGui::FindWindowByName("Mixer")->DockId;
    app.presetLayouts.create("Sound",sound);
    show->parameter.set(false);innerShow->set(false);macro->visible=false;showNames->set(false);
    app.controls->visibility["Timeline"]=false;
    app.container->panels[0].windowState.isOpen=false;macro->child->panels[0].windowState.isOpen=false;
    frame(true,true);settle();auto laser=app.capturePresetLayout();app.presetLayouts.create("Laser",laser);
    // New graph members must retain their visibility; labels cannot control identity.
    auto newShow=make_shared<ofParameter<bool>>("Show",true);newNode.model->number=2;
    newNode.parameters={newShow};app.container->nodes.push_back(&newNode);
    app.pendingIniLoad="old-preset.ini";
    app.recallPresetLayout("Sound");settle();
    assert(app.pendingIniLoad.empty());
    assert(show->parameter.get()&&innerShow->get()&&macro->visible);
    assert(!showNames->get()&&newShow->get()&&macro->forcedDock==0);
    assert(app.controls->visibility["Timeline"]&&app.container->panels[0].windowState.isOpen);
    assert(macro->child->panels[0].windowState.isOpen);
    assert(ImGui::FindWindowByName("Mixer")->DockId==soundDock);
    app.recallPresetLayout("Laser");settle();
    assert(!show->parameter.get()&&!innerShow->get()&&!macro->visible&&!app.controls->visibility["Timeline"]);
    assert(!app.container->panels[0].windowState.isOpen&&!macro->child->panels[0].windowState.isOpen);
    // Removal after capture is safe, including a whole nested macro tree.
    app.container->nodes={&synth};app.container->panels.clear();app.recallPresetLayout("Sound");settle();
    // Production preset-path save/load, first Save, Save As, missing files, New,
    // corrupt files and write errors. Named snapshots remain explicit.
    auto snapshots=app.presetLayouts.entries;
    app.pendingPresetLayout="Laser";
    app.queueGlobalLayout("config/guiLayouts/Studio.ini","Studio.ini");
    assert(app.presetLayouts.entries==snapshots&&app.presetLayouts.active.empty());
    assert(app.pendingPresetLayout.empty()&&app.pendingIniLoad=="config/guiLayouts/Studio.ini");
    assert(app.globalLayoutName=="Studio.ini");
    app.recallPresetLayout("Sound");settle();
    auto original=app.presetLayouts.toJson();
    assert(app.savePresetLayouts());assert(!filesystem::exists("preset_layouts.json"));
    ofxOceanodeShared::preset="preset-A";assert(app.savePresetLayouts());
    assert(ofLoadJson("preset-A/preset_layouts.json")==original);
    app.queueGlobalLayout("config/guiLayouts/Studio.ini","Studio.ini");
    auto globalSelection=ofLoadJson("preset-A/preset_layouts.json");
    assert(globalSelection["active"]==""&&globalSelection["layouts"]==original["layouts"]);
    app.recallPresetLayout("Sound");settle();
    assert(ofLoadJson("preset-A/preset_layouts.json")==original);
    ofxOceanodeShared::preset="preset-B";assert(app.savePresetLayouts());
    app.presetLayouts.overwrite("Sound",laser);assert(app.presetLayouts.active=="Sound"&&app.savePresetLayouts());
    assert(ofLoadJson("preset-A/preset_layouts.json")==original);
    ofxOceanodeShared::preset="preset-A";app.readPresetLayouts();
    assert(app.presetLayouts.toJson()==original&&app.pendingPresetLayout=="Sound");
    // A preset default is independent from the last selection, including after
    // saving, reopening, global-layout selection, rename, deletion and Save As.
    ofxOceanodeShared::preset="preset-default";
    app.presetLayouts.defaultLayout="Laser";
    assert(app.presetLayouts.active=="Sound"&&app.savePresetLayouts());
    app.readPresetLayouts();
    assert(app.pendingPresetLayout=="Laser"&&app.presetLayouts.active=="Sound");
    app.recallPresetLayout("Laser");settle();
    app.recallPresetLayout("Sound");settle();
    assert(app.presetLayouts.defaultLayout=="Laser");
    app.readPresetLayouts();assert(app.pendingPresetLayout=="Laser");
    app.queueGlobalLayout("Studio.ini","Studio.ini");
    app.readPresetLayouts();assert(app.pendingPresetLayout=="Laser"&&app.presetLayouts.active.empty());
    app.presetLayouts.rename("Laser","Lasers");assert(app.presetLayouts.defaultLayout=="Lasers");
    assert(app.savePresetLayouts());app.readPresetLayouts();assert(app.pendingPresetLayout=="Lasers");
    auto defaultSaveAs=app.presetLayouts;
    assert(defaultSaveAs.defaultLayout=="Lasers");
    app.recallPresetLayout("Sound");settle();
    app.presetLayouts.erase("Lasers");assert(app.presetLayouts.defaultLayout.empty());
    assert(app.savePresetLayouts());app.readPresetLayouts();assert(app.pendingPresetLayout=="Sound");
    auto legacy=original;legacy.erase("default");
    ofxOceanodePresetLayouts legacyLayouts;legacyLayouts.fromJson(legacy);
    assert(legacyLayouts.defaultLayout.empty()&&legacyLayouts.layoutOnLoad()=="Sound");
    auto missingDefault=original;missingDefault["default"]="Deleted layout";
    legacyLayouts.fromJson(missingDefault);
    assert(legacyLayouts.defaultLayout.empty()&&legacyLayouts.layoutOnLoad()=="Sound");
    ofxOceanodeShared::preset="old-preset-without-presetLayouts";app.readPresetLayouts();
    assert(app.presetLayouts.entries.empty()&&app.pendingPresetLayout.empty());
    ofxOceanodeShared::preset="preset-A";app.readPresetLayouts();
    oceanodePresetSave::failWrite=true;assert(!app.savePresetLayouts());
    assert(!app.layoutError.empty());oceanodePresetSave::failWrite=false;
    ofstream("preset-B/preset_layouts.json")<<"{broken";
    ofxOceanodeShared::preset="preset-B";app.readPresetLayouts();
    assert(app.presetLayouts.entries.empty()&&!app.layoutError.empty());
    assert(!app.savePresetLayouts());
    ifstream corrupt("preset-B/preset_layouts.json");string content;getline(corrupt,content);assert(content=="{broken");
    ofxOceanodeShared::preset="";app.readPresetLayouts();
    assert(app.presetLayouts.entries.empty()&&app.layoutError.empty());
    app.presetLayouts.fromJson(original);
    auto disk=ofJson::parse(app.presetLayouts.toJson().dump());ofxOceanodePresetLayouts reopened;reopened.fromJson(disk);
    assert(reopened.entries==app.presetLayouts.entries&&reopened.active=="Sound");
    reopened.rename("Sound","Audio");assert(reopened.active=="Audio"&&reopened.entries.at("Audio")==sound);
    rejects([&]{reopened.create("Audio",laser);});rejects([&]{reopened.rename("Audio","Laser");});
    rejects([&]{reopened.create("   ",laser);});rejects([&]{reopened.create("Name##id",laser);});
    assert(ofxOceanodePresetLayouts::cleanName("  Sound  ")=="Sound");
    auto before=reopened.toJson();auto bad=before;bad["version"]=99;
    rejects([&]{reopened.fromJson(bad);});assert(before==reopened.toJson());
    bad=before;bad["layouts"]["Audio"]["layout"]=false;
    rejects([&]{reopened.fromJson(bad);});assert(before==reopened.toJson());
    auto oldFormat=original;oldFormat["version"]=1;oldFormat["workspaces"]=oldFormat["layouts"];oldFormat.erase("layouts");
    filesystem::create_directories("legacy-preset");
    ofstream("legacy-preset/workspaces.json")<<oldFormat.dump();
    ofxOceanodeShared::preset="legacy-preset";app.onPresetLoaded();
    assert(app.pendingPresetLayout=="Sound"&&app.presetLayouts.entries.size()==2);
    assert(app.savePresetLayouts());
    auto migrated=ofLoadJson("legacy-preset/preset_layouts.json");
    assert(migrated["version"]==2&&migrated.contains("layouts")&&!migrated.contains("workspaces"));
    assert(ofLoadJson("legacy-preset/workspaces.json")==oldFormat);
    app.presetLayouts.defaultLayout="Laser";assert(app.savePresetLayouts());
    app.onPresetLoaded();assert(app.pendingPresetLayout=="Laser"); // current file wins over legacy
    ofstream("legacy-preset/preset_layouts.json")<<"broken";
    app.onPresetLoaded();assert(app.presetReadFailed&&app.presetLayouts.entries.empty());
    assert(!app.savePresetLayouts()); // never overwrite corruption or silently load the older file
    ofxOceanodeShared::preset="";app.onPresetLoaded();
    assert(!app.hasActivePreset&&app.pendingIniLoad.empty()&&app.presetLayouts.entries.empty());
    // One layout per preset, legacy macro files ignored, Save As and global default.
    ofxOceanodeShared::preset="life-root";filesystem::create_directories("life-root");app.onPresetLoaded();
    app.presetLayouts.create("Default",sound);app.presetLayouts.defaultLayout="Default";
    app.onPresetSaved();auto savedRoot=ofBufferFromFile("life-root/ImGuiLayout.ini").getText();
    assert(!savedRoot.empty()&&app.hasActivePreset);
    filesystem::create_directories("life-root/Macro_1");
    ofstream("life-root/Macro_1/ImGuiLayout.ini")<<laser["layout"].get<string>();
    app.beforeFrame();settle();app.onPresetSaved();
    assert(ofBufferFromFile("life-root/ImGuiLayout.ini").getText()==savedRoot);
    assert(ofBufferFromFile("life-root/Macro_1/ImGuiLayout.ini").getText()==laser["layout"].get<string>());
    frame(true,true);settle();
    app.container->scopeSavePending=true;app.container->pendingScopeSavePath="life-root";
    app.container->pendingScopeSaveDeadlineMillis=450;
    app.container->flushPendingScopeSave();
    assert(app.container->lastScopeSave=="life-root"&&!app.container->scopeSavePending);
    assert(app.container->pendingScopeSavePath.empty()&&app.container->pendingScopeSaveDeadlineMillis==0);
    assert(ofBufferFromFile("life-root/ImGuiLayout.ini").getText()!=savedRoot);
    assert(ofBufferFromFile("life-root/Macro_1/ImGuiLayout.ini").getText()==laser["layout"].get<string>());
    app.onPresetSaved();
    auto currentLayout=ofBufferFromFile("life-root/ImGuiLayout.ini").getText();
    assert(currentLayout!=savedRoot);
    filesystem::create_directories("life-save-as");ofxOceanodeShared::preset="life-save-as";app.onPresetSaved();
    assert(ofBufferFromFile("life-save-as/ImGuiLayout.ini").getText()==currentLayout);
    assert(!filesystem::exists("life-save-as/Macro_1/ImGuiLayout.ini"));
    assert(ofLoadJson("life-save-as/preset_layouts.json")["default"]=="Default");
    app.onPresetLoaded();assert(app.pendingPresetLayout=="Default");app.beforeFrame();settle();
    assert(app.presetLayouts.active=="Default"&&app.pendingIniLoad.empty());
    app.saveCurrentPresetLayout();
    filesystem::create_directories("config/guiLayouts");
    ofstream("config/guiLayouts/Startup.ini")<<laser["layout"].get<string>();
    ofstream("config/config.json")<<ofJson({{"defaultLayout","Startup.ini"}}).dump();
    app.loadDefaultGlobalLayout();assert(app.pendingIniLoad=="config/guiLayouts/Startup.ini");
    app.beforeFrame();settle();assert(app.pendingIniLoad.empty());
    // A normal preset save must persist View toggles without a named layout.
    // Missing optional controller entries are visible, just like Controls::draw.
    app.controls->visibility.erase("Timeline");
    assert(app.capturePresetLayout()["controllers"]["Timeline"]==true);
    ofxOceanodeShared::preset="view-hidden";
    filesystem::create_directories("view-hidden");app.onPresetLoaded();
    app.controls->visibility["Timeline"]=false;
    app.container->nodes={&synth,&macroNode};app.container->panels={{"root",{false}}};
    show->parameter.set(false);innerShow->set(false);macro->visible=false;
    macro->child->panels[0].windowState.isOpen=false;
    ofxOceanodeScope::getInstance()->setVisible(false);
    ofxOceanodeNode persistent; persistent.persistent=true;persistent.model->number=99;
    auto persistentShow=make_shared<ofParameter<bool>>("Show",false);persistent.parameters={persistentShow};
    app.container->nodes.push_back(&persistent);
    app.onPresetSaved();
    assert(ofLoadJson("view-hidden/view.json")["controllers"]["Timeline"]==false);
    assert(!ofLoadJson("view-hidden/view.json")["windows"]["nodes"].contains(oceanodeLayouts::nodeKey(*persistent.model)));
    // Fresh manager/controls simulate restarting with Timeline visible by default.
    ofxOceanodeLayoutManager restarted;
    restarted.container=app.container;
    show->parameter.set(true);innerShow->set(true);macro->visible=true;persistentShow->set(true);
    restarted.container->panels[0].windowState.isOpen=true;macro->child->panels[0].windowState.isOpen=true;
    ofxOceanodeScope::getInstance()->setVisible(true);
    assert(restarted.controls->visibility["Timeline"]);
    restarted.onPresetLoaded();restarted.beforeFrame();settle();
    assert(!restarted.controls->visibility["Timeline"]);
    assert(!show->parameter.get()&&!innerShow->get()&&!macro->visible);
    assert(!restarted.container->panels[0].windowState.isOpen&&!macro->child->panels[0].windowState.isOpen);
    assert(!ofxOceanodeScope::getInstance()->isVisible()&&persistentShow->get());
    // Even a legacy named layout must not restore deprecated persistent nodes.
    auto withPersistent=sound;
    withPersistent["windows"]["nodes"][oceanodeLayouts::nodeKey(*persistent.model)]["parameters"]["Show"]=false;
    oceanodeLayouts::restoreWindows(restarted.container.get(),*restarted.controls,withPersistent);
    assert(persistentShow->get());
    // Save As captures the current visible state, independently of the original.
    ofxOceanodeShared::preset="view-shown";filesystem::create_directories("view-shown");
    restarted.controls->visibility.erase("Timeline");restarted.onPresetSaved();
    assert(ofLoadJson("view-shown/view.json")["controllers"]["Timeline"]==true);
    ofxOceanodeShared::preset="view-hidden";restarted.onPresetLoaded();restarted.beforeFrame();
    assert(!restarted.controls->visibility["Timeline"]);
    ofxOceanodeShared::preset="view-shown";restarted.onPresetLoaded();restarted.beforeFrame();
    assert(restarted.controls->visibility["Timeline"]);
    // An older default snapshot must not undo View changes saved with the preset.
    restarted.presetLayouts.create("Visible",sound);restarted.presetLayouts.defaultLayout="Visible";
    show->parameter.set(false);innerShow->set(false);macro->visible=false;
    restarted.container->panels[0].windowState.isOpen=false;macro->child->panels[0].windowState.isOpen=false;
    ofxOceanodeScope::getInstance()->setVisible(false);
    restarted.controls->visibility["Timeline"]=false;restarted.onPresetSaved();
    restarted.controls->visibility["Timeline"]=true;
    restarted.onPresetLoaded();restarted.beforeFrame();settle();
    assert(!restarted.controls->visibility["Timeline"]);
    assert(restarted.presetLayouts.entries.at("Visible")==sound);
    assert(!show->parameter.get()&&!innerShow->get()&&!macro->visible);
    assert(!restarted.container->panels[0].windowState.isOpen&&!macro->child->panels[0].windowState.isOpen);
    assert(!ofxOceanodeScope::getInstance()->isVisible());
    restarted.recallPresetLayout("Visible");assert(restarted.controls->visibility["Timeline"]);
    assert(show->parameter.get()&&innerShow->get()&&macro->visible);
    assert(restarted.container->panels[0].windowState.isOpen&&macro->child->panels[0].windowState.isOpen);
    assert(ofxOceanodeScope::getInstance()->isVisible());
    // Legacy/malformed view files retain current visibility, and a second load
    // (including New) cancels a pending visibility restore from the first.
    ofxOceanodeShared::preset="view-hidden";restarted.onPresetLoaded();
    ofxOceanodeShared::preset="view-legacy";restarted.onPresetLoaded();restarted.beforeFrame();
    assert(restarted.controls->visibility["Timeline"]);
    filesystem::create_directories("view-invalid");
    for(const string invalid : {string("{broken"), string("{\"version\":1,\"controllers\":false}")}){
        ofstream("view-invalid/view.json")<<invalid;
        ofxOceanodeShared::preset="view-invalid";restarted.onPresetLoaded();restarted.beforeFrame();
        assert(restarted.controls->visibility["Timeline"]);
    }
    ofxOceanodeShared::preset="view-hidden";restarted.onPresetLoaded();
    ofxOceanodeShared::preset="";restarted.onPresetLoaded();restarted.beforeFrame();
    assert(restarted.controls->visibility["Timeline"]);
    app.presetLayouts.fromJson(original);
    // Save As is a separate serialized copy, with no shared mutable snapshots.
    auto savedAs=reopened;savedAs.entries.at("Audio")=laser;assert(reopened.entries.at("Audio")==sound);
    savedAs.erase("Audio");assert(savedAs.active.empty()&&savedAs.entries.count("Laser"));
    ofxOceanodePresetLayouts empty;reopened=empty;assert(reopened.active.empty()&&reopened.entries.empty());
    for(int i=0;i<20;++i){app.recallPresetLayout(i%2?"Sound":"Laser");settle();}
    ImGui::DestroyContext();
    puts("PASS: real docking recall, nested window visibility, unaffected patch options, new/deleted nodes, pending layout cancellation, global/preset layout switching, preset defaults, legacy migration, frame boundaries, shared preset/Scope persistence, global startup default, serialization, Save As isolation, rename/delete, duplicate/invalid names, corrupt files, repeated switching.");
}
'''


def main():
    source = (ROOT / 'src/Layouts/ofxOceanodeLayoutManager.h').read_text()
    windows = (ROOT / 'src/Layouts/ofxOceanodeLayoutWindowState.h').read_text()
    production = windows[windows.index('namespace oceanodeLayouts {'):]
    production += block((ROOT / 'src/Managers/ofxOceanodeContainer.cpp').read_text(), 'void ofxOceanodeContainer::flushPendingScopeSave()')
    production += block((ROOT / 'src/ofxOceanode.cpp').read_text(), 'void ofxOceanode::applyGUILayout(').replace('ofxOceanode::', 'ofxOceanodeLayoutManager::')
    production += '\n'.join(block(source, signature) for signature in (
        'ofJson ofxOceanodeLayoutManager::capturePresetLayout()',
        'void ofxOceanodeLayoutManager::recallPresetLayout(', 'bool ofxOceanodeLayoutManager::savePresetLayouts()',
        'void ofxOceanodeLayoutManager::readPresetLayouts()', 'void ofxOceanodeLayoutManager::queueGlobalLayout(',
        'void ofxOceanodeLayoutManager::onPresetLoaded()', 'void ofxOceanodeLayoutManager::onPresetSaved()',
        'void ofxOceanodeLayoutManager::beforeFrame()', 'void ofxOceanodeLayoutManager::saveCurrentPresetLayout()',
        'void ofxOceanodeLayoutManager::loadDefaultGlobalLayout()'))
    with tempfile.TemporaryDirectory(prefix='oceanode-presetLayouts-') as work:
        work = Path(work)
        (work / 'imconfig.h').write_text('// Standalone ImGui.\n')
        cpp = work / 'test.cpp'
        cpp.write_text(FIXTURE.replace('PRODUCTION', production))
        binary = work / 'test'
        subprocess.run([os.environ.get('CXX', 'clang++'), '-std=c++17', '-O0', '-g',
                        '-fsanitize=address,undefined', '-I', str(IMGUI), '-I', str(work),
                        '-I', str(JSON), '-I', str(ROOT / 'src'), str(cpp),
                        *[str(IMGUI / name) for name in
                          ('imgui.cpp', 'imgui_draw.cpp', 'imgui_tables.cpp', 'imgui_widgets.cpp')],
                        '-o', str(binary)], check=True)
        subprocess.run([str(binary)], cwd=work, check=True)


if __name__ == '__main__':
    main()
