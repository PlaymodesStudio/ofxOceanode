//
//  ofxOceanodeTheme.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 13/04/2020.
//

#ifndef ofxOceanodeTheme_h
#define ofxOceanodeTheme_h

#include "imgui.h"
#include "imgui_internal.h"

void StyleColorsOceanode(ImGuiStyle* dst = NULL)
{
    ImGuiStyle* style = dst ? dst : &ImGui::GetStyle();
    ImVec4* colors = style->Colors;
    
    colors[ImGuiCol_Text]                   = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_ChildBg]                = ImVec4(1.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_Border]                 = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.22f, 0.22f, 0.22f, 0.250f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.05f, 0.05f, 0.05f, 0.25f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.05f, 0.05f, 0.05f, 0.25f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark]              = colors[ImGuiCol_Text];
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.26f, 0.26f, 0.26f, 0.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.35f, 0.35f, 0.35f, 0.50f);
    colors[ImGuiCol_Header]                 = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.46f, 0.46f, 0.46f, 1.00f);
    colors[ImGuiCol_Separator]              = colors[ImGuiCol_Border];
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.75f, 0.75f, 0.75f, 0.75f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.75f, 0.75f, 0.75f, 0.85f);
    colors[ImGuiCol_ResizeGripActive]       = colors[ImGuiCol_TabHovered];
    colors[ImGuiCol_Tab]                    = ImVec4(0.07f, 0.07f, 0.07f, 1.00f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.80f, 0.41f, 0.1f, 1.00f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.55f, 0.21f, 0.1f, 1.00f);//ImLerp(colors[ImGuiCol_HeaderActive], colors[ImGuiCol_TitleBgActive], 0.60f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.07f, 0.07f, 0.07f, 1.00f);//ImLerp(colors[ImGuiCol_Tab],          colors[ImGuiCol_TitleBg], 0.80f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);//ImLerp(colors[ImGuiCol_TabActive],    colors[ImGuiCol_TitleBg], 0.40f);
    colors[ImGuiCol_DockingPreview]         = ImVec4(colors[ImGuiCol_HeaderActive].x, colors[ImGuiCol_HeaderActive].y,colors[ImGuiCol_HeaderActive].z, 0.7f);
    colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.56f, 1.00f, 1.00f, 0.35f);
    colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
}


#endif /* ofxOceanodeTheme_h */
