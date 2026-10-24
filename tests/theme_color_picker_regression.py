#!/usr/bin/env python3
"""Find theme colours under the mouse in real rendered ImGui draw data.

Exercises ofxOceanodeThemeColorPicker (the Theme Editor's Alt+hover lookup)
with the sibling ofxImGui sources, without an OF host or graphics backend.
Also checks that ofxOceanode wires the lookup after rendering.
Requires Python 3 and clang++; ASan and UBSan are enabled.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
IMGUI = Path(os.environ.get("OCEANODE_TEST_IMGUI_SRC", str(ROOT.parent / "ofxImGui/libs/imgui/src")))

CASES = r'''
#include "ofxOceanodeThemeColorPicker.h"
#include <cassert>
#include <cstdio>

using Picker = ofxOceanodeThemeColorPicker;

ImVec2 buttonMin, buttonMax, lineY, imageMin;
const ImDrawList* tooltipList = nullptr;
bool showTooltip = false, disabled = false;

void frame() {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(600, 400));
    ImGui::Begin("Host", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetCursorScreenPos(ImVec2(50, 50));
    ImGui::BeginDisabled(disabled);
    ImGui::Button("Wide button label", ImVec2(200, 40));
    ImGui::EndDisabled();
    buttonMin = ImGui::GetItemRectMin(); buttonMax = ImGui::GetItemRectMax();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    // A 1px wire drawn with an Oceanode colour, like the canvas grid.
    draw->AddLine(ImVec2(50, 200), ImVec2(400, 200), OceanodeColors::U32(OceanodeColors::GridCenter));
    // A user image must be ignored even though its vertices are white.
    imageMin = ImVec2(450, 50);
    draw->AddImage(ImTextureRef((ImTextureID)1), imageMin, ImVec2(550, 150));
    ImGui::End();
    if(showTooltip) {
        ImGui::SetNextWindowPos(ImVec2(300, 300));
        ImGui::BeginTooltip();
        tooltipList = ImGui::GetWindowDrawList();
        ImGui::TextUnformatted("tooltip");
        ImGui::EndTooltip();
    }
    ImGui::Render();
}

bool has(const Picker::Layer& layer, bool oceanode, const char* name) {
    for(const auto& m : layer.matches) if(m.oceanode == oceanode && m.name == name) return true;
    return false;
}
const Picker::Layer* find(const std::vector<Picker::Layer>& layers, bool oceanode, const char* name) {
    for(const auto& layer : layers) if(has(layer, oceanode, name)) return &layer;
    return nullptr;
}

int main() {
    ImGui::CreateContext(); auto& io = ImGui::GetIO();
    io.IniFilename = nullptr; io.DisplaySize = ImVec2(800, 600); io.DeltaTime = 1.0f / 60;
    unsigned char* pixels; int w, h; io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    ImGuiStyle& style = ImGui::GetStyle();
    // Distinct values so every lookup has one answer.
    style.Colors[ImGuiCol_WindowBg]      = ImVec4(0.10f, 0.11f, 0.12f, 1.0f);
    style.Colors[ImGuiCol_PopupBg]       = style.Colors[ImGuiCol_WindowBg];
    style.Colors[ImGuiCol_Button]        = ImVec4(0.20f, 0.40f, 0.60f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.50f, 0.70f, 1.0f);
    style.Colors[ImGuiCol_Text]          = ImVec4(0.95f, 0.85f, 0.15f, 1.0f);
    OceanodeColors::GridCenter           = ImVec4(0.83f, 0.21f, 0.47f, 1.0f);
    frame(); frame();

    // Empty / invalid input.
    assert(Picker::pick(nullptr, ImVec2(0, 0)).empty());

    // Button body away from its label: Button over WindowBg (mouse not hovering).
    ImVec2 body(buttonMin.x + 4, buttonMin.y + 4);
    auto layers = Picker::pick(ImGui::GetDrawData(), body);
    assert(layers.size() >= 2);
    assert(has(layers[0], false, "Button"));
    assert(layers[0].exactAlpha);
    const auto* bg = find(layers, false, "WindowBg");
    assert(bg && bg != &layers[0]);
    assert(!find(layers, false, "PopupBg")); // hidden mirror of WindowBg

    // Hovered button switches colour.
    io.AddMousePosEvent(body.x, body.y); frame(); frame();
    layers = Picker::pick(ImGui::GetDrawData(), body);
    assert(has(layers[0], false, "ButtonHovered"));
    io.AddMousePosEvent(-FLT_MAX, -FLT_MAX); frame(); frame();

    // Label glyphs are on top of the button.
    bool foundText = false;
    for(float x = buttonMin.x; x < buttonMax.x && !foundText; x += 0.5f) {
        layers = Picker::pick(ImGui::GetDrawData(), ImVec2(x, (buttonMin.y + buttonMax.y) * 0.5f));
        foundText = !layers.empty() && has(layers[0], false, "Text");
        if(foundText) assert(find(layers, false, "Button"));
    }
    assert(foundText);

    // Thin lines are found within the tolerance, Oceanode colours included.
    layers = Picker::pick(ImGui::GetDrawData(), ImVec2(200, 201.5f));
    assert(!layers.empty() && has(layers[0], true, "OC_GridCenter"));
    layers = Picker::pick(ImGui::GetDrawData(), ImVec2(200, 210));
    assert(!find(layers, true, "OC_GridCenter"));

    // User images are skipped; the window background below them is found.
    layers = Picker::pick(ImGui::GetDrawData(), ImVec2(imageMin.x + 20, imageMin.y + 20));
    assert(!layers.empty() && has(layers[0], false, "WindowBg"));

    // Disabled drawing only changes alpha: matched by RGB, flagged inexact.
    disabled = true; frame(); frame();
    layers = Picker::pick(ImGui::GetDrawData(), body);
    assert(has(layers[0], false, "Button") && !layers[0].exactAlpha);
    disabled = false; frame();

    // Unknown colours are reported without matches; transparent ones never match.
    bool exact = false;
    assert(Picker::match(IM_COL32(1, 2, 3, 255), &exact).empty());
    OceanodeColors::TransparentButton = ImVec4(0, 0, 0, 0);
    for(const auto& m : Picker::match(IM_COL32(0, 0, 0, 255))) assert(m.name != "OC_TransparentButton");
    assert(Picker::hex(IM_COL32(0x12, 0xAB, 0xEF, 0x80)) == "#12ABEF80");

    // The lookup's own tooltip can be excluded.
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.05f, 0.60f, 0.25f, 1.0f);
    showTooltip = true; frame(); frame();
    const ImU32 popupBg = ImGui::GetColorU32(ImGuiCol_PopupBg);
    auto hasColor = [](const std::vector<Picker::Layer>& found, ImU32 color) {
        for(const auto& layer : found) if(layer.color == color) return true;
        return false;
    };
    ImVec2 tip(310, 310);
    assert(hasColor(Picker::pick(ImGui::GetDrawData(), tip), popupBg));
    auto withoutTooltip = Picker::pick(ImGui::GetDrawData(), tip, tooltipList);
    assert(!hasColor(withoutTooltip, popupBg) && find(withoutTooltip, false, "WindowBg"));

    // Layer count is capped and colours are distinct.
    layers = Picker::pick(ImGui::GetDrawData(), body, nullptr, 1.5f, 1);
    assert(layers.size() == 1);

    std::printf("PASS: button/hover/text layers, thin Oceanode line, images skipped, disabled alpha, unmatched and transparent colours, tooltip exclusion, layer cap.\n");
    ImGui::DestroyContext();
}
'''


def check_wiring():
    source = (ROOT / "src/ofxOceanode.cpp").read_text()
    draw = source[source.index("void ofxOceanode::draw(){"):source.index("void ofxOceanode::loadDefaultGUILayout()")]
    # The lookup reads rendered draw data, so it must follow gui.draw().
    assert draw.index("gui.draw();") < draw.index("pickThemeColorUnderMouse();")
    assert "drawThemeColorPickTooltip();" in draw
    pick = source[source.index("void ofxOceanode::pickThemeColorUnderMouse(){"):]
    pick = pick[:pick.index("\n}\n")]
    for needle in ("showThemeEditor", "io.KeyAlt", "themeEditorHovered", "themeColorPickTooltipDrawList"):
        assert needle in pick, needle


def main():
    check_wiring()
    with tempfile.TemporaryDirectory(prefix="oceanode-theme-color-picker-") as work:
        work = Path(work)
        (work / "imconfig.h").write_text("// Standalone ImGui without OF adapters.\n")
        cpp = work / "regressions.cpp"
        cpp.write_text(CASES)
        binary = work / "regressions"
        subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++17", "-O0", "-g",
                        "-fsanitize=address,undefined", "-I", str(IMGUI), "-I", str(work),
                        "-I", str(ROOT / "src"), "-I", str(ROOT / "src/Gui"), str(cpp),
                        str(ROOT / "src/Gui/ofxOceanodeThemeColorPicker.cpp"),
                        *[str(IMGUI / name) for name in
                          ("imgui.cpp", "imgui_draw.cpp", "imgui_tables.cpp", "imgui_widgets.cpp")],
                        "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
