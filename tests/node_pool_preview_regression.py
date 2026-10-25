#!/usr/bin/env python3
"""Render the production preview fitter with real ImGui and a tall node fixture."""
from pathlib import Path
import subprocess
import tempfile
from new_preset_regression import block

ROOT = Path(__file__).resolve().parents[1]
IMGUI = ROOT.parent / 'ofxImGui/libs/imgui/src'
source = (ROOT / 'src/Managers/ofxOceanodeCanvas.cpp').read_text()
method = block(source, 'void ofxOceanodeCanvas::drawNodePreview(')
fixture = r'''
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cassert>
#include <memory>
#include <vector>
namespace glm {
struct vec2 {
    float x, y;
    vec2(float a, float b):x(a),y(b){}
    vec2 operator+(vec2 b)const{return {x+b.x,y+b.y};}
    vec2 operator-(vec2 b)const{return {x-b.x,y-b.y};}
};
}
struct ofxOceanodeShared {
    static inline float zoom=0.7f;
    static inline ImFont* bold=nullptr;
    static float getZoomLevel(){return zoom;}
    static void setZoomLevel(float z){zoom=z;}
    static ImFont* getCurrentBoldFont(){return bold;}
    static void setCurrentBoldFont(ImFont* f){bold=f;}
};
constexpr int ofxOceanodeParameterFlags_DisableInConnection=1;
constexpr int ofxOceanodeParameterFlags_DisableOutConnection=2;
struct Parameter {virtual ~Parameter()=default;};
struct ofxOceanodeAbstractParameter:Parameter {int getFlags(){return 0;}};
struct NodeGui {
    int rows=0, rendered=0;
    glm::vec2 size{0,0};
    ImVec2 start;
    bool constructGui(float text, float widget, float zoom){
        assert(zoom==1.0f);
        ImGui::BeginGroup();
        start=ImGui::GetCursorScreenPos();
        ImGui::TextUnformatted("Actual node header");
        for(int i=0;i<rows;++i){
            ImGui::PushID(i);
            float value=0.5f;
            ImGui::SetNextItemWidth(text+widget);
            ImGui::SliderFloat("##value",&value,0,1);
            ++rendered;
            ImGui::PopID();
        }
        ImGui::EndGroup();
        return true;
    }
    void setSize(glm::vec2 s){size=s;}
    unsigned int drawFrame(glm::vec2 a,glm::vec2 b,float,bool){
        ImGui::GetWindowDrawList()->AddRectFilled({a.x,a.y},{b.x,b.y},0xff444444);
        return 0xffffffff;
    }
    bool hasConnectionPosition(ofxOceanodeAbstractParameter&){return true;}
    glm::vec2 getSinkConnectionPositionFromParameter(ofxOceanodeAbstractParameter&){return {start.x,start.y+32};}
    glm::vec2 getSourceConnectionPositionFromParameter(ofxOceanodeAbstractParameter&){return {start.x+200,start.y+32};}
};
struct ofxOceanodeNode {
    NodeGui gui;
    std::vector<std::shared_ptr<Parameter>> pins{std::make_shared<ofxOceanodeAbstractParameter>()};
    auto& getNodeGui(){return gui;}
    auto& getParameters(){return pins;}
};
struct ofxOceanodeCanvas {
    inline static ImFont* zoomFonts[9]={};
    inline static ImFont* zoomFontsBold[9]={};
    glm::vec2 NODE_WINDOW_PADDING{8,7};
    float NODE_WIDTH_TEXT=100,NODE_WIDTH_WIDGET=100;
    void drawNodePreview(ofxOceanodeNode& node);
};
'''
main = r'''
int main(){
    ImGui::CreateContext();
    auto& io=ImGui::GetIO();
    io.IniFilename=nullptr;
    io.DisplaySize={1000,800};
    io.DeltaTime=1.0f/60;
    unsigned char* pixels;int width,height;
    io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    ofxOceanodeCanvas canvas;
    for(int rows:{2,100}){
        ofxOceanodeNode node;
        node.gui.rows=rows;
        for(int frame=0;frame<2;++frame){
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({0,0});
            ImGui::SetNextWindowSize({500,400});
            ImGui::Begin("Host");
            ImGui::BeginChild("Preview",{400,250},false,
                ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoBackground);
            auto* window=ImGui::GetCurrentWindow();
            auto* draw=ImGui::GetWindowDrawList();
            const int first=draw->VtxBuffer.Size;
            const auto clip=window->ClipRect;
            canvas.drawNodePreview(node);
            assert(ofxOceanodeShared::getZoomLevel()==0.7f);
            assert(window->ClipRect.Min.x==clip.Min.x && window->ClipRect.Max.y==clip.Max.y);
            assert(draw->VtxBuffer.Size>first);
            for(int v=first;v<draw->VtxBuffer.Size;++v){
                const auto p=draw->VtxBuffer[v].pos;
                assert(p.x>=clip.Min.x && p.x<=clip.Max.x);
                assert(p.y>=clip.Min.y && p.y<=clip.Max.y);
            }
            for(const auto& cmd:draw->CmdBuffer){
                if(!cmd.ElemCount)continue;
                assert(cmd.ClipRect.x>=clip.Min.x && cmd.ClipRect.z<=clip.Max.x);
                assert(cmd.ClipRect.y>=clip.Min.y && cmd.ClipRect.w<=clip.Max.y);
            }
            assert(!ImGui::IsAnyItemActive());
            ImGui::EndChild();ImGui::End();ImGui::Render();
        }
        assert(node.gui.rendered==rows*2);
    }
    ImGui::DestroyContext();
}
'''
with tempfile.TemporaryDirectory(prefix='oceanode-node-preview-') as tmp:
    (Path(tmp) / 'imconfig.h').write_text('')
    cpp = Path(tmp) / 'preview.cpp'
    cpp.write_text(fixture + method + main)
    exe = Path(tmp) / 'preview'
    subprocess.run(['clang++', '-std=c++17', '-I'+str(IMGUI), '-I'+tmp, str(cpp),
                    *[str(IMGUI / name) for name in ['imgui.cpp', 'imgui_draw.cpp', 'imgui_widgets.cpp', 'imgui_tables.cpp']],
                    '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: full-detail short/tall node previews fit, clips fit, shared state restored, and widgets stay inactive.')
