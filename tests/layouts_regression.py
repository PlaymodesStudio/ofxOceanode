#!/usr/bin/env python3
"""Exercise production layout capture/recall with real ImGui docking, ASan/UBSan.

Compiles the real ofxOceanodeLayouts.cpp and ofxOceanodeWindowManager.cpp against
isolated graph/OF types (shared with window_manager_regression.py); window
toggles, nested traversal, ini application, layout.json persistence and the
Container's docking-save notification are production code. Does not launch a
graphics host.
"""
from pathlib import Path
import os
import subprocess
import tempfile
from new_preset_regression import block
from window_manager_regression import MOCKS as WINDOW_MOCKS

ROOT = Path(__file__).resolve().parents[1]
IMGUI = Path(os.environ.get('OCEANODE_TEST_IMGUI_SRC', str(ROOT.parent / 'ofxImGui/libs/imgui/src')))
JSON = ROOT.parents[1] / 'libs/json/include'

MOCKS = dict(WINDOW_MOCKS)
MOCKS['ofEvents.h'] = r'''
#pragma once
#include <functional>
#include <memory>
#include <vector>
struct ofEventListener { std::shared_ptr<void> token; };
template<class T> struct ofEvent {
    std::vector<std::weak_ptr<std::function<void(T&)>>> listeners;
    ofEventListener newListener(std::function<void(T&)> f){
        auto p = std::make_shared<std::function<void(T&)>>(std::move(f)); listeners.push_back(p); return {p};
    }
    void notify(T& value){ for(auto& w : listeners) if(auto f = w.lock()) (*f)(value); }
};
'''
MOCKS['ofMain.h'] = r'''
#pragma once
#include "mock_of.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
inline std::string ofToDataPath(const std::string& path, bool = false){ return path; }
struct ofFile {
    std::string path; ofFile(std::string p = ""):path(p){}
    bool exists() const { return std::filesystem::exists(path); }
    std::string getBaseName() const { return std::filesystem::path(path).stem().string(); }
};
struct ofBuffer {
    std::string text; ofBuffer() = default; ofBuffer(const char* data, size_t size):text(data, size){}
    std::string getText() const { return text; }
};
inline ofBuffer ofBufferFromFile(const std::string& path){
    std::ifstream in(path); std::string text((std::istreambuf_iterator<char>(in)), {}); return ofBuffer(text.data(), text.size());
}
inline bool ofBufferToFile(const std::string& path, const ofBuffer& buffer){ std::ofstream out(path); out << buffer.text; return bool(out); }
inline ofJson ofLoadJson(const std::string& path){
    std::ifstream in(path); auto json = ofJson::parse(in, nullptr, false); return json.is_discarded() ? ofJson() : json;
}
inline bool ofSavePrettyJson(const std::string& path, const ofJson& json){ std::ofstream out(path); out << json.dump(4); return bool(out); }
struct ofDirectory {
    std::string path, extension; std::vector<ofFile> files;
    ofDirectory(std::string p):path(p){}
    void allowExt(const std::string& e){ extension = "." + e; }
    bool exists() const { return std::filesystem::is_directory(path); }
    void listDir(){
        files.clear();
        for(auto& entry : std::filesystem::directory_iterator(path))
            if(entry.path().extension() == extension) files.push_back(ofFile(entry.path().string()));
        std::sort(files.begin(), files.end(), [](const ofFile& a, const ofFile& b){ return a.path < b.path; });
    }
    size_t size() const { return files.size(); }
    ofFile getFile(int i) const { return files[i]; }
    static bool createDirectory(const std::string& p, bool, bool){ std::filesystem::create_directories(p); return true; }
};
struct Logger { template<class T> Logger& operator<<(const T&){ return *this; } };
inline Logger ofLogError(const std::string&){ return {}; }
'''
MOCKS['PresetSaveCleanup.h'] = r'''
#pragma once
#include "ofMain.h"
namespace oceanodePresetSave {
inline bool failWrite = false;
inline bool saveJson(const std::string& path, const ofJson& json){
    if(failWrite) return false;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream out(path); out << json.dump(); return bool(out);
}
}
'''
MOCKS['ofxOceanodeShared.h'] = WINDOW_MOCKS['ofxOceanodeShared.h'].replace(
    '#include <string>\n', '#include <string>\n#include "ofEvents.h"\n').replace(
    '    static const std::string& getActiveCanvasUniqueID(){ return active; }\n',
    '    static const std::string& getActiveCanvasUniqueID(){ return active; }\n'
    '    inline static std::string preset;\n'
    '    static std::string getCurrentPresetPath(){ return preset; }\n'
    '    static ofEvent<const std::string>& getDockingSaveRequestEvent(){ static ofEvent<const std::string> e; return e; }\n')
# The scope autosave transaction of Container, with its production flush below.
MOCKS['ofxOceanodeContainer.h'] = WINDOW_MOCKS['ofxOceanodeContainer.h'].replace(
    '    std::vector<ofxOceanodeNode*> nodes; Panels panels; bool hasPanels=true;\n',
    '    std::vector<ofxOceanodeNode*> nodes; Panels panels; bool hasPanels=true;\n'
    '    bool scopeSavePending=false; std::string pendingScopeSavePath, lastScopeSave;\n'
    '    unsigned long long pendingScopeSaveDeadlineMillis=0;\n'
    '    void saveScope(const std::string& path){ lastScopeSave=path; }\n'
    '    void flushPendingScopeSave();\n')

FIXTURE = r'''
#include "ofxOceanodeWindowManager.h"
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeControls.h"
#include "ofxOceanodeNodeMacro.h"
#include "ofxOceanodeScope.h"
#include "ofxOceanodeShared.h"
#include "PresetSaveCleanup.h"
#define private public
#include "ofxOceanodeLayouts.h"
#undef private
#include "imgui.h"
#include "imgui_internal.h"
#include <cassert>
#include <cstdio>
using namespace std;
namespace ImGui { inline void SaveIniSettingsToDisk(const char*){ assert(false && "the Container must not write ini files"); } }
PRODUCTION
struct App {
    shared_ptr<ofxOceanodeContainer> container;
    ofxOceanodeControls controls;
    ofxOceanodeWindowManager windows;
    ofxOceanodeLayouts layouts{windows};
    App(shared_ptr<ofxOceanodeContainer> c = make_shared<ofxOceanodeContainer>()):container(c){ windows.setup(container, controls); }
};
string key(const string& name, int number){ return ofJson::array({name, number}).dump(); }
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
string text(const string& path){return ofBufferFromFile(path).getText();}
int main(){
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;
    io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;io.DisplaySize={1200,800};io.DeltaTime=1.f/60;
    unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    App app;app.layouts.setup();
    auto& layouts=app.layouts;auto& collection=app.layouts.collection;
    ofxOceanodeNode synth,macroNode,inner,newNode;
    auto show=make_shared<ofxOceanodeParameter<bool>>("Show Window",true);
    auto showNames=make_shared<ofParameter<bool>>("Show Names",true);
    auto innerShow=make_shared<ofParameter<bool>>("Show GUI",true);
    synth.parameters.items={show,showNames};inner.inspector.items={innerShow};inner.model->number=3;
    auto macro=make_shared<ofxOceanodeNodeMacro>();macro->name="Macro";macroNode.model=macro;
    macro->child->nodes={&inner};macro->child->panels.open={{"nested",true}};
    app.container->nodes={&synth,&macroNode};app.container->panels.open={{"root",true}};
    frame(true);settle();auto sound=layouts.capturePresetLayout();
    assert(sound["ini"].is_string()&&sound["view"]["controllers"].is_object()&&sound["view"]["windows"].is_object());
    const auto soundDock=ImGui::FindWindowByName("Mixer")->DockId;
    collection.create("Sound",sound);
    show->parameter.set(false);innerShow->set(false);macro->visible=false;showNames->set(false);
    app.controls.visibility["Timeline"]=false;
    app.container->panels.open["root"]=false;macro->child->panels.open["nested"]=false;
    frame(true,true);settle();auto laser=layouts.capturePresetLayout();collection.create("Laser",laser);
    const string laserIni=laser["ini"].get<string>();
    // New graph members must retain their visibility; labels cannot control identity.
    auto newShow=make_shared<ofParameter<bool>>("Show",true);newNode.model->number=2;
    newNode.parameters.items={newShow};app.container->nodes.push_back(&newNode);
    layouts.pendingIni="old preset ini";
    layouts.recallPresetLayout("Sound");settle();
    assert(layouts.pendingIni.empty());
    assert(show->parameter.get()&&innerShow->get()&&macro->visible);
    assert(!showNames->get()&&newShow->get()&&macro->forcedDock==0);
    assert(app.controls.visibility["Timeline"]&&app.container->panels.open["root"]);
    assert(macro->child->panels.open["nested"]);
    assert(ImGui::FindWindowByName("Mixer")->DockId==soundDock);
    layouts.recallPresetLayout("Laser");settle();
    assert(!show->parameter.get()&&!innerShow->get()&&!macro->visible&&!app.controls.visibility["Timeline"]);
    assert(!app.container->panels.open["root"]&&!macro->child->panels.open["nested"]);
    // Removal after capture is safe, including a whole nested macro tree.
    app.container->nodes={&synth};app.container->panels.open.clear();layouts.recallPresetLayout("Sound");settle();
    // Production preset-path save/load, first Save, Save As, missing files, New,
    // corrupt files and write errors. Named snapshots remain explicit.
    auto snapshots=collection.entries;
    filesystem::create_directories("config/guiLayouts");
    ofSavePrettyJson("config/guiLayouts/Studio.json",{{"version",1},{"ini",laserIni},{"view",laser["view"]}});
    layouts.pendingSnapshot="Laser";
    layouts.queueGlobalLayout("Studio");
    assert(collection.entries==snapshots&&collection.active.empty());
    assert(layouts.pendingSnapshot.empty()&&layouts.pendingIni==laserIni&&layouts.pendingView==laser["view"]);
    assert(layouts.globalLayoutName=="Studio");
    layouts.recallPresetLayout("Sound");settle();
    auto original=collection.toJson();
    assert(original["version"]==1&&original["layouts"].size()==2&&!original.contains("workspaces"));
    // An unsaved patch keeps everything in memory until its first Save.
    assert(layouts.writeLayoutFile());assert(!filesystem::exists("layout.json"));
    ofxOceanodeShared::preset="preset-A";assert(layouts.writeLayoutFile());
    auto fileA=ofLoadJson("preset-A/layout.json");
    assert(fileA["version"]==1&&fileA["layouts"]==original["layouts"]&&fileA["active"]=="Sound"&&fileA["default"]=="");
    assert(!fileA["current"]["ini"].get<string>().empty()&&fileA["current"]["view"].is_null());
    layouts.queueGlobalLayout("Studio");
    auto globalSelection=ofLoadJson("preset-A/layout.json");
    assert(globalSelection["active"]==""&&globalSelection["layouts"]==original["layouts"]);
    layouts.recallPresetLayout("Sound");settle();
    assert(ofLoadJson("preset-A/layout.json")["active"]=="Sound");
    ofxOceanodeShared::preset="preset-B";assert(layouts.writeLayoutFile());
    collection.overwrite("Sound",laser);assert(collection.active=="Sound"&&layouts.writeLayoutFile());
    assert(ofLoadJson("preset-A/layout.json")["layouts"]==original["layouts"]);
    ofxOceanodeShared::preset="preset-A";layouts.onPresetLoaded();
    assert(collection.toJson()==original&&layouts.pendingSnapshot=="Sound"&&!layouts.pendingIni.empty());
    // A preset default is independent from the last selection, including after
    // saving, reopening, global-layout selection, rename, deletion and Save As.
    ofxOceanodeShared::preset="preset-default";
    collection.defaultLayout="Laser";
    assert(collection.active=="Sound"&&layouts.writeLayoutFile());
    layouts.onPresetLoaded();
    assert(layouts.pendingSnapshot=="Laser"&&collection.active=="Sound");
    layouts.recallPresetLayout("Laser");settle();
    layouts.recallPresetLayout("Sound");settle();
    assert(collection.defaultLayout=="Laser");
    layouts.onPresetLoaded();assert(layouts.pendingSnapshot=="Laser");
    layouts.queueGlobalLayout("Studio");
    layouts.onPresetLoaded();assert(layouts.pendingSnapshot=="Laser"&&collection.active.empty());
    collection.rename("Laser","Lasers");assert(collection.defaultLayout=="Lasers");
    assert(layouts.writeLayoutFile());layouts.onPresetLoaded();assert(layouts.pendingSnapshot=="Lasers");
    auto defaultSaveAs=collection;
    assert(defaultSaveAs.defaultLayout=="Lasers");
    layouts.recallPresetLayout("Sound");settle();
    collection.erase("Lasers");assert(collection.defaultLayout.empty());
    assert(layouts.writeLayoutFile());layouts.onPresetLoaded();assert(layouts.pendingSnapshot=="Sound");
    auto withoutDefault=original;withoutDefault.erase("default");
    ofxOceanodePresetLayouts reread;reread.fromJson(withoutDefault);
    assert(reread.defaultLayout.empty()&&reread.layoutOnLoad()=="Sound");
    auto missingDefault=original;missingDefault["default"]="Deleted layout";
    reread.fromJson(missingDefault);
    assert(reread.defaultLayout.empty()&&reread.layoutOnLoad()=="Sound");
    ofxOceanodeShared::preset="old-preset-without-layouts";layouts.onPresetLoaded();
    assert(collection.entries.empty()&&layouts.pendingSnapshot.empty()&&layouts.pendingIni.empty());
    ofxOceanodeShared::preset="preset-A";layouts.onPresetLoaded();
    oceanodePresetSave::failWrite=true;assert(!layouts.writeLayoutFile());
    assert(!layouts.layoutError.empty());oceanodePresetSave::failWrite=false;
    // An invalid layout.json is reported, never replaced, and blocks every write
    // until a preset is loaded again.
    const ofJson validFile=ofLoadJson("preset-A/layout.json");
    vector<string> invalidFiles{"{broken","[]"};
    auto variant=[&](auto edit){ofJson file=validFile;edit(file);return file.dump();};
    invalidFiles.push_back(variant([](ofJson& f){f["version"]=2;}));
    invalidFiles.push_back(variant([](ofJson& f){f["layouts"]=ofJson::array();}));
    invalidFiles.push_back(variant([](ofJson& f){f["layouts"]["Sound"]["ini"]="";}));
    invalidFiles.push_back(variant([](ofJson& f){f["layouts"]["Sound"]["view"].erase("windows");}));
    invalidFiles.push_back(variant([](ofJson& f){f["layouts"]["Bad##name"]=f["layouts"]["Sound"];}));
    invalidFiles.push_back(variant([](ofJson& f){f["current"]=false;}));
    invalidFiles.push_back(variant([](ofJson& f){f["current"]["ini"]=3;}));
    invalidFiles.push_back(variant([](ofJson& f){f["current"]["view"]=ofJson::object({{"controllers",false}});}));
    for(const auto& invalid:invalidFiles){
        ofstream("preset-B/layout.json")<<invalid;
        ofxOceanodeShared::preset="preset-B";layouts.onPresetLoaded();
        assert(layouts.readFailed&&collection.entries.empty()&&!layouts.layoutError.empty());
        assert(layouts.pendingIni.empty()&&layouts.pendingSnapshot.empty()&&layouts.pendingView.is_null());
        assert(!layouts.writeLayoutFile());
        layouts.onPresetSaved();layouts.saveDocking("preset-B");
        string path="preset-B";ofxOceanodeShared::getDockingSaveRequestEvent().notify(path);
        layouts.queueGlobalLayout("Studio");
        assert(text("preset-B/layout.json")==invalid);
    }
    ofxOceanodeShared::preset="";layouts.onPresetLoaded();
    assert(collection.entries.empty()&&layouts.layoutError.empty()&&!layouts.readFailed);
    collection.fromJson(original);
    auto disk=ofJson::parse(collection.toJson().dump());ofxOceanodePresetLayouts reopened;reopened.fromJson(disk);
    assert(reopened.entries==collection.entries&&reopened.active=="Sound");
    reopened.rename("Sound","Audio");assert(reopened.active=="Audio"&&reopened.entries.at("Audio")==sound);
    rejects([&]{reopened.create("Audio",laser);});rejects([&]{reopened.rename("Audio","Laser");});
    rejects([&]{reopened.create("   ",laser);});rejects([&]{reopened.create("Name##id",laser);});
    assert(ofxOceanodePresetLayouts::cleanName("  Sound  ")=="Sound");
    auto before=reopened.toJson();auto bad=before;bad["version"]=99;
    rejects([&]{reopened.fromJson(bad);});assert(before==reopened.toJson());
    bad=before;bad["layouts"]["Audio"]["ini"]=false;
    rejects([&]{reopened.fromJson(bad);});assert(before==reopened.toJson());
    bad=before;bad["version"]=2;bad["workspaces"]=bad["layouts"];bad.erase("layouts"); // no runtime legacy reader
    rejects([&]{reopened.fromJson(bad);});assert(before==reopened.toJson());
    // One layout file per preset: macro folders are ignored, Save writes the live
    // docking, the Container's docking-save request writes current.ini and keeps
    // current.view, and Save As carries everything into the new preset.
    ofxOceanodeShared::preset="life-root";filesystem::create_directories("life-root");layouts.onPresetLoaded();
    collection.create("Default",sound);collection.defaultLayout="Default";
    layouts.onPresetSaved();auto savedRoot=ofLoadJson("life-root/layout.json")["current"];
    assert(!savedRoot["ini"].get<string>().empty()&&savedRoot["view"].is_object());
    filesystem::create_directories("life-root/Macro_1");
    ofstream("life-root/Macro_1/layout.json")<<laser.dump();
    layouts.beforeFrame();settle();layouts.onPresetSaved();
    assert(ofLoadJson("life-root/layout.json")["current"]==savedRoot);
    assert(ofLoadJson("life-root/Macro_1/layout.json")==laser);
    frame(true,true);settle();
    show->parameter.set(!show->parameter.get());
    app.container->scopeSavePending=true;app.container->pendingScopeSavePath="life-root";
    app.container->pendingScopeSaveDeadlineMillis=450;
    app.container->flushPendingScopeSave();
    assert(app.container->lastScopeSave=="life-root"&&!app.container->scopeSavePending);
    assert(app.container->pendingScopeSavePath.empty()&&app.container->pendingScopeSaveDeadlineMillis==0);
    auto docked=ofLoadJson("life-root/layout.json");
    assert(docked["current"]["ini"]!=savedRoot["ini"]&&docked["current"]["view"]==savedRoot["view"]);
    assert(docked["layouts"]["Default"]==sound&&docked["default"]=="Default");
    assert(ofLoadJson("life-root/Macro_1/layout.json")==laser);
    // Requests for another preset (a stale autosave) and saveDocking with the current
    // path behave the same way: only the current preset's file is written.
    filesystem::create_directories("elsewhere");
    string elsewhere="elsewhere";ofxOceanodeShared::getDockingSaveRequestEvent().notify(elsewhere);
    layouts.saveDocking("elsewhere");assert(!filesystem::exists("elsewhere/layout.json"));
    layouts.saveDocking("life-root");assert(ofLoadJson("life-root/layout.json")["current"]["view"]==savedRoot["view"]);
    layouts.onPresetSaved();
    auto currentLayout=ofLoadJson("life-root/layout.json")["current"];
    assert(currentLayout["ini"]!=savedRoot["ini"]&&currentLayout["view"]!=savedRoot["view"]);
    filesystem::create_directories("life-save-as");ofxOceanodeShared::preset="life-save-as";layouts.onPresetSaved();
    auto savedAs=ofLoadJson("life-save-as/layout.json");
    assert(savedAs["current"]==currentLayout&&savedAs["default"]=="Default"&&savedAs["layouts"]["Default"]==sound);
    assert(!filesystem::exists("life-save-as/Macro_1"));
    layouts.onPresetLoaded();assert(layouts.pendingSnapshot=="Default");layouts.beforeFrame();settle();
    assert(collection.active=="Default"&&layouts.pendingIni.empty());
    layouts.saveDocking("life-save-as");assert(ofLoadJson("life-save-as/layout.json")["active"]=="Default");
    // Global layouts: {version, ini, view} files named without extension in menus,
    // a separate default file, and the startup default with its window visibility.
    ofSavePrettyJson("config/guiLayouts.json",{{"version",1},{"default","Startup"}});
    {
        App fresh;fresh.layouts.setup();
        assert(fresh.layouts.pendingIni.empty()&&fresh.layouts.globalDefault=="Startup"); // Startup.json is missing
        assert(!fresh.layouts.globalError.empty());
        ofSavePrettyJson("config/guiLayouts/Startup.json",{{"version",1},{"ini",laserIni},{"view",laser["view"]}});
        App restartedWithDefault;restartedWithDefault.layouts.setup();
        assert(restartedWithDefault.layouts.pendingIni==laserIni&&restartedWithDefault.layouts.globalLayoutName=="Startup");
        assert(restartedWithDefault.controls.visibility["Timeline"]);
        restartedWithDefault.layouts.beforeFrame();settle();assert(restartedWithDefault.layouts.pendingIni.empty());
        assert(!restartedWithDefault.controls.visibility["Timeline"]&&restartedWithDefault.layouts.pendingView.is_null());
        // New preset starts again from the default global layout, docking and visibility.
        restartedWithDefault.controls.visibility["Timeline"]=true;
        ofxOceanodeShared::preset="";restartedWithDefault.layouts.onPresetLoaded();
        assert(restartedWithDefault.layouts.pendingIni==laserIni&&restartedWithDefault.layouts.pendingView==laser["view"]);
        restartedWithDefault.layouts.beforeFrame();settle();
        assert(!restartedWithDefault.controls.visibility["Timeline"]);
        // A preset with its own layout file wins over the default.
        ofxOceanodeShared::preset="preset-A";restartedWithDefault.layouts.onPresetLoaded();
        assert(restartedWithDefault.layouts.pendingIni!=laserIni&&restartedWithDefault.layouts.pendingView.is_null());
        ofxOceanodeShared::preset="";
        // A docking-only layout leaves visibility alone; invalid and old .ini files are not loaded.
        ofSavePrettyJson("config/guiLayouts/Docking.json",{{"version",1},{"ini",laserIni}});
        restartedWithDefault.layouts.queueGlobalLayout("Docking");
        assert(restartedWithDefault.layouts.pendingIni==laserIni&&restartedWithDefault.layouts.pendingView.is_null());
        restartedWithDefault.layouts.beforeFrame();
        for(const string invalid:{string("{broken"),ofJson({{"version",2},{"ini",laserIni}}).dump(),
                                  ofJson({{"version",1},{"ini",""}}).dump(),
                                  ofJson({{"version",1},{"ini",laserIni},{"view",{{"controllers",false}}}}).dump()}){
            ofstream("config/guiLayouts/Broken.json")<<invalid;
            restartedWithDefault.layouts.queueGlobalLayout("Broken");
            assert(!restartedWithDefault.layouts.globalError.empty()&&restartedWithDefault.layouts.pendingIni.empty());
            assert(restartedWithDefault.layouts.globalLayoutName=="Docking");
        }
        ofBufferToFile("config/guiLayouts/Legacy.ini",ofBuffer(laserIni.data(),laserIni.size()));
        restartedWithDefault.layouts.queueGlobalLayout("Legacy");assert(restartedWithDefault.layouts.pendingIni.empty());
        restartedWithDefault.layouts.queueGlobalLayout("Startup");assert(restartedWithDefault.layouts.globalError.empty());
        restartedWithDefault.layouts.beforeFrame();
        ofSavePrettyJson("config/guiLayouts.json",{{"default","Startup"}});
        App unversioned;unversioned.layouts.setup();assert(unversioned.layouts.pendingIni.empty()&&unversioned.layouts.globalDefault.empty());
        restartedWithDefault.layouts.setGlobalDefault("Studio");
        assert(ofLoadJson("config/guiLayouts.json")==ofJson({{"version",1},{"default","Studio"}}));
        for(const char* name:{"Studio","My layout 2"}) assert(ofxOceanodeLayouts::validGlobalName(name));
        for(const char* name:{"a/b","a\\b","a:b"," a","","x##y"}) assert(!ofxOceanodeLayouts::validGlobalName(name));
    }
    // A normal preset save must persist window visibility without a named layout.
    // Missing optional controller entries are visible, just like Controls::draw.
    app.controls.visibility.erase("Timeline");
    assert(layouts.capturePresetLayout()["view"]["controllers"]["Timeline"]==true);
    ofxOceanodeShared::preset="view-hidden";
    filesystem::create_directories("view-hidden");layouts.onPresetLoaded();
    app.controls.visibility["Timeline"]=false;
    app.container->nodes={&synth,&macroNode};app.container->panels.open={{"root",false}};
    show->parameter.set(false);innerShow->set(false);macro->visible=false;
    macro->child->panels.open["nested"]=false;
    ofxOceanodeScope::getInstance()->setVisible(false);
    ofxOceanodeNode persistent; persistent.persistent=true;persistent.model->number=99;
    auto persistentShow=make_shared<ofParameter<bool>>("Show",false);persistent.parameters.items={persistentShow};
    app.container->nodes.push_back(&persistent);
    layouts.onPresetSaved();
    assert(ofLoadJson("view-hidden/layout.json")["current"]["view"]["controllers"]["Timeline"]==false);
    assert(!ofLoadJson("view-hidden/layout.json")["current"]["view"]["windows"]["nodes"].contains(key("Synth",99)));
    // A fresh manager/controls simulate restarting with Timeline visible by default.
    App restarted(app.container);
    show->parameter.set(true);innerShow->set(true);macro->visible=true;persistentShow->set(true);
    restarted.container->panels.open["root"]=true;macro->child->panels.open["nested"]=true;
    ofxOceanodeScope::getInstance()->setVisible(true);
    assert(restarted.controls.visibility["Timeline"]);
    restarted.layouts.onPresetLoaded();restarted.layouts.beforeFrame();settle();
    assert(!restarted.controls.visibility["Timeline"]);
    assert(!show->parameter.get()&&!innerShow->get()&&!macro->visible);
    assert(!restarted.container->panels.open["root"]&&!macro->child->panels.open["nested"]);
    assert(!ofxOceanodeScope::getInstance()->isVisible()&&persistentShow->get());
    // Even a named layout must not restore deprecated persistent nodes.
    auto withPersistent=sound;
    withPersistent["view"]["windows"]["nodes"][key("Synth",99)]["parameters"]["Show"]=false;
    restarted.windows.restoreView(withPersistent["view"]);
    assert(persistentShow->get());
    // Save As captures the current visible state, independently of the original.
    ofxOceanodeShared::preset="view-shown";filesystem::create_directories("view-shown");
    restarted.controls.visibility.erase("Timeline");restarted.layouts.onPresetSaved();
    assert(ofLoadJson("view-shown/layout.json")["current"]["view"]["controllers"]["Timeline"]==true);
    ofxOceanodeShared::preset="view-hidden";restarted.layouts.onPresetLoaded();restarted.layouts.beforeFrame();
    assert(!restarted.controls.visibility["Timeline"]);
    ofxOceanodeShared::preset="view-shown";restarted.layouts.onPresetLoaded();restarted.layouts.beforeFrame();
    assert(restarted.controls.visibility["Timeline"]);
    // An older default snapshot must not undo visibility saved with the preset.
    restarted.layouts.collection.create("Visible",sound);restarted.layouts.collection.defaultLayout="Visible";
    show->parameter.set(false);innerShow->set(false);macro->visible=false;
    restarted.container->panels.open["root"]=false;macro->child->panels.open["nested"]=false;
    ofxOceanodeScope::getInstance()->setVisible(false);
    restarted.controls.visibility["Timeline"]=false;restarted.layouts.onPresetSaved();
    restarted.controls.visibility["Timeline"]=true;
    restarted.layouts.onPresetLoaded();restarted.layouts.beforeFrame();settle();
    assert(!restarted.controls.visibility["Timeline"]);
    assert(restarted.layouts.collection.entries.at("Visible")==sound);
    assert(!show->parameter.get()&&!innerShow->get()&&!macro->visible);
    assert(!restarted.container->panels.open["root"]&&!macro->child->panels.open["nested"]);
    assert(!ofxOceanodeScope::getInstance()->isVisible());
    restarted.layouts.recallPresetLayout("Visible");assert(restarted.controls.visibility["Timeline"]);
    assert(show->parameter.get()&&innerShow->get()&&macro->visible);
    assert(restarted.container->panels.open["root"]&&macro->child->panels.open["nested"]);
    assert(ofxOceanodeScope::getInstance()->isVisible());
    // Presets without a layout file or with an invalid one keep current visibility,
    // and a second load (including New) cancels a pending restore from the first.
    ofxOceanodeShared::preset="view-hidden";restarted.layouts.onPresetLoaded();
    ofxOceanodeShared::preset="view-without-layout";restarted.layouts.onPresetLoaded();restarted.layouts.beforeFrame();
    assert(restarted.controls.visibility["Timeline"]);
    filesystem::create_directories("view-invalid");
    for(const string invalid : {string("{broken"), string("{\"version\":1,\"layouts\":{},\"current\":{\"view\":false}}")}){
        ofstream("view-invalid/layout.json")<<invalid;
        ofxOceanodeShared::preset="view-invalid";restarted.layouts.onPresetLoaded();restarted.layouts.beforeFrame();
        assert(restarted.controls.visibility["Timeline"]&&restarted.layouts.readFailed);
    }
    ofxOceanodeShared::preset="view-hidden";restarted.layouts.onPresetLoaded();
    ofxOceanodeShared::preset="";restarted.layouts.onPresetLoaded();restarted.layouts.beforeFrame();
    assert(restarted.controls.visibility["Timeline"]);
    collection.fromJson(original);
    // Save As is a separate serialized copy, with no shared mutable snapshots.
    auto copy=reopened;copy.entries.at("Audio")=laser;assert(reopened.entries.at("Audio")==sound);
    copy.erase("Audio");assert(copy.active.empty()&&copy.entries.count("Laser"));
    ofxOceanodePresetLayouts empty;reopened=empty;assert(reopened.active.empty()&&reopened.entries.empty());
    for(int i=0;i<20;++i){layouts.recallPresetLayout(i%2?"Sound":"Laser");settle();}
    ImGui::DestroyContext();
    puts("PASS: real docking recall, nested window visibility, unaffected patch options, new/deleted nodes, pending layout cancellation, global/preset layout switching, preset defaults, frame boundaries, layout.json persistence, docking-save requests keep current.view, invalid files block every write, global layout names/default/startup, serialization, Save As isolation, rename/delete, duplicate/invalid names, repeated switching.");
}
'''


def main():
    container = (ROOT / 'src/Managers/ofxOceanodeContainer.cpp').read_text()
    production = block(container, 'void ofxOceanodeContainer::flushPendingScopeSave()')
    with tempfile.TemporaryDirectory(prefix='oceanode-layouts-') as work:
        work = Path(work)
        (work / 'imconfig.h').write_text('// Standalone ImGui.\n')
        for name, content in MOCKS.items():
            (work / name).write_text(content)
        (work / 'ofxOceanodeCustomGuiExtension.h').write_text(
            (ROOT / 'src/Managers/ofxOceanodeCustomGuiExtension.h').read_text())
        cpp = work / 'test.cpp'
        cpp.write_text(FIXTURE.replace('PRODUCTION', production))
        gui = ROOT / 'src/Gui'
        binary = work / 'test'
        subprocess.run([os.environ.get('CXX', 'clang++'), '-std=c++17', '-O0', '-g',
                        '-fsanitize=address,undefined', '-I', str(work), '-I', str(IMGUI),
                        '-I', str(JSON), '-I', str(ROOT / 'src'), '-I', str(gui), str(cpp),
                        *[str(gui / name) for name in ('ofxOceanodeLayouts.cpp', 'ofxOceanodeWindowManager.cpp',
                                                        'ofxOceanodeProportionalDocking.cpp', 'ofxOceanodeDockCollapse.cpp',
                                                        'ofxOceanodeFloatingWindowDecor.cpp')],
                        *[str(IMGUI / name) for name in
                          ('imgui.cpp', 'imgui_draw.cpp', 'imgui_tables.cpp', 'imgui_widgets.cpp')],
                        '-o', str(binary)], check=True)
        subprocess.run([str(binary)], cwd=work, check=True)


if __name__ == '__main__':
    main()
