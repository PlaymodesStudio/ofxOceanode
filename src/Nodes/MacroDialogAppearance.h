#pragma once

#include "imgui.h"
#include "imgui_internal.h"
#include "ofxOceanodeColors.h"

namespace MacroDialogAppearance {

// Use the ordinary panel background, as in Nodes, while retaining the theme's
// controls, padding and text colors. Scope this before BeginPopupModal().
struct Style {
    Style() {
        // Canvas rendering temporarily tints ChildBg. Nodes uses the theme's
        // original ChildBg; recover it before applying the dialog's own scope.
        ImVec4 childBackground = ImGui::GetStyleColorVec4(ImGuiCol_ChildBg);
        for(const auto& color : ImGui::GetCurrentContext()->ColorStack) {
            if(color.Col == ImGuiCol_ChildBg) {
                childBackground = color.BackupValue;
                break;
            }
        }
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, childBackground);
    }
    ~Style() { ImGui::PopStyleColor(2); }
    Style(const Style&) = delete;
    Style& operator=(const Style&) = delete;
};

// Stripe every visible row across the full panel, including beneath indentation.
// Separate draw channels keep stripes behind text, input fields and selection.
class Rows {
public:
    Rows() : drawList(ImGui::GetWindowDrawList()), text(ImGui::GetStyleColorVec4(ImGuiCol_Text)) {
        drawList->ChannelsSplit(2);
        drawList->ChannelsSetCurrent(1);
    }
    ~Rows() { drawList->ChannelsMerge(); }
    Rows(const Rows&) = delete;
    Rows& operator=(const Rows&) = delete;

    void begin(bool selected = false, bool dimmed = false) {
        ImVec4 color = selected ? OceanodeColors::SelectedNodeText : text;
        if(dimmed && !selected) {
            color.x *= .75f;
            color.y *= .75f;
            color.z *= .75f;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, color);
    }

    void end() {
        const ImVec2 minimum(ImGui::GetWindowPos().x, ImGui::GetItemRectMin().y);
        const ImVec2 maximum(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetItemRectMax().y);
        if(ImGui::IsRectVisible(minimum, maximum)) {
            drawList->ChannelsSetCurrent(0);
            drawList->AddRectFilled(minimum, maximum, OceanodeColors::U32(
                index % 2 == 0 ? OceanodeColors::ZebraEven : OceanodeColors::ZebraOdd));
            drawList->ChannelsSetCurrent(1);
        }
        ++index;
        ImGui::PopStyleColor();
    }

private:
    ImDrawList* drawList;
    ImVec4 text;
    int index = 0;
};

} // namespace MacroDialogAppearance
