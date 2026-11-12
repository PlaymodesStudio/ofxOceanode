#!/usr/bin/env python3
"""Exercise the production low-zoom pin layout with real ImGui frames."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
IMGUI = Path(os.environ.get('OCEANODE_TEST_IMGUI_SRC', str(ROOT.parent / 'ofxImGui/libs/imgui/src')))
source = (ROOT / 'src/Nodes/ofxOceanodeNodeGui.cpp').read_text()
start = source.index('            // A node loaded at low zoom')
end = source.index('        } // end if(renderGui)', start)
body = source[start:end]
fixture = r'''
#include "imgui.h"
#include <vector>
#include <cassert>
#include <cmath>
#include <cstdio>
namespace glm { struct vec2 { float x, y; vec2(float a=0,float b=0):x(a),y(b){} }; }
const int ofxOceanodeParameterFlags_NoGuiWidget = 1;
struct ofxOceanodeAbstractParameter { int flags=0; bool header=false, shown=true; int getFlags(){return flags;} };
struct Params { std::vector<ofxOceanodeAbstractParameter> v; size_t size(){return v.size();} auto& get(int i){return v[i];} } params;
struct Node { bool isParameterShownInGui(ofxOceanodeAbstractParameter& p){return p.shown;} } node;
Params& getParameters(){return params;}
std::vector<glm::vec2> inputPositions, outputPositions;
float cachedContentHeight=0, zoomLevel=.25f, nodeWidthText=20, nodeWidthWidget=40;
void layout(float y) {
 ImGui::NewFrame(); ImGui::SetNextWindowPos(ImVec2(0,0)); ImGui::SetNextWindowSize(ImVec2(800,800));
 ImGui::Begin("test"); ImGui::SetCursorScreenPos(ImVec2(100,y));
 auto isHeaderToggle=[](ofxOceanodeAbstractParameter& p){return p.header;};
 PRODUCTION_BODY
 ImGui::Dummy(ImVec2(1,1));
 ImGui::End(); ImGui::Render();
}
int main(){
 ImGui::CreateContext(); auto& io=ImGui::GetIO(); io.IniFilename=nullptr; io.DisplaySize=ImVec2(800,800); io.DeltaTime=1.f/60;
 unsigned char* pixels; int w,h; io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
 params.v.resize(3); inputPositions.resize(3); outputPositions.resize(3);
 layout(200); for(auto p:inputPositions) assert(p.y>=200 && p.y<=215);
 layout(350); for(auto p:outputPositions) assert(p.y>=350 && p.y<=365);
 cachedContentHeight=120; layout(250); assert(std::abs(inputPositions[2].y-280)<.01f);
 cachedContentHeight=0; for(auto& p:params.v) p.flags=ofxOceanodeParameterFlags_NoGuiWidget;
 layout(400); for(auto p:inputPositions) assert(std::abs(p.y-400)<.01f);
 params.v[0].header=true; inputPositions[0].y=123; layout(450);
 assert(inputPositions[0].y==123 && inputPositions[1].y==450);
 ImGui::DestroyContext(); puts("PASS: first-frame low-zoom pins, panning, measured height and widgetless rows");
}
'''.replace('PRODUCTION_BODY', body)
with tempfile.TemporaryDirectory(prefix='oceanode-low-zoom-') as tmp:
    tmp = Path(tmp)
    (tmp / 'imconfig.h').write_text('// Standalone ImGui.\n')
    cpp = tmp / 'test.cpp'
    cpp.write_text(fixture)
    cmd = ['c++', '-std=c++17', '-I'+str(tmp), '-I'+str(IMGUI), str(cpp)]
    cmd += [str(IMGUI / name) for name in ('imgui.cpp','imgui_draw.cpp','imgui_tables.cpp','imgui_widgets.cpp')]
    cmd += ['-o', str(tmp / 'test')]
    subprocess.run(cmd, check=True)
    subprocess.run([str(tmp / 'test')], check=True)
