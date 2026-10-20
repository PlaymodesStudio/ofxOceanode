#!/usr/bin/env python3
"""Check production preset centering across menu-time loads and deferred layouts.

Uses the real reset, load callback prefix, next-frame re-arm and draw centering
code with isolated graph/viewport types. No OF/GPU host is launched.
"""
from pathlib import Path
import os
import subprocess
import tempfile
from new_preset_regression import block
ROOT=Path(__file__).resolve().parents[1]
FIXTURE=r'''
#include <cassert>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>
using namespace std;
namespace glm {
struct vec2 {
    float x,y;vec2(float v=0):x(v),y(v){}vec2(float a,float b):x(a),y(b){}
    vec2 operator/(float v) const{return {x/v,y/v};}
};
}
struct ofxOceanodeCanvas {
    glm::vec2 scrolling{-700,900},contentRegionSize{0,0};float zoomLevel=1;
    bool returnToOldScrolling=true,centerOriginPending=false,recenterCanvas=false;
    void* pendingCenterNode=this;int pendingCenterFrames=5;
    struct {string nodeID;} inspectorSelectionClick{"Old selection"};
    void resetViewToOrigin();void center();
};
struct Model {virtual ~Model()=default;};
struct Node {Model* model;Model& getNodeModel(){return *model;}};
struct ofxOceanodeContainer {
    vector<Node*> nodes;auto getAllModules(){return nodes;}
};
struct ofxOceanodeNodeMacro:Model {
    shared_ptr<ofxOceanodeContainer> child=make_shared<ofxOceanodeContainer>();
    ofxOceanodeCanvas canvas;
    auto getContainer(){return child;}auto* getCanvas(){return &canvas;}
};
enum {ofxOceanodePresetLoadType_FullPreset,ofxOceanodePresetLoadType_ClipboardPaste};
struct ofxOceanodeShared {
    inline static int type=ofxOceanodePresetLoadType_FullPreset;
    static int getPresetLoadType(){return type;}
};
struct App {
    shared_ptr<ofxOceanodeContainer> container=make_shared<ofxOceanodeContainer>();
    ofxOceanodeCanvas canvas;bool presetCanvasCenterPending=false;
    void loadWorkspaces(){}
    void loaded();void beforeGuiFrame();
};
PRODUCTION
static void centered(ofxOceanodeCanvas& view,glm::vec2 size){
    view.contentRegionSize=size;view.center();
    assert(!view.centerOriginPending);
    assert(view.scrolling.x*view.zoomLevel==size.x*.5f);
    assert(view.scrolling.y*view.zoomLevel==size.y*.5f);
}
int main(){
    for(float zoom:{.25f,1.f,2.f}){
        App app;ofxOceanodeNodeMacro outer,inner;
        Node a{&outer},b{&inner};app.container->nodes={&a};outer.child->nodes={&b};
        vector<ofxOceanodeCanvas*> views{&app.canvas,&outer.canvas,&inner.canvas};
        for(auto* view:views)view->zoomLevel=zoom;
        // A menu load can center against the old layout in the current frame.
        ofxOceanodeShared::type=ofxOceanodePresetLoadType_FullPreset;app.loaded();
        assert(app.presetCanvasCenterPending);
        for(auto* view:views){
            assert(view->centerOriginPending&&!view->pendingCenterNode&&view->inspectorSelectionClick.nodeID.empty());
            assert(!view->returnToOldScrolling);
        }
        centered(app.canvas,{400,300});centered(outer.canvas,{500,320});
        // Saved layout is applied before NewFrame on the next draw; re-arm then.
        app.beforeGuiFrame();assert(!app.presetCanvasCenterPending);
        centered(app.canvas,{900,640});centered(outer.canvas,{320,180});
        // A hidden Macro still needs centering when first displayed later.
        assert(inner.canvas.centerOriginPending);inner.canvas.center();assert(inner.canvas.centerOriginPending);
        centered(inner.canvas,{760,420});
        for(auto* view:views)assert(view->zoomLevel==zoom);
        // A second load from update also centers using that frame's viewport.
        app.loaded();app.beforeGuiFrame();centered(app.canvas,{640,480});
        // Clipboard/duplicate keeps the current pan; ordinary panning stays free.
        for(auto* view:views){view->centerOriginPending=false;view->scrolling={17,23};}
        ofxOceanodeShared::type=ofxOceanodePresetLoadType_ClipboardPaste;app.loaded();app.beforeGuiFrame();
        for(auto* view:views){view->center();assert(view->scrolling.x==17&&view->scrolling.y==23);}
    }
    puts("Preset view centering passed: menu-time loads, deferred layout sizes, nested/hidden Macros, zooms, repeated loads and clipboard preservation.");
}
'''
def main():
    canvas=(ROOT/'src/Managers/ofxOceanodeCanvas.cpp').read_text()
    app=(ROOT/'src/ofxOceanode.cpp').read_text()
    callback_start=app.index('        if(ofxOceanodeShared::getPresetLoadType() !=')
    callback_end=app.index('        if(ofxOceanodeShared::getCurrentPresetPath().empty())',callback_start)
    draw=block(app,'void ofxOceanode::draw()')
    assert draw.index('loadGUILayoutFromDisk(pendingIniLoad)') < draw.index('if(presetCanvasCenterPending)') < draw.index('gui.begin()')
    production=block(canvas,'void ofxOceanodeCanvas::resetViewToOrigin()')
    production+='\nvoid ofxOceanodeCanvas::center(){'+block(canvas,'if(centerOriginPending &&')+'}\n'
    production+=block(app,'void resetPresetCanvasViews(')
    production+='\nvoid App::loaded(){'+app[callback_start:callback_end]+'}\n'
    production+='void App::beforeGuiFrame(){'+block(app,'if(presetCanvasCenterPending)')+'}\n'
    with tempfile.TemporaryDirectory(prefix='oceanode-preset-center-') as directory:
        cpp=Path(directory)/'test.cpp';cpp.write_text(FIXTURE.replace('PRODUCTION',production));binary=Path(directory)/'test'
        subprocess.run([os.environ.get('CXX','clang++'),'-std=c++17','-O1','-g','-fsanitize=address,undefined',
                        '-fno-omit-frame-pointer',str(cpp),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
if __name__=='__main__':main()
