#!/usr/bin/env python3
"""Exercise production workspace capture/recall with real ImGui docking, ASan/UBSan.

Graph/OF types are isolated; window toggles, nested traversal, layout application,
and workspace operations are production code. Does not launch a graphics host.
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
#include "ofxOceanodeWorkspaces.h"
#include "ofxOceanodeWindowVisibility.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cassert>
#include <cstdio>
#include <fstream>
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
    shared_ptr<ofxOceanodeNodeModel> model=make_shared<ofxOceanodeNodeModel>();
    ofParameterGroup parameters,inspector;
    auto& getNodeModel(){return *model;}
    auto& getParameters(){return parameters;}auto& getInspectorParameters(){return inspector;}
};
struct WindowState {bool isOpen=false;};
struct Panel {string id;WindowState windowState;};
struct ofxOceanodeContainer {
    vector<ofxOceanodeNode*> nodes; vector<Panel> panels;
    auto getAllModules(){return nodes;} auto& getCustomGuiPanelsData(){return panels;}
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
    static string& getPendingLayoutSavePath(){static string s;return s;}
    static string& getPendingLayoutLoadPath(){static string s;return s;}
    static string& getActiveCanvasLayoutPath(){static string s;return s;}
    static bool& getWorkspaceLayoutActive(){static bool b=false;return b;}
    static bool getGuiLayoutChangesWithMacros(){return true;}
    WORKSPACE_SWITCH_POLICY
    static void setLeftNodeID(ImGuiID){} static void setCentralNodeID(ImGuiID){}
};
struct Policy {void reset(){}};
vector<string> ofSplitString(const string& text,const string&,bool,bool){
    vector<string> out;istringstream s(text);string line;while(getline(s,line))out.push_back(line);return out;
}
string ofTrim(string s){auto a=s.find_first_not_of(" \r\t");if(a==string::npos)return {};return s.substr(a,s.find_last_not_of(" \r\t")-a+1);}
string ofToDataPath(const string& path){return path;}
struct ofFile {string path;ofFile(string p):path(p){}bool exists(){return filesystem::exists(path);}};
ofJson ofLoadJson(const string& path){ifstream input(path);return ofJson::parse(input);}
struct Logger {template<class T>Logger& operator<<(const T&){return *this;}};
Logger ofLogError(const string&){return {};}
namespace oceanodePresetSave {
    bool failWrite=false;
    bool saveJson(const string& path,const ofJson& json){
        if(failWrite)return false;
        filesystem::create_directories(filesystem::path(path).parent_path());
        ofstream output(path);output<<json.dump();return bool(output);
    }
}
struct ofxOceanode {
    shared_ptr<ofxOceanodeContainer> container=make_shared<ofxOceanodeContainer>();
    shared_ptr<Controls> controls=make_shared<Controls>();
    unique_ptr<Policy> proportionalDocking=make_unique<Policy>();
    ofxOceanodeWorkspaces workspaces;string pendingIniLoad,rootLayoutPath="preset/ImGuiLayout.ini",loadedGUILayoutData;
    ImGuiID loadedGUILayoutLeftDockId=0;bool pendingControllerDockReconciliation=false;
    ofJson captureWorkspace();void recallWorkspace(const string&);void applyGUILayout(const string&);
    enum class WorkspacePopup {None,Save,Rename,Delete};
    WorkspacePopup workspacePopup=WorkspacePopup::None;
    string pendingWorkspace,workspaceError;bool workspaceReadFailed=false;
    bool saveWorkspaces();void loadWorkspaces();
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
    ofxOceanode app;
    ofxOceanodeNode synth,macroNode,inner,newNode;
    auto show=make_shared<ofxOceanodeParameter<bool>>("Show Window",true);
    auto showNames=make_shared<ofParameter<bool>>("Show Names",true);
    auto innerShow=make_shared<ofParameter<bool>>("Show GUI",true);
    synth.parameters={show,showNames};inner.inspector={innerShow};
    auto macro=make_shared<ofxOceanodeNodeMacro>();macro->name="Macro";macroNode.model=macro;
    macro->child->nodes={&inner};macro->child->panels={{"nested",{true}}};
    app.container->nodes={&synth,&macroNode};app.container->panels={{"root",{true}}};
    frame(true);settle();auto sound=app.captureWorkspace();
    const auto soundDock=ImGui::FindWindowByName("Mixer")->DockId;
    app.workspaces.create("Sound",sound);
    show->parameter.set(false);innerShow->set(false);macro->visible=false;showNames->set(false);
    app.controls->visibility["Timeline"]=false;
    app.container->panels[0].windowState.isOpen=false;macro->child->panels[0].windowState.isOpen=false;
    frame(true,true);settle();auto laser=app.captureWorkspace();app.workspaces.create("Laser",laser);
    // New graph members must retain their visibility; labels cannot control identity.
    auto newShow=make_shared<ofParameter<bool>>("Show",true);newNode.model->number=2;
    newNode.parameters={newShow};app.container->nodes.push_back(&newNode);
    ofxOceanodeShared::getPendingLayoutLoadPath()="old-macro.ini";
    ofxOceanodeShared::getPendingLayoutSavePath()="old.ini";
    app.pendingIniLoad="old-preset.ini";
    app.recallWorkspace("Sound");settle();
    assert(show->parameter.get()&&innerShow->get()&&macro->visible);
    assert(!showNames->get()&&newShow->get()&&macro->forcedDock==0);
    assert(app.controls->visibility["Timeline"]&&app.container->panels[0].windowState.isOpen);
    assert(macro->child->panels[0].windowState.isOpen);
    assert(ImGui::FindWindowByName("Mixer")->DockId==soundDock);
    assert(ofxOceanodeShared::getPendingLayoutLoadPath().empty()&&ofxOceanodeShared::getPendingLayoutSavePath().empty());
    assert(app.pendingIniLoad.empty()&&ofxOceanodeShared::getWorkspaceLayoutActive());
    assert(!ofxOceanodeShared::shouldSwitchCanvasLayout());
    assert(ofxOceanodeShared::getActiveCanvasLayoutPath()==app.rootLayoutPath);
    app.recallWorkspace("Laser");settle();
    assert(!show->parameter.get()&&!innerShow->get()&&!macro->visible&&!app.controls->visibility["Timeline"]);
    assert(!app.container->panels[0].windowState.isOpen&&!macro->child->panels[0].windowState.isOpen);
    // Removal after capture is safe, including a whole nested macro tree.
    app.container->nodes={&synth};app.container->panels.clear();app.recallWorkspace("Sound");settle();
    // Production preset-path save/load, first Save, Save As, missing files, New,
    // corrupt files and write errors. Named snapshots remain explicit.
    auto original=app.workspaces.toJson();
    assert(app.saveWorkspaces());assert(!filesystem::exists("workspaces.json"));
    ofxOceanodeShared::preset="preset-A";assert(app.saveWorkspaces());
    assert(ofLoadJson("preset-A/workspaces.json")==original);
    ofxOceanodeShared::preset="preset-B";assert(app.saveWorkspaces());
    app.workspaces.entries.at("Sound")=laser;assert(app.saveWorkspaces());
    assert(ofLoadJson("preset-A/workspaces.json")==original);
    ofxOceanodeShared::preset="preset-A";app.loadWorkspaces();
    assert(app.workspaces.toJson()==original&&app.pendingWorkspace=="Sound");
    assert(!ofxOceanodeShared::getWorkspaceLayoutActive());
    assert(ofxOceanodeShared::shouldSwitchCanvasLayout());
    ofxOceanodeShared::preset="old-preset-without-workspaces";app.loadWorkspaces();
    assert(app.workspaces.entries.empty()&&app.pendingWorkspace.empty());
    ofxOceanodeShared::preset="preset-A";app.loadWorkspaces();
    oceanodePresetSave::failWrite=true;assert(!app.saveWorkspaces());
    assert(!app.workspaceError.empty());oceanodePresetSave::failWrite=false;
    ofstream("preset-B/workspaces.json")<<"{broken";
    ofxOceanodeShared::preset="preset-B";app.loadWorkspaces();
    assert(app.workspaces.entries.empty()&&!app.workspaceError.empty());
    assert(!app.saveWorkspaces());
    ifstream corrupt("preset-B/workspaces.json");string content;getline(corrupt,content);assert(content=="{broken");
    ofxOceanodeShared::preset="";app.loadWorkspaces();
    assert(app.workspaces.entries.empty()&&app.workspaceError.empty());
    app.workspaces.fromJson(original);
    auto disk=ofJson::parse(app.workspaces.toJson().dump());ofxOceanodeWorkspaces reopened;reopened.fromJson(disk);
    assert(reopened.entries==app.workspaces.entries&&reopened.active=="Sound");
    reopened.rename("Sound","Audio");assert(reopened.active=="Audio"&&reopened.entries.at("Audio")==sound);
    rejects([&]{reopened.create("Audio",laser);});rejects([&]{reopened.rename("Audio","Laser");});
    rejects([&]{reopened.create("   ",laser);});rejects([&]{reopened.create("Name##id",laser);});
    assert(ofxOceanodeWorkspaces::cleanName("  Sound  ")=="Sound");
    auto before=reopened.toJson();auto bad=before;bad["version"]=99;
    rejects([&]{reopened.fromJson(bad);});assert(before==reopened.toJson());
    bad=before;bad["workspaces"]["Audio"]["layout"]=false;
    rejects([&]{reopened.fromJson(bad);});assert(before==reopened.toJson());
    // Save As is a separate serialized copy, with no shared mutable snapshots.
    auto savedAs=reopened;savedAs.entries.at("Audio")=laser;assert(reopened.entries.at("Audio")==sound);
    savedAs.erase("Audio");assert(savedAs.active.empty()&&savedAs.entries.count("Laser"));
    ofxOceanodeWorkspaces empty;reopened=empty;assert(reopened.active.empty()&&reopened.entries.empty());
    for(int i=0;i<20;++i){app.recallWorkspace(i%2?"Sound":"Laser");settle();}
    ImGui::DestroyContext();
    puts("PASS: real docking recall, nested window visibility, unaffected patch options, new/deleted nodes, queued layout cancellation, serialization, Save As isolation, rename/delete, duplicate/invalid names, corrupt files, repeated switching.");
}
'''


def main():
    source = (ROOT / 'src/ofxOceanode.cpp').read_text()
    production = '\n'.join(block(source, signature) for signature in (
        'std::string workspaceNodeKey(', 'void workspaceWindowToggles(', 'void workspaceWindows(',
        'ofJson ofxOceanode::captureWorkspace()', 'void ofxOceanode::applyGUILayout(',
        'void ofxOceanode::recallWorkspace(', 'bool ofxOceanode::saveWorkspaces()',
        'void ofxOceanode::loadWorkspaces()'))
    with tempfile.TemporaryDirectory(prefix='oceanode-workspaces-') as work:
        work = Path(work)
        (work / 'imconfig.h').write_text('// Standalone ImGui.\n')
        cpp = work / 'test.cpp'
        policy = block((ROOT / 'src/ofxOceanodeShared.h').read_text(), 'static bool shouldSwitchCanvasLayout()')
        cpp.write_text(FIXTURE.replace('PRODUCTION', production).replace('WORKSPACE_SWITCH_POLICY', policy))
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
