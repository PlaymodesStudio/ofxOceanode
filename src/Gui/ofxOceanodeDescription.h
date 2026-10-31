#pragma once

#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cfloat>
#include <string>
#include <utility>
#include <vector>

// Small, shared description format: **bold**, **[ Parameter]** labels and
// explicit paragraph breaks. Everything else is literal text, not Markdown.
// Measuring and drawing use the same layout, including mixed-font wrapping.
namespace ofxOceanodeDescription {
namespace detail {
struct Span {
    std::string text;
    bool bold = false;
    bool parameter = false;
};

inline std::vector<Span> parse(const std::string& text) {
    std::vector<Span> spans;
    size_t cursor = 0;
    while(cursor < text.size()) {
        const auto open = text.find("**", cursor);
        const auto close = open == std::string::npos ? std::string::npos : text.find("**", open + 2);
        // An unmatched marker stays visible instead of hiding the remaining text.
        if(close == std::string::npos) {
            spans.push_back({text.substr(cursor), false, false});
            break;
        }
        if(open > cursor) spans.push_back({text.substr(cursor, open - cursor), false, false});
        auto label = text.substr(open + 2, close - open - 2);
        const auto first = label.find_first_not_of(" \t");
        const auto last = label.find_last_not_of(" \t");
        const bool parameter = first != std::string::npos && label[first] == '[' && label[last] == ']';
        spans.push_back({std::move(label), true, parameter});
        cursor = close + 2;
    }
    return spans;
}

struct Glyph {
    std::string text;
    ImFont* font;
    float width;
    bool parameter;
    bool space;
    bool newline;
    size_t span;
};
struct Fragment {
    std::string text;
    ImFont* font;
    ImVec2 position;
    float width;
    bool parameter;
};
struct Layout {
    std::vector<Fragment> fragments;
    ImVec2 size{0, 0};
};

inline Layout layout(const std::string& text, float width, ImFont* boldFont) {
    Layout result;
    if(text.empty()) return result;
    const float lineHeight = ImGui::GetTextLineHeight();
    const float fontSize = ImGui::GetFontSize();
    width = std::max(1.0f, width);
    std::vector<Glyph> glyphs;
    size_t spanIndex = 0;
    for(const auto& span : parse(text)) {
        ++spanIndex;
        ImFont* font = span.bold && boldFont ? boldFont : ImGui::GetFont();
        const char* end = span.text.data() + span.text.size();
        for(const char* p = span.text.data(); p < end; ) {
            unsigned int codepoint;
            const int bytes = ImTextCharFromUtf8(&codepoint, p, end);
            if(bytes <= 0) break;
            if(codepoint != '\r') {
                const std::string value = codepoint == '\t' ? "    " : std::string(p, bytes);
                const bool newline = codepoint == '\n';
                const float advance = newline ? 0 : font->CalcTextSizeA(fontSize, FLT_MAX, 0,
                    value.data(), value.data() + value.size()).x;
                glyphs.push_back({value, font, advance, span.parameter,
                    codepoint == ' ' || codepoint == '\t', newline, spanIndex});
            }
            p += bytes;
        }
    }

    float x = 0, y = 0;
    bool wrapped = false;
    auto nextLine = [&]() {
        result.size.x = std::max(result.size.x, x);
        x = 0;
        y += lineHeight;
    };
    for(size_t i = 0; i < glyphs.size(); ++i) {
        const auto& glyph = glyphs[i];
        if(glyph.newline) { nextLine(); wrapped = false; continue; }
        if(glyph.space && wrapped && x == 0) continue;
        // A label may contain spaces. Keep the complete label together when
        // it fits on a fresh line; very long labels can still wrap safely.
        if(glyph.parameter && (i == 0 || glyphs[i - 1].span != glyph.span)) {
            float badgeWidth = 0;
            for(size_t j = i; j < glyphs.size() && glyphs[j].span == glyph.span && !glyphs[j].newline; ++j)
                badgeWidth += glyphs[j].width;
            if(badgeWidth <= width && x > 0 && x + badgeWidth > width) { nextLine(); wrapped = true; }
        }
        // Measure whole words across style boundaries: "**Gate**:" must wrap
        // as a single word. Oversized words split only at UTF-8 boundaries.
        if(!glyph.space && (i == 0 || glyphs[i - 1].space || glyphs[i - 1].newline)) {
            float wordWidth = 0;
            for(size_t j = i; j < glyphs.size() && !glyphs[j].space && !glyphs[j].newline; ++j)
                wordWidth += glyphs[j].width;
            if(x > 0 && x + wordWidth > width) { nextLine(); wrapped = true; }
        }
        if(x > 0 && x + glyph.width > width) {
            nextLine();
            wrapped = true;
            if(glyph.space) continue;
        }
        if(!result.fragments.empty() && result.fragments.back().font == glyph.font &&
           result.fragments.back().parameter == glyph.parameter && result.fragments.back().position.y == y) {
            result.fragments.back().text += glyph.text;
            result.fragments.back().width += glyph.width;
        } else {
            result.fragments.push_back({glyph.text, glyph.font, ImVec2(x, y), glyph.width, glyph.parameter});
        }
        x += glyph.width;
    }
    result.size.x = std::max(result.size.x, x);
    result.size.y = y + lineHeight;
    return result;
}
} // namespace detail

inline ImVec2 measure(const std::string& text, float width, ImFont* boldFont = nullptr) {
    if(text.empty()) return ImVec2(0, 0);
    if(text.find("**") == std::string::npos)
        return ImGui::CalcTextSize(text.data(), text.data() + text.size(), false, std::max(1.0f, width));
    return detail::layout(text, width, boldFont).size;
}

inline void draw(const std::string& text, ImFont* boldFont = nullptr) {
    // Retain ImGui's fast path and exact layout for existing plain descriptions.
    if(text.find("**") == std::string::npos) {
        ImGui::TextWrapped("%s", text.c_str());
        return;
    }
    if(ImGui::GetCurrentWindow()->SkipItems) return;
    const auto layout = detail::layout(text, ImGui::GetContentRegionAvail().x, boldFont);
    const auto origin = ImGui::GetCursorScreenPos();
    ImGui::Dummy(layout.size);
    if(!ImGui::IsItemVisible()) return;
    auto* drawList = ImGui::GetWindowDrawList();
    const auto textColor = ImGui::GetColorU32(ImGuiCol_Text);
    for(const auto& fragment : layout.fragments) {
        const ImVec2 position(origin.x + fragment.position.x, origin.y + fragment.position.y);
        drawList->AddText(fragment.font, ImGui::GetFontSize(), position, textColor,
            fragment.text.data(), fragment.text.data() + fragment.text.size());
    }
}
} // namespace ofxOceanodeDescription
