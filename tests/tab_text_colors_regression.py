#!/usr/bin/env python3
"""Verify Oceanode-only tab text colors, docking focus, and theme persistence.

Uses unmodified sibling ImGui, OceanodeTheme, and extracted production save/load
methods, with filesystem/OF stand-ins. No graphics backend or live host app.
"""
from pathlib import Path
import os
import subprocess
import tempfile
from new_preset_regression import block

ROOT = Path(__file__).resolve().parents[1]
IMGUI = Path(os.environ.get("OCEANODE_TEST_IMGUI_SRC", str(ROOT.parent / "ofxImGui/libs/imgui/src")))

CASES = r'''
#include "imgui.h"
#include "imgui_internal.h"
#include "ofxOceanodeTheme.h"
#include "ofxOceanodeThemeColorPicker.h"
#include "ofxOceanodeTabText.h"
#include <nlohmann/json.hpp>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
using namespace std;
using ofJson = nlohmann::json;
string ofToDataPath(const string& path, bool){ return path; }
struct ofFile { string path; ofFile(string p):path(p){} bool exists(){ return filesystem::exists(path); } };
struct ofDirectory { static void createDirectory(const string& p, bool, bool){ filesystem::create_directories(p); } };
ofJson ofLoadJson(const string& p){ ifstream f(p); return ofJson::parse(f); }
void ofSavePrettyJson(const string& p, const ofJson& j){ ofstream(p) << j.dump(2); }
struct Log { template<class T> Log& operator<<(const T&){ return *this; } };
Log ofLogNotice(const char*){ return {}; } Log ofLogWarning(const char*){ return {}; }
const char* themesFolder = "config/themes/";
struct ThemeHarness {
    string themeFontFamily, currentThemeName;
    bool pendingThemeFontChange = false;
    void save(const string& name);
    void load(const string& name);
};
PRODUCTION

ImGuiID dockID = 0;
ImRect regularRects[2];
ImGuiTabBar* regularBar = nullptr;
bool probeGeometry = false;
int probeBegin = 0, probeEnd = 0;
std::vector<ImDrawVert> probeVertices;
void frame(int selectRegular = -1){
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0)); ImGui::SetNextWindowSize(ImVec2(600, 380));
    ImGui::Begin("DockHost", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    dockID = ImGui::GetID("Dock");
    ImGui::DockSpace(dockID);
    ImGui::End();
    for(const char* name : {"Panel A", "Panel B"}){
        ImGui::SetNextWindowDockID(dockID, ImGuiCond_Always);
        bool open = true;
        ImGui::Begin(name, &open); ImGui::TextUnformatted("Ordinary content text"); ImGui::End();
    }
    ImGui::SetNextWindowPos(ImVec2(0, 400)); ImGui::SetNextWindowSize(ImVec2(600, 200));
    ImGui::Begin("RegularTabs");
    if(ImGui::BeginTabBar("Regular")){
        regularBar = GImGui->CurrentTabBar;
        int i = 0;
        for(const char* name : {"First", "Second"}){
            bool visible = ImGui::BeginTabItem(name, nullptr, selectRegular == i ? ImGuiTabItemFlags_SetSelected : 0);
            regularRects[i++] = GImGui->LastItemData.Rect;
            if(visible){ ImGui::TextUnformatted("Ordinary tab content"); ImGui::EndTabItem(); }
        }
        ImGui::EndTabBar();
    }
    if(probeGeometry){
        auto* draw = ImGui::GetWindowDrawList();
        probeBegin = draw->VtxBuffer.Size;
        const auto p = regularRects[0].Min;
        const ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);
        draw->AddRectFilled(p, ImVec2(p.x + 12, p.y + 12), textColor);
        draw->AddLine(p, ImVec2(p.x + 12, p.y + 12), textColor, 1.0f);
        draw->AddImage(ImTextureRef((ImTextureID)42), p, ImVec2(p.x + 12, p.y + 12),
                       ImVec2(0, 0), ImVec2(1, 1), textColor);
        probeEnd = draw->VtxBuffer.Size;
        probeVertices.assign(draw->VtxBuffer.Data + probeBegin, draw->VtxBuffer.Data + probeEnd);
    }
    ImGui::End();
    ImGui::Render();
    assert(GImGui->ColorStack.empty());
}

int countColor(const ImDrawList* list, const ImRect& rect, ImU32 color){
    int count = 0;
    for(const auto& v : list->VtxBuffer) if(rect.Contains(v.pos) && v.col == color) ++count;
    return count;
}
void checkDock(){
    auto* node = ImGui::DockBuilderGetNode(dockID);
    assert(node && node->TabBar && node->HostWindow);
    for(const char* name : {"Panel A", "Panel B"}){
        auto* window = ImGui::FindWindowByName(name);
        bool selected = node->TabBar->VisibleTabId == window->TabId;
        ImU32 expected = ImGui::GetColorU32(selected ? OceanodeColors::TabSelectedText : OceanodeColors::TabText);
        ImU32 other = ImGui::GetColorU32(selected ? OceanodeColors::TabText : OceanodeColors::TabSelectedText);
        assert(countColor(node->HostWindow->DrawList, window->DC.DockTabItemRect, expected) > 0);
        assert(countColor(node->HostWindow->DrawList, window->DC.DockTabItemRect, other) == 0);
    }
}
void checkRegular(int selected){
    const auto* list = ImGui::FindWindowByName("RegularTabs")->DrawList;
    for(int i = 0; i < 2; ++i){
        ImU32 expected = ImGui::GetColorU32(i == selected ? OceanodeColors::TabSelectedText : OceanodeColors::TabText);
        assert(countColor(list, regularRects[i], expected) > 0);
    }
    // The label override must not affect text in the tab's contents.
    ImRect body = ImGui::FindWindowByName("RegularTabs")->InnerRect;
    body.Min.y = regularRects[0].Max.y + 1;
    assert(countColor(list, body, ImGui::GetColorU32(ImGuiCol_Text)) > 0);
    assert(countColor(list, body, ImGui::GetColorU32(OceanodeColors::TabSelectedText)) == 0);
    assert(countColor(list, body, ImGui::GetColorU32(OceanodeColors::TabText)) == 0);
}

int main(){
    ImGui::CreateContext();
    auto& io = ImGui::GetIO(); io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.DisplaySize = ImVec2(800, 700); io.DeltaTime = 1.0f / 60;
    unsigned char* pixels; int w, h; io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    auto& colors = ImGui::GetStyle().Colors;
    // Oceanode installs its own hook without extending the ImGui color enum.
    for(int i = 0; i < ImGuiCol_COUNT; ++i){
        assert(string(ImGui::GetStyleColorName(i)) != "TabText");
        assert(string(ImGui::GetStyleColorName(i)) != "TabSelectedText");
    }
    ofxOceanodeTabText::install(); ofxOceanodeTabText::install();
    int hooks = 0;
    for(const auto& hook : GImGui->Hooks) if(hook.Owner == ImHashStr("OceanodeTabText")) ++hooks;
    assert(hooks == 1);
    OceanodeTheme theme; theme.setup();
    assert(OceanodeColors::TabText.x == colors[ImGuiCol_TextDisabled].x);
    assert(OceanodeColors::TabSelectedText.x == colors[ImGuiCol_Text].x);
    OceanodeColors::TabSelectedText = ImVec4(0.13f, 0.91f, 0.23f, 0.8f);
    OceanodeColors::TabText = ImVec4(0.89f, 0.17f, 0.71f, 0.6f);
    frame(0); frame(0); frame(0); checkDock(); checkRegular(0);
    auto* node = ImGui::DockBuilderGetNode(dockID);
    auto* panelA = ImGui::FindWindowByName("Panel A");
    auto* panelB = ImGui::FindWindowByName("Panel B");
    node->TabBar->NextSelectedTabId = panelA->TabId;
    frame(1); frame(1); checkDock(); checkRegular(1);
    assert(node->TabBar->VisibleTabId == panelA->TabId);
    // Move focus outside the dock group: its visible tab keeps its color.
    ImGui::FocusWindow(ImGui::FindWindowByName("RegularTabs"));
    frame(); checkDock(); checkRegular(1);
    assert(!(node->TabBar->Flags & ImGuiTabBarFlags_IsFocused));
    // Hover the unselected dock tab, then switch which content is visible.
    const auto hover = panelB->DC.DockTabItemRect.GetCenter();
    io.AddMousePosEvent(hover.x, hover.y); frame(); frame(); checkDock();
    node->TabBar->NextSelectedTabId = panelB->TabId;
    frame(); checkDock(); assert(node->TabBar->VisibleTabId == panelB->TabId);
    // Source text alpha is replaced, while global opacity is preserved.
    colors[ImGuiCol_Text].w = 0.5f;
    ImGui::GetStyle().Alpha = 0.5f;
    frame(); frame(); checkDock(); checkRegular(1);
    colors[ImGuiCol_Text].w = 1.0f; ImGui::GetStyle().Alpha = 1.0f;
    // Other geometry within tab bounds must stay byte-for-byte unchanged.
    probeGeometry = true; frame();
    const auto& vertices = ImGui::FindWindowByName("RegularTabs")->DrawList->VtxBuffer;
    for(int i = probeBegin; i < probeEnd; ++i) assert(vertices[i].col == probeVertices[i - probeBegin].col);
    probeGeometry = false;
    // Alt+hover lookup resolves the Oceanode names automatically.
    auto matches = ofxOceanodeThemeColorPicker::match(ImGui::GetColorU32(OceanodeColors::TabText));
    assert(matches.size() == 1 && matches[0].name == "OC_TabText" && matches[0].oceanode);
    // JSON persistence executes the real ThemeManager methods.
    const ImU32 selected = ImGui::ColorConvertFloat4ToU32(OceanodeColors::TabSelectedText);
    const ImU32 unselected = ImGui::ColorConvertFloat4ToU32(OceanodeColors::TabText);
    ThemeHarness manager; manager.save("tabs");
    theme.setup(); manager.load("tabs");
    assert(ImGui::ColorConvertFloat4ToU32(OceanodeColors::TabSelectedText) == selected);
    assert(ImGui::ColorConvertFloat4ToU32(OceanodeColors::TabText) == unselected);
    auto old = ofLoadJson("config/themes/tabs.json");
    assert(!old["colors"].contains("TabText") && !old["colors"].contains("TabSelectedText"));
    assert(old["oceanodeColors"].contains("OC_TabText") && old["oceanodeColors"].contains("OC_TabSelectedText"));
    old["oceanodeColors"].erase("OC_TabText"); old["oceanodeColors"].erase("OC_TabSelectedText");
    old["colors"]["Text"] = {0.7f, 0.6f, 0.5f, 1.0f};
    old["colors"]["TextDisabled"] = {0.4f, 0.3f, 0.2f, 1.0f};
    ofSavePrettyJson("config/themes/old.json", old); manager.load("old");
    assert(OceanodeColors::TabSelectedText.x == 0.7f && OceanodeColors::TabText.x == 0.4f);
    // Partially updated themes retain explicit colors and derive only the missing one.
    old["colors"]["TabText"] = {0.2f, 0.8f, 0.9f, 0.5f};
    ofSavePrettyJson("config/themes/partial.json", old); manager.load("partial");
    assert(OceanodeColors::TabSelectedText.x == 0.7f && OceanodeColors::TabText.x == 0.2f);
    old["colors"]["TabSelectedText"] = {0.6f, 0.7f, 0.8f, 0.9f};
    ofSavePrettyJson("config/themes/legacy.json", old); manager.load("legacy");
    assert(OceanodeColors::TabSelectedText.x == 0.6f && OceanodeColors::TabText.x == 0.2f);
    // Explicit Oceanode fields override legacy ImGui values when both exist.
    old["oceanodeColors"]["OC_TabText"] = {0.1f, 0.2f, 0.3f, 0.4f};
    old["oceanodeColors"]["OC_TabSelectedText"] = {0.9f, 0.8f, 0.7f, 0.6f};
    ofSavePrettyJson("config/themes/mixed.json", old); manager.load("mixed");
    assert(OceanodeColors::TabText.x == 0.1f && OceanodeColors::TabSelectedText.x == 0.9f);
    theme.setup();
    assert(OceanodeColors::TabText.x == 0.55f && OceanodeColors::TabSelectedText.x == 0.85f);
    ImGui::DestroyContext();
    puts("PASS: regular/docking tab colors, selection, unfocused groups, hover, alpha, balanced style, picker, theme save/load, old and partial themes.");
}
'''


def main():
    source = (ROOT / "src/Gui/ofxOceanodeThemeManager.cpp").read_text()
    assert "ofxOceanodeTabText::install();" in block(source, "void ofxOceanodeThemeManager::setup(")
    production = "\n".join(block(source, f"void ofxOceanodeThemeManager::{method}(const std::string& name)")
                           .replace("ofxOceanodeThemeManager::", "ThemeHarness::") for method in ("save", "load"))
    with tempfile.TemporaryDirectory(prefix="oceanode-tab-text-") as folder:
        work = Path(folder)
        (work / "imconfig.h").write_text("// Standalone ImGui.\n")
        (work / "BaseTheme.h").write_text('#pragma once\n#include "imgui.h"\nnamespace ofxImGui { struct BaseTheme { virtual void setup() = 0; }; }\n')
        cpp = work / "tabs.cpp"
        cpp.write_text(CASES.replace("PRODUCTION", production))
        binary = work / "tabs"
        subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++17", "-O0", "-g",
                        "-fsanitize=address,undefined", "-I" + str(IMGUI), "-I" + str(work),
                        "-I" + str(ROOT / "src"), "-I" + str(ROOT / "src/Gui"),
                        "-I" + str(ROOT.parents[1] / "libs/json/include"), str(cpp),
                        str(ROOT / "src/Gui/ofxOceanodeTheme.cpp"),
                        str(ROOT / "src/Gui/ofxOceanodeThemeColorPicker.cpp"),
                        *[str(IMGUI / name) for name in ("imgui.cpp", "imgui_draw.cpp", "imgui_widgets.cpp", "imgui_tables.cpp")],
                        "-o", str(binary)], check=True)
        subprocess.run([str(binary)], cwd=work, check=True)


if __name__ == "__main__":
    main()
