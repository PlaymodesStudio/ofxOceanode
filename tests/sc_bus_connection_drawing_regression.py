#!/usr/bin/env python3
"""Check ScBus wire classification and dashed curve geometry with real ImGui.

Uses production drawing code and the canvas type classifier. No OF/GPU/audio
server is needed. ASan/UBSan cover curved, short, reversed and very long wires.
"""
from pathlib import Path
import os
import subprocess
import tempfile
from canvas_navigation_regression import ROOT, IMGUI, block

CODE = r'''
#include "imgui.h"
#include "imgui_internal.h"
#include "ofxOceanodeConnectionDrawing.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
using namespace std;
struct Registry {
    map<string,string> names;int lookups=0;
    string getTypeNameFromTypeDescription(string type){++lookups;return names[type];}
};
struct ofxOceanodeAbstractParameter {string type;string valueType(){return type;}};
struct Container {shared_ptr<Registry> registry=make_shared<Registry>();auto getTypesRegistry(){return registry;}};
static void classify(Container* container){
    unordered_map<string,bool> scBusTypes;
    CLASSIFIER;
    ofxOceanodeAbstractParameter audio{"opaque-node-port"},number{"float"},unknown{"SC-looking-name"};
    assert(isScBusParameter(&audio));assert(!isScBusParameter(&number));
    assert(!isScBusParameter(&unknown));assert(!isScBusParameter(nullptr));
    int calls=container->registry->lookups;
    assert(isScBusParameter(&audio));assert(container->registry->lookups==calls);
    container->registry.reset();ofxOceanodeAbstractParameter another{"unregistered"};
    assert(!isScBusParameter(&another));
}
static void near(float a,float b){assert(abs(a-b)<.03f);}
static void bounds(const ImDrawList& list,int offset,float& lo,float& hi){
    lo=hi=list.VtxBuffer[offset].pos.x;
    for(int j=0;j<4;++j){lo=min(lo,list.VtxBuffer[offset+j].pos.x);hi=max(hi,list.VtxBuffer[offset+j].pos.x);}
}
int main(){
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;
    unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    Container container;container.registry->names["opaque-node-port"]="ScBus";
    container.registry->names["float"]="float";classify(&container);
    io.DisplaySize={1200,800};io.DeltaTime=1.f/60;ImGui::NewFrame();
    {
    ImDrawList list(ImGui::GetDrawListSharedData());
    auto reset=[&](){list._ResetForNewFrame();list.PushClipRectFullScreen();list.Flags=ImDrawListFlags_None;};
    oceanodeConnectionDrawing::Renderer renderer(&list);
    const ImU32 color=IM_COL32(100,60,30,128);
    for(float zoom:{.1f,1.f,2.f}){
        reset();const float dash=max(3.f,8.f*zoom),gap=max(2.f,5.f*zoom),period=dash+gap;
        renderer.bezier({0,0},{1,0},{129,0},{130,0},color,2,true,zoom);
        int count=int(ceil(130/period));assert(list.VtxBuffer.Size==count*4);
        for(int i=0;i<count;++i){
            float lo,hi;bounds(list,i*4,lo,hi);near(lo,i*period);near(hi,min(130.f,i*period+dash));
        }
        for(const auto& vertex:list.VtxBuffer){assert(vertex.col==color);near(abs(vertex.pos.y),1.5f);}
        assert(list._Path.empty());
    }
    reset();renderer.bezier({0,0},{1,0},{2,0},{3,0},color,2,true,1);
    assert(list.VtxBuffer.Size==4);float lo,hi;bounds(list,0,lo,hi);near(lo,0);near(hi,3);
    reset();renderer.bezier({130,0},{129,0},{1,0},{0,0},color,2,true,1);
    assert(list.VtxBuffer.Size==40);bounds(list,0,lo,hi);near(lo,122);near(hi,130);
    reset();renderer.bezier({0,0},{0,0},{0,0},{0,0},color,2,true,1);
    assert(list.VtxBuffer.empty()&&list._Path.empty());
    reset();renderer.bezier({0,0},{100,900},{-100,-900},{600,0},color,2,true,1);
    assert(list.VtxBuffer.Size>40&&list._Path.empty());
    for(const auto& v:list.VtxBuffer)assert(isfinite(v.pos.x)&&isfinite(v.pos.y)&&v.col==color);
    reset();renderer.bezier({0,0},{1,0},{1e8f,0},{1e8f,0},color,2,true,1);
    assert(list.VtxBuffer.Size<20000&&list._Path.empty());
    reset();renderer.bezier({0,0},{1,0},{129,0},{130,0},color,2,false,1);
    assert(list.VtxBuffer.Size==4);bounds(list,0,lo,hi);near(lo,0);near(hi,130);
    reset();renderer.bezier({0,0},{1,0},{129,0},{130,0},0,2,true,1);assert(list.VtxBuffer.empty());
    }
    ImGui::Render();ImGui::DestroyContext();
    puts("ScBus wires passed: registered type routing, cached lookup, zoom-aware dash gaps, colors, thickness, curves, reversed/short/degenerate/long wires and solid fallback.");
}
'''

def main():
    canvas=(ROOT/'src/Managers/ofxOceanodeCanvas.cpp').read_text()
    classifier=block(canvas,'auto isScBusParameter =')
    assert 'isScBusParameter(&sourceParameter) || isScBusParameter(&sinkParameter)' in canvas
    assert 'isScBusParameter(tempSourceParameter) || isScBusParameter(tempSinkParameter)' in canvas
    code=CODE.replace('CLASSIFIER',classifier)
    with tempfile.TemporaryDirectory(prefix='oceanode-sc-bus-lines-') as directory:
        work=Path(directory);(work/'imconfig.h').write_text('')
        cpp=work/'test.cpp';cpp.write_text(code);binary=work/'test'
        subprocess.run([os.environ.get('CXX','clang++'),'-std=c++17','-O1','-g','-fsanitize=address,undefined',
                        '-I',str(IMGUI),'-I',str(work),'-I',str(ROOT/'src/Managers'),str(cpp),
                        *[str(IMGUI/name) for name in ('imgui.cpp','imgui_draw.cpp','imgui_widgets.cpp','imgui_tables.cpp')],
                        '-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True,timeout=30)

if __name__=='__main__':main()
