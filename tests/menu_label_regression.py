#!/usr/bin/env python3
"""Exercise responsive top-level menus with real ImGui, without a graphics host."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
IMGUI = Path(os.environ.get('OCEANODE_TEST_IMGUI_SRC', ROOT.parent / 'ofxImGui/libs/imgui/src'))

CASES = r'''
#include "ofxOceanodeMenuLabel.h"
#include "imgui_internal.h"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace ofxOceanodeMenuLabel;
int main(){
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1800, 700);
    io.DeltaTime = 1.0f / 60;
    auto* regular = io.Fonts->AddFontDefault();
    // Separate faces, with deliberately different metrics to catch measuring
    // regular text while drawing bold. No installed system fonts required.
    ImFontConfig config;
    config.GlyphExtraAdvanceX = 2.0f;
    auto* bold = io.Fonts->AddFontDefault(&config);
    io.FontDefault = regular;
    unsigned char* pixels; int w, h;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    const std::vector<std::string> labels = {"Presets", "Edit", "SuperCollider", "MIDI", "Config", "Window", "Layouts", "Help"};
    ImGuiID superColliderId = 0;
    // Alternate wide/narrow widths to exercise resize recovery immediately.
    for(float available : {1400.f, 550.f, 1400.f, 300.f, 800.f, 180.f}){
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(1750, 600));
        ImGui::Begin("Host", nullptr, ImGuiWindowFlags_MenuBar);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 6));
        assert(ImGui::BeginMenuBar());
        const float base = ImGui::GetStyle().FontSizeBase;
        ImGui::PushFont(bold, 0.0f);
        float full = labels.size() * 18.0f, maxWidth = 0;
        for(const auto& label : labels){
            const float width = ImGui::CalcTextSize(label.c_str()).x;
            full += width;
            maxWidth = std::max(maxWidth, width);
        }
        assert(widthLimit(labels, full, bold, 9) == maxWidth);
        assert(widthLimit(labels, full + 500, bold, 9) == maxWidth);
        assert(widthLimit(labels, full - 1, bold, 9) < maxWidth);
        assert(abbreviated("SuperCollider", maxWidth) == "SuperCollider###SuperCollider");
        assert(abbreviated("SuperCollider", ImGui::CalcTextSize("SuperC...").x) == "SuperC...###SuperCollider");
        const float limit = widthLimit(labels, available, bold, 9);
        const float boldSize = ImGui::GetFontSize();
        std::vector<std::string> visibleLabels;
        for(const auto& label : labels){
            visibleLabels.push_back(abbreviated(label, limit));
            assert(ImGui::CalcTextSize(visibleLabels.back().c_str(), nullptr, true).x <= limit + 0.01f);
            if(full <= available) assert(visibleLabels.back() == label + "###" + label);
        }
        // An abbreviated UTF-8 prefix must end on a whole code point.
        const auto unicode = abbreviated(u8"Échelle角度", 45);
        const char* cursor = unicode.c_str();
        while(*cursor){
            unsigned int c;
            const int bytes = ImTextCharFromUtf8(&c, cursor, nullptr);
            assert(bytes > 0 && c != 0xFFFD);
            cursor += bytes;
        }
        ImGui::PopFont();
        const float start = ImGui::GetCursorScreenPos().x;
        const int fontDepth = GImGui->FontStack.Size;
        int labelIndex = 0;
        for(const auto& label : labels){
            auto* host = ImGui::GetCurrentWindow();
            const ImGuiID id = host->GetID(("###" + label).c_str());
            if(label == "SuperCollider"){
                if(superColliderId) assert(superColliderId == id);
                superColliderId = id;
                // Opening programmatically also checks popup body restoration.
                ImGui::OpenPopup(id);
            }
            const int vertexStart = host->DrawList->VtxBuffer.Size;
            const bool open = begin(label.c_str(), bold, limit);
            assert(ImGui::GetFont() == regular);
            assert(ImGui::GetStyle().FontSizeBase == base);
            assert(GImGui->FontStack.Size == fontDepth);
            assert(ImGui::GetStyle().ItemSpacing.x == 6);
            assert(GImGui->LastItemData.ID == id);
            if(!host->SkipItems){
                const int vertices = host->DrawList->VtxBuffer.Size - vertexStart;
                const auto& visible = visibleLabels[labelIndex];
                const char* end = ImGui::FindRenderedTextEnd(visible.c_str());
                assert(vertices >= (end - visible.c_str()) * 4);
                // Inspect actual text geometry to catch accidental font scaling.
                const int textStart = host->DrawList->VtxBuffer.Size - (end - visible.c_str()) * 4;
                for(int v = textStart; v < host->DrawList->VtxBuffer.Size; v += 4){
                    const float glyphHeight = host->DrawList->VtxBuffer[v + 2].pos.y - host->DrawList->VtxBuffer[v].pos.y;
                    const int character = (v - textStart) / 4;
                    const auto* glyph = bold->GetFontBaked(boldSize)->FindGlyph(visible[character]);
                    assert(std::abs(glyphHeight - (glyph->Y1 - glyph->Y0)) < 0.01f);
                }
            }
            if(open){
                ImGui::MenuItem("Normal dropdown size");
                ImGui::CloseCurrentPopup();
                ImGui::EndMenu();
            }
            ++labelIndex;
        }
        const float advance = ImGui::GetCursorScreenPos().x - start;
        assert(advance <= available + 0.01f);
        ImGui::EndMenuBar();
        ImGui::PopStyleVar();
        ImGui::End();
        ImGui::Render();
    }
    ImGui::DestroyContext();
    std::cout << "PASS: fixed-size bold labels, ellipsis on overflow, exact-fit threshold, UTF-8, resize bounds, stable IDs and normal dropdown fonts\n";
}
'''

with tempfile.TemporaryDirectory(prefix='oceanode-menu-label-') as temp:
    work = Path(temp)
    (work / 'imconfig.h').write_text('')
    source = work / 'menu_label.cpp'
    source.write_text(CASES)
    binary = work / 'menu_label'
    subprocess.run([os.environ.get('CXX', 'clang++'), '-std=c++17', '-Wall', '-Wextra',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    '-I' + str(IMGUI), '-I' + str(ROOT / 'src/Gui'), '-I' + temp, str(source),
                    *[str(IMGUI / name) for name in ('imgui.cpp', 'imgui_draw.cpp', 'imgui_widgets.cpp', 'imgui_tables.cpp')],
                    '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
