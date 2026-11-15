#!/usr/bin/env python3
"""Exercise the production save dialog with real ImGui, without writing presets.

Checks direct replacement, duplicate-name selection, cancellation, new saves,
and centering after content/window size changes. Save persistence is covered by
preset_duplicate_names_regression.py; this intercepts the save pipeline entry.
"""
from pathlib import Path
import os
import subprocess
import tempfile

from new_preset_regression import block

ROOT = Path(__file__).resolve().parents[1]
IMGUI = Path(os.environ.get("OCEANODE_TEST_IMGUI_SRC", ROOT.parent / "ofxImGui/libs/imgui/src"))

FIXTURE = r'''
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
using namespace std;
string sanitizePresetName(string name){replace(name.begin(), name.end(), ' ', '_');return name;}
template<class T> string ofToString(T v){return to_string(v);}
struct ofxOceanodeShared {
    static string getCurrentPresetName(){return "Existing";}
    static string getCurrentBankName(){return "Bank";}
};
map<string, ImVec2> buttons;
bool trackedButton(const char* label, ImVec2 size=ImVec2(0,0)){
    bool clicked=ImGui::Button(label,size);
    buttons[label]=ImGui::GetItemRectMin()+ImGui::GetItemRectSize()*0.5f;
    return clicked;
}
struct ofxOceanodePresetsController {
    enum class PopupRequest {None,NewBank,SaveAs,Delete,ReviewSave,UnsavedChanges,SaveResult};
    PopupRequest popupRequest=PopupRequest::None;
    vector<string> banks{"Bank"};
    map<string, vector<string>> bankPresets{{"Bank", {"Existing", "Other", "Existing"}}};
    map<string,int> currentPreset{{"Bank",2}};
    int currentBank=0,saveAsBank=0,saveAsReplacementIndex=-1,deletePresetIndex=-1;
    char bankNameBuffer[256]={},presetNameBuffer[256]={};
    string lastPresetName="Existing",deleteBankName,deletePresetName,saveResultText;
    bool presetNameCursorPending=false,savingBeforeAction=false,actionApproved=false;
    int saves=0,cancels=0,savedIndex=-2; string savedName,savedBank;
    void beginSavePreset(string name,string bank,int index){
        ++saves;savedName=name;savedBank=bank;savedIndex=index;
        popupRequest=PopupRequest::ReviewSave;
    }
    void cancelPendingAction(){++cancels;savingBeforeAction=false;actionApproved=false;}
    void deletePreset(int,const string&){}
    void drawGlobalMacroSaveReview(){
        if(ImGui::BeginPopupModal("Review Global Macros",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
            ImGui::TextUnformatted("Review reached");ImGui::EndPopup();
        }
    }
    string presetNameAt(int index,const string& bankName) const;
    vector<int> saveAsReplacementTargets(const string& bank,const string& name) const;
    void drawPopups();
};
PRODUCTION
void frame(ofxOceanodePresetsController& p){
    ImGui::NewFrame();ImGui::Begin("Host");p.drawPopups();ImGui::End();ImGui::Render();
}
void idle(ofxOceanodePresetsController& p){for(int i=0;i<4;++i)frame(p);}
void centered(){
    auto* w=ImGui::FindWindowByName("Save Preset As");assert(w&&w->Active);
    auto center=ImGui::GetMainViewport()->GetCenter();
    assert(abs(w->Pos.x+w->Size.x*.5f-center.x)<=1.f);
    assert(abs(w->Pos.y+w->Size.y*.5f-center.y)<=1.f);
    assert(w->Viewport==ImGui::GetMainViewport());
}
void click(ofxOceanodePresetsController& p,const char* label){
    auto& io=ImGui::GetIO();auto pos=buttons.at(label);
    io.AddMousePosEvent(pos.x,pos.y);frame(p);
    io.AddMouseButtonEvent(0,true);frame(p);
    io.AddMouseButtonEvent(0,false);frame(p);
}
void setup(){
    buttons.clear();
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;
    io.DisplaySize=ImVec2(1400,900);io.DeltaTime=1.f/60;
    unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
}
int main(){
    for(int scenario=0;scenario<5;++scenario){
        setup();ofxOceanodePresetsController p;
        p.popupRequest=ofxOceanodePresetsController::PopupRequest::SaveAs;
        p.savingBeforeAction=true;idle(p);centered();
        assert(p.saveAsReplacementIndex==2); // Prefer the loaded duplicate.
        if(scenario==0){
            // Change content height, then resize the app while the form is open.
            p.bankPresets["Bank"].push_back("New");
            ImGui::ClearActiveID();
            snprintf(p.presetNameBuffer,sizeof(p.presetNameBuffer),"New");
            idle(p);centered();
            ImGui::GetIO().DisplaySize=ImVec2(1000,650);idle(p);centered();
            snprintf(p.presetNameBuffer,sizeof(p.presetNameBuffer),"Existing");idle(p);
            p.saveAsReplacementIndex=0;idle(p);centered();
        }
        if(scenario==2){
            ImGui::ClearActiveID();
            snprintf(p.presetNameBuffer,sizeof(p.presetNameBuffer),"Fresh");idle(p);
        }
        if(scenario==3)click(p,"Cancel");
        else if(scenario==4){
            ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape,true);frame(p);
            ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape,false);frame(p);
        }else click(p,scenario==2?"Save":"Replace");
        idle(p);
        assert(!ImGui::FindWindowByName("Save Preset As")->Active);
        auto* extra=ImGui::FindWindowByName("Replace Preset?");assert(!extra||!extra->Active);
        if(scenario>=3){assert(p.saves==0&&p.cancels==1&&!p.savingBeforeAction);}
        else{
            assert(p.saves==1&&p.savedBank=="Bank"&&p.cancels==0);
            assert(p.savedIndex==(scenario==2?-1:scenario==0?0:2));
            assert(p.savedName==(scenario==2?"Fresh":""));
            assert(p.savingBeforeAction);
            assert(ImGui::FindWindowByName("Review Global Macros")->Active);
        }
        ImGui::DestroyContext();
    }
    puts("PASS: direct replacement, duplicate targets, new save, Cancel/Escape, macro-review handoff and dynamic centering");
}
'''


def main():
    source = (ROOT / "src/Controls/ofxOceanodePresetsController.cpp").read_text()
    production = "\n".join(block(source, signature) for signature in (
        "int moveCursorToEnd(",
        "string ofxOceanodePresetsController::presetNameAt(",
        "vector<int> ofxOceanodePresetsController::saveAsReplacementTargets(",
        "void ofxOceanodePresetsController::drawPopups()",
    )).replace("ImGui::Button(", "trackedButton(")
    with tempfile.TemporaryDirectory(prefix="oceanode-save-dialog-") as directory:
        work = Path(directory)
        (work / "imconfig.h").write_text("// Standalone ImGui.\n")
        cpp = work / "test.cpp"
        cpp.write_text(FIXTURE.replace("PRODUCTION", production))
        binary = work / "test"
        cmd = [os.environ.get("CXX", "clang++"), "-std=c++17", "-DIMGUI_DEFINE_MATH_OPERATORS",
               "-I" + str(work), "-I" + str(IMGUI), str(cpp)]
        cmd += [str(IMGUI / name) for name in ("imgui.cpp", "imgui_draw.cpp", "imgui_tables.cpp", "imgui_widgets.cpp")]
        subprocess.run(cmd + ["-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
