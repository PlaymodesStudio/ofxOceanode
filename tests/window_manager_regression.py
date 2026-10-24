#!/usr/bin/env python3
"""Compile the real ofxOceanodeWindowManager.cpp against isolated graph types and run it with real ImGui.

Checks the shared window walker: captured views skip deprecated persistent nodes
while the Window menu lists them, restoring a view leaves nodes it does not name
alone, and window-toggle recognition matches the labels documented in
docs/layouts.md. ASan/UBSan are enabled. Does not launch a graphics host.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
IMGUI = Path(os.environ.get('OCEANODE_TEST_IMGUI_SRC', str(ROOT.parent / 'ofxImGui/libs/imgui/src')))
JSON = ROOT.parents[1] / 'libs/json/include'

# Isolated stand-ins for the OF and graph headers WindowManager.cpp includes.
MOCKS = {
'ofJson.h': r'''
#pragma once
#include <nlohmann/json.hpp>
using ofJson = nlohmann::json;
''',
'mock_of.h': r'''
#pragma once
#include "ofJson.h"
#include <memory>
#include <sstream>
#include <string>
#include <vector>
struct ofAbstractParameter { std::string name; virtual ~ofAbstractParameter()=default; std::string getName() const { return name; } };
template<class T> struct ofParameter : ofAbstractParameter {
    T value{}; int reads=0;
    ofParameter(std::string n="", T v=T()){ name=n; value=v; }
    T get(){ ++reads; return value; } void set(T v){ value=v; }
};
template<class T> struct ofxOceanodeParameter : ofAbstractParameter {
    ofParameter<T> parameter;
    ofxOceanodeParameter(std::string n, T v):parameter(n,v){ name=n; }
    ofParameter<T>& getParameter(){ return parameter; }
};
struct ofParameterGroup {
    std::string name; std::vector<std::shared_ptr<ofAbstractParameter>> items;
    std::string getName() const { return name; }
    auto begin(){ return items.begin(); } auto end(){ return items.end(); }
};
inline std::vector<std::string> ofSplitString(const std::string& text, const std::string&, bool, bool){
    std::vector<std::string> out; std::istringstream s(text); std::string line; while(std::getline(s, line)) out.push_back(line); return out;
}
inline std::string ofTrim(std::string s){ auto a=s.find_first_not_of(" \r\t"); if(a==std::string::npos) return {}; return s.substr(a, s.find_last_not_of(" \r\t")-a+1); }
inline float ofGetFrameRate(){ return 60; }
template<class T> std::string ofToString(const T& v){ std::ostringstream s; s<<v; return s.str(); }
''',
'ofxOceanodeContainer.h': r'''
#pragma once
#include "mock_of.h"
#include "ofxOceanodeCustomGuiExtension.h"
#include <map>
struct ofxOceanodeNodeModel {
    virtual ~ofxOceanodeNodeModel()=default;
    std::string name="Synth"; int number=1;
    std::string nodeName(){ return name; } int getNumIdentifier(){ return number; }
};
struct ofxOceanodeNode {
    bool persistent=false;
    std::shared_ptr<ofxOceanodeNodeModel> model=std::make_shared<ofxOceanodeNodeModel>();
    ofParameterGroup parameters, inspector;
    bool getIsPersistent(){ return persistent; }
    ofxOceanodeNodeModel& getNodeModel(){ return *model; }
    ofParameterGroup& getParameters(){ return parameters; }
    ofParameterGroup& getInspectorParameters(){ return inspector; }
};
struct Panels : ofxOceanodeCustomGuiExtension {
    std::map<std::string,bool> open; std::vector<std::string> listedPaths; int creates=0;
    void clear() override {} void draw() override {} void nodesChanged() override {}
    void loadPreset(const std::string&) override {} void savePreset(const std::string&) override {}
    bool containsParameter(ofxOceanodeAbstractParameter&) const override { return false; }
    void removeParameterBindings(ofxOceanodeAbstractParameter&) override {}
    void collectBoundParameters(std::unordered_set<ofxOceanodeAbstractParameter*>&) const override {}
    void drawParameterMenu(ofxOceanodeAbstractParameter&) override {} void drawCanvasMenu() override {}
    void drawCreationModal() override {} void requestCreate() override { ++creates; }
    std::vector<WindowEntry> windowEntries(const std::string& path) override {
        listedPaths.push_back(path); std::vector<WindowEntry> out;
        for(auto& p : open) out.push_back({path + p.first, []{}});
        return out;
    }
    void windowState(ofJson& state, bool restore) override {
        for(auto& p : open){
            if(!restore) state["panels"][p.first] = p.second;
            else if(state.contains("panels") && state["panels"].contains(p.first)) p.second = state["panels"][p.first].get<bool>();
        }
    }
    ofJson panelsForReview() const override { return {}; } ofJson snapshotsForReview() const override { return {}; }
    ofJson captureState() const override { return {}; } void restoreState(const ofJson&, const ofJson&) override {}
    ofJson captureNodeBindings(const std::string&) const override { return {}; } void restoreNodeBindings(const ofJson&) override {}
    void migrateTo(ofxOceanodeCustomGuiExtension&, const std::map<std::string,std::string>&) override {}
    void importMovedState(const ofJson&, const std::map<std::string,std::string>&) override {}
};
struct ofxOceanodeContainer {
    std::vector<ofxOceanodeNode*> nodes; Panels panels; bool hasPanels=true;
    std::vector<ofxOceanodeNode*> getAllModules(){ return nodes; }
    ofxOceanodeCustomGuiExtension* getCustomGuiExtension(){ return hasPanels ? &panels : nullptr; }
    ofxOceanodeContainer* getContainerForCanvasID(const std::string&){ return nullptr; }
};
''',
'ofxOceanodeNodeMacro.h': r'''
#pragma once
#include "ofxOceanodeContainer.h"
struct ofxOceanodeNodeMacro : ofxOceanodeNodeModel {
    std::shared_ptr<ofxOceanodeContainer> child=std::make_shared<ofxOceanodeContainer>();
    bool visible=true; int forcedDock=0;
    bool isWindowVisible(){ return visible; } bool isSharedWindowVisible(){ return visible; }
    bool isWindowShownByOtherInstance(){ return false; }
    void setWindowVisible(bool b, bool focus=true){ visible=b; if(focus) ++forcedDock; }
    std::shared_ptr<ofxOceanodeContainer> getContainer(){ return child; }
};
''',
'ofxOceanodeControls.h': r'''
#pragma once
#include "mock_of.h"
#include <map>
struct ofxOceanodeBaseController {
    std::string name; bool menu=false;
    std::string getControllerName(){ return name; } bool isMenuController(){ return menu; }
};
struct ofxOceanodeControls {
    std::map<std::string,bool> visibility{{"Nodes",true},{"Timeline",true}};
    std::vector<std::shared_ptr<ofxOceanodeBaseController>> controllers{
        std::make_shared<ofxOceanodeBaseController>(ofxOceanodeBaseController{"Nodes"}),
        std::make_shared<ofxOceanodeBaseController>(ofxOceanodeBaseController{"Timeline"})};
    std::map<std::string,bool>& getControllersVisibility(){ return visibility; }
    std::vector<std::shared_ptr<ofxOceanodeBaseController>>& getControllers(){ return controllers; }
};
''',
'ofxOceanodeScope.h': r'''
#pragma once
struct ofxOceanodeScope {
    bool visible=true;
    static ofxOceanodeScope* getInstance(){ static ofxOceanodeScope s; return &s; }
    bool isVisible(){ return visible; } void setVisible(bool v){ visible=v; }
};
''',
'ofxOceanodeCanvas.h': r'''
#pragma once
struct ofxOceanodeCanvas { int focusRequests=0; void requestFocus(){ ++focusRequests; } };
''',
'ofxOceanodeShared.h': r'''
#pragma once
#include "ofxOceanodeCanvas.h"
#include <string>
struct ofxOceanodeShared {
    inline static unsigned int dockspace=0, left=0, central=0;
    inline static ofxOceanodeCanvas canvas;
    inline static std::string active;
    static unsigned int getDockspaceID(){ return dockspace; } static void setDockspaceID(unsigned int id){ if(!dockspace) dockspace=id; }
    static unsigned int getLeftNodeID(){ return left; } static void setLeftNodeID(unsigned int id){ left=id; }
    static unsigned int getCentralNodeID(){ return central; } static void setCentralNodeID(unsigned int id){ central=id; }
    static ofxOceanodeCanvas* getRootCanvas(){ return &canvas; }
    static const std::string& getActiveCanvasUniqueID(){ return active; }
};
''',
}

CASES = r'''
#include "ofxOceanodeWindowManager.h"
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeControls.h"
#include "ofxOceanodeNodeMacro.h"
#include "ofxOceanodeScope.h"
#include "imgui.h"
#include <cassert>
#include <cstdio>
#include <cctype>
using namespace std;

// The recognition rule of the deleted src/ofxOceanodeWindowVisibility.h.
bool oldIsWindowToggle(const string& label){
    string key;
    for(unsigned char c : label) if(isalnum(c)) key += static_cast<char>(tolower(c));
    return key == "show" || key == "showwin" || key == "showwindow" ||
        key == "showgui" || key == "showeditor" || key == "showsnapshot" ||
        key == "showall" || key == "showsplit";
}

ofxOceanodeWindowManager* manager = nullptr;
void frame(bool menu){
    ImGui::NewFrame();
    manager->beginHostWindow();
    if(menu){ ImGui::Begin("Window menu"); manager->drawWindowMenu(); ImGui::End(); }
    manager->endHostWindow();
    manager->endFrame();
    ImGui::Render();
}

int main(){
    ImGui::CreateContext(); auto& io = ImGui::GetIO(); io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; io.DisplaySize = {1200, 800}; io.DeltaTime = 1.f/60;
    unsigned char* pixels; int w, h; io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);

    for(const char* label : LABELS) assert(ofxOceanodeWindowManager::isWindowToggle(label) == oldIsWindowToggle(label));
    for(const char* label : {"Show", "Show.Win", "Show Snapshot?", "show_window", "Show Names", "Show Clipping", "Grid", ""})
        assert(ofxOceanodeWindowManager::isWindowToggle(label) == oldIsWindowToggle(label));

    auto root = make_shared<ofxOceanodeContainer>();
    ofxOceanodeControls controls;
    ofxOceanodeWindowManager windows; manager = &windows;
    windows.setup(root, controls);
    windows.installImGuiHandlers();

    ofxOceanodeNode synth, persistent, macroNode, inner;
    synth.parameters.name = "Synth_1";
    auto show = make_shared<ofxOceanodeParameter<bool>>("Show Window", true);
    auto names = make_shared<ofParameter<bool>>("Show Names", true);
    synth.parameters.items = {show, names};
    persistent.persistent = true; persistent.model->name = "Persist"; persistent.model->number = 9;
    persistent.parameters.name = "Persist_9";
    auto persistentShow = make_shared<ofParameter<bool>>("Show", true);
    persistent.parameters.items = {persistentShow};
    auto macro = make_shared<ofxOceanodeNodeMacro>(); macro->name = "Macro"; macroNode.model = macro;
    macroNode.parameters.name = "Macro_1";
    inner.model->number = 2; inner.parameters.name = "Inner_2";
    auto innerShow = make_shared<ofParameter<bool>>("Show GUI", true);
    inner.inspector.items = {innerShow};
    macro->child->nodes = {&inner}; macro->child->panels.open = {{"nested", true}};
    root->nodes = {&synth, &persistent, &macroNode}; root->panels.open = {{"root", true}};

    // Captured views skip persistent nodes; the Window menu lists them.
    auto view = windows.captureView();
    assert(view["controllers"]["Nodes"] == true && view["scopes"] == true);
    const auto& nodes = view["windows"]["nodes"];
    assert(nodes.size() == 2 && nodes.contains(ofJson::array({"Synth", 1}).dump()));
    assert(!nodes.contains(ofJson::array({"Persist", 9}).dump()));
    assert(persistentShow->reads == 0);
    const auto& macroState = nodes[ofJson::array({"Macro", 1}).dump()];
    assert(macroState["open"] == true && macroState["children"]["nodes"].size() == 1);
    assert(macroState["children"]["panels"]["nested"] == true && view["windows"]["panels"]["root"] == true);
    assert(nodes[ofJson::array({"Synth", 1}).dump()]["parameters"]["Show Window"] == true);
    assert(!nodes[ofJson::array({"Synth", 1}).dump()]["parameters"].contains("Show Names"));
    frame(true); frame(true);
    assert(persistentShow->reads > 0 && show->parameter.reads > 0 && innerShow->reads > 0);
    assert(names->reads == 0);
    assert((root->panels.listedPaths.back() == "" && macro->child->panels.listedPaths.back() == "Macro_1 / "));

    // Restoring changes what the view names and leaves everything else alone.
    show->parameter.set(false); innerShow->set(false); macro->visible = false;
    root->panels.open["root"] = false; macro->child->panels.open["nested"] = false;
    controls.visibility["Timeline"] = false; ofxOceanodeScope::getInstance()->setVisible(false);
    persistentShow->set(false);
    ofxOceanodeNode added; added.model->name = "Added"; added.parameters.name = "Added_1";
    auto addedShow = make_shared<ofParameter<bool>>("Show", false); added.parameters.items = {addedShow};
    root->nodes.push_back(&added);
    windows.restoreView(view);
    assert(show->parameter.get() && innerShow->get() && macro->visible && macro->forcedDock == 0);
    assert(root->panels.open["root"] && macro->child->panels.open["nested"]);
    assert(controls.visibility["Timeline"] && ofxOceanodeScope::getInstance()->isVisible());
    assert(!persistentShow->get() && !addedShow->get());
    // A view with a missing node key (or no windows at all) leaves the node unchanged.
    auto partial = view; partial["windows"]["nodes"].erase(ofJson::array({"Synth", 1}).dump());
    show->parameter.set(false); windows.restoreView(partial); assert(!show->parameter.get());
    ofJson empty = {{"controllers", ofJson::object()}, {"windows", ofJson::object()}};
    windows.restoreView(empty); assert(!show->parameter.get() && innerShow->get());

    // Applying ini text keeps the Nodes anchor for controllers absent from it.
    frame(false);
    const auto ini = windows.captureIni();
    windows.clearIni(); windows.applyIni(ini); frame(false); frame(false);
    assert(windows.captureIni().find("[Docking]") != string::npos);
    assert(!windows.isShowMode()); windows.showModeRef() = true; assert(windows.isShowMode());
    ImGui::DestroyContext();
    puts("PASS: captured views skip persistent nodes and the Window menu lists them, nested Macros and Custom GUIs, restore leaves unnamed nodes alone, documented window toggles, ini application.");
}
'''


def documented_labels():
    text = (ROOT / 'docs/layouts.md').read_text()
    labels = [re.sub(r'\s+', ' ', label) for label in re.findall(r'`(Show[^`]*)`', text)]
    assert len(labels) >= 8, labels
    return labels


def main():
    with tempfile.TemporaryDirectory(prefix='oceanode-window-manager-') as work:
        work = Path(work)
        (work / 'imconfig.h').write_text('// Standalone ImGui.\n')
        for name, text in MOCKS.items():
            (work / name).write_text(text)
        shutil.copy(ROOT / 'src/Managers/ofxOceanodeCustomGuiExtension.h', work)
        labels = ', '.join('"%s"' % label for label in documented_labels())
        cpp = work / 'test.cpp'
        cpp.write_text(CASES.replace('LABELS', '{' + labels + '}'))
        gui = ROOT / 'src/Gui'
        binary = work / 'test'
        subprocess.run([os.environ.get('CXX', 'clang++'), '-std=c++17', '-O0', '-g',
                        '-fsanitize=address,undefined', '-I', str(work), '-I', str(IMGUI),
                        '-I', str(JSON), '-I', str(ROOT / 'src'), '-I', str(gui), str(cpp),
                        *[str(gui / name) for name in ('ofxOceanodeWindowManager.cpp', 'ofxOceanodeProportionalDocking.cpp',
                                                        'ofxOceanodeDockCollapse.cpp', 'ofxOceanodeFloatingWindowDecor.cpp')],
                        *[str(IMGUI / name) for name in
                          ('imgui.cpp', 'imgui_draw.cpp', 'imgui_tables.cpp', 'imgui_widgets.cpp')],
                        '-o', str(binary)], check=True)
        subprocess.run([str(binary)], cwd=work, check=True)


if __name__ == '__main__':
    main()
