#!/usr/bin/env python3
"""Exercise the shared description renderer with real ImGui, without a GPU."""
from pathlib import Path
import json
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
IMGUI = Path(os.environ.get('OCEANODE_TEST_IMGUI_SRC', ROOT.parent / 'ofxImGui/libs/imgui/src'))

CASES = r'''
#include "ofxOceanodeDescription.h"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace ofxOceanodeDescription;
CATALOG_FIXTURES
std::string visible(const detail::Layout& layout) {
    std::string text;
    for(const auto& fragment : layout.fragments) text += fragment.text;
    return text;
}
int main() {
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(900, 700);
    io.DeltaTime = 1.0f / 60;
    auto* regular = io.Fonts->AddFontDefault();
    // A separate face exercises actual font switching and metrics.
    ImFontConfig config; config.SizePixels = 20;
    auto* bold = io.Fonts->AddFontDefault(&config);
    io.FontDefault = regular;
    unsigned char* pixels; int w, h;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    for(int frame = 0; frame < 2; ++frame) {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(800, 650));
        ImGui::Begin("Description");
        const float height = ImGui::GetTextLineHeight();
        assert(measure("", 200, bold).y == 0);
        assert(measure("First\n\nSecond", 500, bold).y == height * 3);
        assert(measure("First\r\nSecond", 500, bold).y == height * 2);
        assert(visible(detail::layout("Literal **unmatched", 500, bold)) == "Literal **unmatched");
        assert(visible(detail::layout("100% **bold** ##text", 500, bold)) == "100% bold ##text");
        auto spans = detail::parse("**Inputs**\n**[ In.X] ** X\n**[ In.Y]** Y");
        assert(spans[0].bold && !spans[0].parameter);
        assert(spans[2].parameter && spans[2].text == "[ In.X] ");
        assert(spans[4].parameter && spans[4].text == "[ In.Y]");
        auto mixed = detail::layout("Regular **bold** tail", 500, bold);
        assert(mixed.fragments.size() == 3);
        assert(mixed.fragments[0].font == regular);
        assert(mixed.fragments[1].font == bold);
        assert(mixed.fragments[2].font == regular);
        // Punctuation attached to bold text stays with its word when wrapping.
        const float prefix = measure("Prefix ", 500, bold).x;
        const float word = measure("**Gate**:", 500, bold).x;
        auto wrapped = detail::layout("Prefix **Gate**:", prefix + word - 1, bold);
        assert(wrapped.fragments[1].position.x == 0);
        assert(wrapped.fragments[1].position.y == height);
        assert(wrapped.fragments[2].position.y == height);
        const float badgeWidth = measure("**[ In.X]**", 500, bold).x;
        auto badge = detail::layout("Prefix **[ In.X]** X", prefix + badgeWidth - 1, bold);
        assert(badge.fragments[1].text == "[ In.X]");
        assert(badge.fragments[1].position.x == 0 && badge.fragments[1].position.y == height);
        assert(measure("**Bold**", 500, nullptr).x == measure("Bold", 500, nullptr).x);
        // Exercise the authored descriptions, including long parameter labels,
        // at Inspector, Node Pool and Help-like column widths.
        for(const auto& sample : catalogFixtures) {
            for(float width : {80.f, 250.f, 600.f}) {
                const auto layout = detail::layout(sample, width, bold);
                assert(layout.size.y > 0 && layout.size.x <= width + 0.01f);
                for(const auto& fragment : layout.fragments) {
                    assert(fragment.position.x >= 0);
                    assert(fragment.position.x + fragment.width <= width + 0.01f);
                    assert(fragment.position.y + height <= layout.size.y + 0.01f);
                }
            }
        }
        for(float width : {1.f, 40.f, 90.f, 200.f, 600.f}) {
            auto unicode = detail::layout(u8"**[ Échelle]** café → 角度", width, bold);
            assert(unicode.size.y >= height);
            // Validate every fragment as complete UTF-8, even with forced breaks.
            for(const auto& f : unicode.fragments) {
                const char* p = f.text.data();
                const char* end = p + f.text.size();
                while(p < end) {
                    unsigned int c;
                    const int bytes = ImTextCharFromUtf8(&c, p, end);
                    assert(bytes > 0 && c != 0xFFFD);
                    p += bytes;
                }
            }
            ImGui::PushID(static_cast<int>(width));
            ImGui::BeginChild("Narrow", ImVec2(width + 20, 100));
            const std::string text = "**Inputs**\n\n**[ In.X]** Coordinate vector.\n\n**[ In.Y]** Coordinate vector.";
            const auto expected = measure(text, ImGui::GetContentRegionAvail().x, bold);
            const auto origin = ImGui::GetCursorScreenPos();
            auto* drawList = ImGui::GetWindowDrawList();
            const int first = drawList->VtxBuffer.Size;
            const int fonts = GImGui->FontStack.Size;
            draw(text, bold);
            assert(GImGui->FontStack.Size == fonts && ImGui::GetFont() == regular);
            assert(std::abs(ImGui::GetItemRectSize().y - expected.y) < 0.01f);
            assert(std::abs(ImGui::GetItemRectMin().y - origin.y) < 0.01f);
            for(int i = first; i < drawList->VtxBuffer.Size; ++i)
                assert(drawList->VtxBuffer[i].col == ImGui::GetColorU32(ImGuiCol_Text));
            ImGui::EndChild();
            ImGui::PopID();
        }
        ImGui::End();
        ImGui::Render();
    }
    ImGui::DestroyContext();
    std::cout << "PASS: bold faces, uncolored parameter labels, paragraphs, UTF-8, wrapping and measured/drawn heights; "
              << catalogFixtures.size() << " catalog descriptions at three widths\n";
}
'''

with tempfile.TemporaryDirectory(prefix='oceanode-description-') as temp:
    work = Path(temp)
    (work / 'imconfig.h').write_text('')
    source = work / 'description.cpp'
    catalog = json.loads((ROOT / 'docs/node-help-catalog.json').read_text())
    fixtures = [entry['description'] for entry in catalog.values() if '**' in entry['description']]
    fixture_code = 'const std::vector<std::string> catalogFixtures = {' + ',\n'.join(
        json.dumps(description, ensure_ascii=False) for description in fixtures) + '};'
    source.write_text(CASES.replace('CATALOG_FIXTURES', fixture_code))
    binary = work / 'description'
    subprocess.run([os.environ.get('CXX', 'clang++'), '-std=c++17', '-Wall', '-Wextra',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    '-I' + str(IMGUI), '-I' + str(ROOT / 'src/Gui'), '-I' + temp, str(source),
                    *[str(IMGUI / name) for name in ('imgui.cpp', 'imgui_draw.cpp', 'imgui_widgets.cpp', 'imgui_tables.cpp')],
                    '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
