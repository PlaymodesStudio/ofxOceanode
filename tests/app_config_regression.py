#!/usr/bin/env python3
"""Compile the real ofxOceanodeAppConfig.cpp against isolated OF/graph types.

save() then load() round-trips all 13 keys of config/config.json, a missing key
leaves its target unchanged, and a file whose version is not 2 is ignored.
ASan/UBSan are enabled. Does not launch a graphics host.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
IMGUI = Path(os.environ.get('OCEANODE_TEST_IMGUI_SRC', str(ROOT.parent / 'ofxImGui/libs/imgui/src')))
JSON = ROOT.parents[1] / 'libs/json/include'

MOCKS = {
'ofJson.h': r'''
#pragma once
#include <nlohmann/json.hpp>
using ofJson = nlohmann::json;
''',
'ofMain.h': r'''
#pragma once
#include "ofJson.h"
#include <filesystem>
#include <fstream>
#include <string>
inline int frameRate = 0, verticalSyncCalls = 0; inline bool verticalSync = false;
inline void ofSetFrameRate(int fps){ frameRate = fps; }
inline void ofSetVerticalSync(bool on){ verticalSync = on; ++verticalSyncCalls; }
inline float ofGetFrameRate(){ return 60; }
inline std::string ofToDataPath(const std::string& path, bool = false){ return path; }
struct ofFile { std::string path; ofFile(std::string p):path(p){} bool exists(){ return std::filesystem::exists(path); } };
struct ofDirectory { static bool createDirectory(const std::string& path, bool, bool){ std::filesystem::create_directories(path); return true; } };
inline ofJson ofLoadJson(const std::string& path){ std::ifstream in(path); return ofJson::parse(in, nullptr, false); }
inline bool ofSavePrettyJson(const std::string& path, const ofJson& json){ std::ofstream out(path); out << json.dump(4); return bool(out); }
struct Logger { template<class T> Logger& operator<<(const T&){ return *this; } };
inline int loggedErrors = 0;
inline Logger ofLogError(const std::string&){ ++loggedErrors; return {}; }
''',
'ofxOceanodeShared.h': r'''
#pragma once
typedef int ofxOceanodeConfigurationFlags;
enum { ofxOceanodeConfigurationFlags_DisableRenderAll = 1, ofxOceanodeConfigurationFlags_DisableHistograms = 4,
       ofxOceanodeConfigurationFlags_ColorConnections = 8, ofxOceanodeConfigurationFlags_DashedDottedLines = 16 };
struct ofxOceanodeShared {
    inline static int flags = 0, precision = 32, snapDivs = 0; inline static bool snap = false, autoInspector = true, centers = false;
    inline static float connection = 3.0f;
    static int getConfigurationFlags(){ return flags; } static void setConfigurationFlags(int f){ flags = f; }
    static int getColorTexturePrecision(){ return precision; } static void setColorTexturePrecision(int p){ precision = p; }
    static void setSnapToGrid(bool b){ snap = b; } static void setSnapGridDiv(int d){ snapDivs = d; }
    static bool getAutoInspectorShowHide(){ return autoInspector; } static void setAutoInspectorShowHide(bool b){ autoInspector = b; }
    static bool getSelectionCentersCanvas(){ return centers; } static void setSelectionCentersCanvas(bool b){ centers = b; }
    static float getConnectionWidth(){ return connection; } static void setConnectionWidth(float w){ connection = w; }
};
''',
'ofxOceanodeCanvas.h': r'''
#pragma once
struct ofxOceanodeCanvas {
    int textWidth = 90, widgetWidth = 150, divisions = 8, gridUpdates = 0; bool snap = true; float zoom = 1;
    int getNodeWidthText(){ return textWidth; } void setNodeWidthText(int n){ textWidth = n; }
    int getNodeWidthWidget(){ return widgetWidth; } void setNodeWidthWidget(int n){ widgetWidth = n; }
    int getGridDivisions(){ return divisions; } void setGridDivisions(int n){ divisions = n; }
    void updateGridSize(){ ++gridUpdates; }
    bool getSnapToGrid(){ return snap; } void setSnapToGrid(bool b){ snap = b; }
    float getZoomLevel(){ return zoom; } void setZoomLevel(float z){ zoom = z; }
};
''',
'ofxOceanodeTimeController.h': r'''
#pragma once
struct ofxOceanodeTimeController {
    bool space = false;
    bool getSpaceTogglesPlay() const { return space; } void setSpaceTogglesPlay(bool b){ space = b; }
};
''',
'ofxOceanodeControls.h': r'''
#pragma once
#include "ofxOceanodeTimeController.h"
#include <memory>
struct ofxOceanodeControls {
    std::shared_ptr<ofxOceanodeTimeController> time = std::make_shared<ofxOceanodeTimeController>();
    template<class T> std::shared_ptr<T> get(){ return time; }
};
''',
}

CASES = r'''
#include "ofxOceanodeAppConfig.h"
#include "ofxOceanodeWindowManager.h"
#include "ofxOceanodeProportionalDocking.h"
#include "ofxOceanodeCanvas.h"
#include "ofxOceanodeControls.h"
#include "ofxOceanodeShared.h"
#include "ofMain.h"
#include <cassert>
#include <cstdio>

// AppConfig only reads and sets show mode, both inline in the real header.
ofxOceanodeWindowManager::ofxOceanodeWindowManager(){}
ofxOceanodeWindowManager::~ofxOceanodeWindowManager(){}

struct State {
    int fps; bool vsync; int flags; bool show; int precision; int text, widget, divisions; bool snap, inspector, centers;
    float connection; bool space;
};
State read(ofxOceanodeCanvas& c, ofxOceanodeControls& k, ofxOceanodeWindowManager& w){
    return {frameRate, verticalSync, ofxOceanodeShared::flags, w.isShowMode(), ofxOceanodeShared::precision,
            c.textWidth, c.widgetWidth, c.divisions, c.snap, ofxOceanodeShared::autoInspector,
            ofxOceanodeShared::centers, ofxOceanodeShared::connection, k.time->space};
}
bool operator==(const State& a, const State& b){
    return a.fps==b.fps && a.vsync==b.vsync && a.flags==b.flags && a.show==b.show && a.precision==b.precision &&
        a.text==b.text && a.widget==b.widget && a.divisions==b.divisions && a.snap==b.snap && a.inspector==b.inspector &&
        a.centers==b.centers && a.connection==b.connection && a.space==b.space;
}
void scramble(ofxOceanodeCanvas& c, ofxOceanodeControls& k, ofxOceanodeWindowManager& w){
    ofxOceanodeShared::flags = 0; ofxOceanodeShared::precision = 32; ofxOceanodeShared::autoInspector = true;
    ofxOceanodeShared::centers = false; ofxOceanodeShared::connection = 3; ofxOceanodeShared::snap = true;
    c.textWidth = 90; c.widgetWidth = 150; c.divisions = 8; c.snap = true; k.time->space = false;
    w.setShowMode(false); frameRate = 0; verticalSync = true;
}

int main(){
    ofxOceanodeCanvas canvas; ofxOceanodeControls controls; ofxOceanodeWindowManager windows;
    ofxOceanodeAppConfig config;
    // No file: every target keeps its own default; nothing is written.
    config.setup(canvas, controls, windows);
    assert(!std::filesystem::exists("config/config.json") && frameRate == 0 && verticalSyncCalls == 0);
    config.save();
    auto defaults = ofLoadJson("config/config.json");
    assert(defaults.size() == 14 && defaults["version"] == 2 && defaults["desiredFPS"] == 60 && defaults["vsync"] == true);
    assert(!defaults.contains("defaultLayout") && !defaults.contains("fps"));

    // Round trip of all 13 keys from their live targets.
    frameRate = 0;
    ofxOceanodeShared::flags = ofxOceanodeConfigurationFlags_ColorConnections | ofxOceanodeConfigurationFlags_DisableRenderAll;
    ofxOceanodeShared::precision = 16; ofxOceanodeShared::autoInspector = false; ofxOceanodeShared::centers = true;
    ofxOceanodeShared::connection = 5.5f;
    canvas.textWidth = 64; canvas.widgetWidth = 200; canvas.divisions = 4; canvas.snap = false;
    controls.time->space = true; windows.showModeRef() = true;
    ofSavePrettyJson("config/config.json", {{"version", 2}, {"desiredFPS", 30}, {"vsync", false}});
    config.load(); assert(frameRate == 30 && !verticalSync);
    const State saved = read(canvas, controls, windows);
    config.save();
    auto written = ofLoadJson("config/config.json");
    assert(written.size() == 14 && written["desiredFPS"] == 30 && written["vsync"] == false && written["showMode"] == true);
    assert(written["selectionCentersCanvas"] == true && written["spaceTogglesPlay"] == true && written["gridDivisions"] == 4);
    scramble(canvas, controls, windows);
    config.load();
    assert(read(canvas, controls, windows) == saved);
    assert(ofxOceanodeShared::snap == false && ofxOceanodeShared::snapDivs == 4 && canvas.gridUpdates > 0);

    // A missing key leaves its target unchanged.
    ofSavePrettyJson("config/config.json", {{"version", 2}, {"nodeTextWidth", 100}, {"spaceTogglesPlay", false}});
    scramble(canvas, controls, windows);
    ofxOceanodeShared::precision = 8; ofxOceanodeShared::centers = true; ofxOceanodeShared::connection = 7; canvas.divisions = 12;
    windows.setShowMode(true); controls.time->space = true; verticalSync = false; const int vsyncCalls = verticalSyncCalls;
    config.load();
    assert(canvas.textWidth == 100 && !controls.time->space);
    assert(ofxOceanodeShared::precision == 8 && ofxOceanodeShared::centers && ofxOceanodeShared::connection == 7);
    assert(canvas.divisions == 12 && windows.isShowMode() && frameRate == 0 && verticalSyncCalls == vsyncCalls);

    // Another version is ignored, including the pre-migration format.
    for(int version : {0, 1, 3}){
        ofJson old = {{"desiredFPS", 12}, {"nodeTextWidth", 33}, {"defaultLayout", "init.ini"}};
        if(version) old["version"] = version;
        ofSavePrettyJson("config/config.json", old);
        const int errors = loggedErrors;
        config.load();
        assert(frameRate == 0 && canvas.textWidth == 100 && loggedErrors == errors + 1);
    }
    puts("PASS: all 13 config keys round-trip from live targets, missing keys keep targets, other versions ignored.");
}
'''


def main():
    with tempfile.TemporaryDirectory(prefix='oceanode-app-config-') as work:
        work = Path(work)
        (work / 'imconfig.h').write_text('// Standalone ImGui.\n')
        for name, text in MOCKS.items():
            (work / name).write_text(text)
        cpp = work / 'test.cpp'
        cpp.write_text(CASES)
        gui = ROOT / 'src/Gui'
        binary = work / 'test'
        subprocess.run([os.environ.get('CXX', 'clang++'), '-std=c++17', '-O0', '-g',
                        '-fsanitize=address,undefined', '-I', str(work), '-I', str(IMGUI),
                        '-I', str(JSON), '-I', str(ROOT / 'src'), '-I', str(gui), str(cpp),
                        *[str(gui / name) for name in ('ofxOceanodeAppConfig.cpp', 'ofxOceanodeProportionalDocking.cpp',
                                                        'ofxOceanodeDockCollapse.cpp')],
                        *[str(IMGUI / name) for name in
                          ('imgui.cpp', 'imgui_draw.cpp', 'imgui_tables.cpp', 'imgui_widgets.cpp')],
                        '-o', str(binary)], check=True)
        subprocess.run([str(binary)], cwd=work, check=True)


if __name__ == '__main__':
    main()
