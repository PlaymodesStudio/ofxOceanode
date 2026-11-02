//
//  ofxOceanodeTheme.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 13/04/2020.
//
//  Colour values are baked from the "remake_darkie_2" theme so a fresh app with
//  no config/themes folder starts with it. Regenerate with scripts/bake_theme.py.
//

#include "ofxOceanodeTheme.h"

void OceanodeTheme::setup() {
    ImGuiStyle* style = &ImGui::GetStyle();
    ImVec4* colors = style->Colors;
    // Hide the docking-menu/collapse icon and reclaim its title-bar space.
    style->WindowMenuButtonPosition = ImGuiDir_None;
    // Wider gap between docked windows; painted with OceanodeColors::DockGutter.
    style->DockingSeparatorSize = 4.0f;
    
    colors[ImGuiCol_Text]                      = ImVec4(0.675f, 0.675f, 0.675f, 1.000f);
    colors[ImGuiCol_TextDisabled]              = ImVec4(0.476f, 0.474f, 0.474f, 1.000f);
    colors[ImGuiCol_WindowBg]                  = ImVec4(0.120f, 0.120f, 0.120f, 1.000f);
    colors[ImGuiCol_ChildBg]                   = ImVec4(1.000f, 1.000f, 1.000f, 0.000f);
    colors[ImGuiCol_PopupBg]                   = colors[ImGuiCol_WindowBg]; // WindowBg controls both
    colors[ImGuiCol_Border]                    = ImVec4(0.100f, 0.100f, 0.100f, 1.000f);
    colors[ImGuiCol_BorderShadow]              = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
    colors[ImGuiCol_FrameBg]                   = ImVec4(0.366f, 0.366f, 0.366f, 0.250f);
    colors[ImGuiCol_FrameBgHovered]            = ImVec4(0.050f, 0.050f, 0.050f, 0.250f);
    colors[ImGuiCol_FrameBgActive]             = ImVec4(0.050f, 0.050f, 0.050f, 0.250f);
    colors[ImGuiCol_TitleBg]                   = ImVec4(0.099f, 0.099f, 0.099f, 1.000f);
    colors[ImGuiCol_TitleBgActive]             = ImVec4(0.099f, 0.099f, 0.099f, 1.000f);
    colors[ImGuiCol_TitleBgCollapsed]          = ImVec4(0.500f, 0.500f, 0.500f, 1.000f);
    colors[ImGuiCol_MenuBarBg]                 = ImVec4(0.000f, 0.000f, 0.000f, 1.000f);
    colors[ImGuiCol_ScrollbarBg]               = ImVec4(0.050f, 0.050f, 0.050f, 1.000f);
    colors[ImGuiCol_ScrollbarGrab]             = ImVec4(0.188f, 0.188f, 0.188f, 1.000f);
    colors[ImGuiCol_ScrollbarGrabHovered]      = ImVec4(0.410f, 0.410f, 0.410f, 1.000f);
    colors[ImGuiCol_ScrollbarGrabActive]       = ImVec4(0.510f, 0.510f, 0.510f, 1.000f);
    colors[ImGuiCol_CheckMark]                 = ImVec4(0.850f, 0.850f, 0.850f, 1.000f);
    colors[ImGuiCol_SliderGrab]                = ImVec4(0.623f, 0.298f, 0.042f, 1.000f);
    colors[ImGuiCol_SliderGrabActive]          = ImVec4(0.984f, 0.554f, 0.170f, 1.000f);
    colors[ImGuiCol_Button]                    = ImVec4(0.215f, 0.215f, 0.215f, 0.743f);
    colors[ImGuiCol_ButtonHovered]             = ImVec4(0.649f, 0.315f, 0.112f, 1.000f);
    colors[ImGuiCol_ButtonActive]              = ImVec4(1.000f, 0.504f, 0.204f, 0.948f);
    colors[ImGuiCol_Header]                    = ImVec4(0.377f, 0.213f, 0.085f, 1.000f);
    colors[ImGuiCol_HeaderHovered]             = ImVec4(0.351f, 0.181f, 0.079f, 1.000f);
    colors[ImGuiCol_HeaderActive]              = ImVec4(0.859f, 0.433f, 0.175f, 0.492f);
    colors[ImGuiCol_Separator]                 = ImVec4(0.251f, 0.251f, 0.251f, 1.000f);
    colors[ImGuiCol_SeparatorHovered]          = ImVec4(0.750f, 0.750f, 0.750f, 1.000f);
    colors[ImGuiCol_SeparatorActive]           = ImVec4(0.750f, 0.750f, 0.750f, 1.000f);
    colors[ImGuiCol_ResizeGrip]                = ImVec4(0.750f, 0.250f, 0.050f, 0.250f);
    colors[ImGuiCol_ResizeGripHovered]         = ImVec4(0.750f, 0.250f, 0.050f, 0.250f);
    colors[ImGuiCol_ResizeGripActive]          = ImVec4(0.980f, 0.554f, 0.260f, 1.000f);
    colors[ImGuiCol_InputTextCursor]           = ImVec4(1.000f, 1.000f, 1.000f, 1.000f);
    colors[ImGuiCol_TabHovered]                = ImVec4(1.000f, 0.471f, 0.000f, 0.513f);
    colors[ImGuiCol_Tab]                       = ImVec4(0.070f, 0.070f, 0.070f, 1.000f);
    colors[ImGuiCol_TabSelected]               = ImVec4(0.550f, 0.210f, 0.100f, 1.000f);
    colors[ImGuiCol_TabSelectedOverline]       = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
    colors[ImGuiCol_TabDimmed]                 = ImVec4(0.070f, 0.070f, 0.070f, 1.000f);
    colors[ImGuiCol_TabDimmedSelected]         = ImVec4(0.356f, 0.177f, 0.037f, 1.000f);
    colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
    colors[ImGuiCol_DockingPreview]            = ImVec4(0.513f, 0.341f, 0.223f, 1.000f);
    colors[ImGuiCol_DockingEmptyBg]            = ImVec4(0.000f, 0.000f, 0.000f, 1.000f);
    colors[ImGuiCol_PlotLines]                 = ImVec4(0.084f, 1.000f, 0.000f, 1.000f);
    colors[ImGuiCol_PlotLinesHovered]          = ImVec4(1.000f, 0.000f, 0.911f, 1.000f);
    colors[ImGuiCol_PlotHistogram]             = ImVec4(1.000f, 0.000f, 0.911f, 1.000f);
    colors[ImGuiCol_PlotHistogramHovered]      = ImVec4(1.000f, 0.408f, 0.000f, 1.000f);
    colors[ImGuiCol_TableHeaderBg]             = ImVec4(0.529f, 0.272f, 0.116f, 1.000f);
    colors[ImGuiCol_TableBorderStrong]         = ImVec4(0.310f, 0.310f, 0.350f, 1.000f);
    colors[ImGuiCol_TableBorderLight]          = ImVec4(0.230f, 0.230f, 0.250f, 1.000f);
    colors[ImGuiCol_TableRowBg]                = ImVec4(0.136f, 0.136f, 0.136f, 0.948f);
    colors[ImGuiCol_TableRowBgAlt]             = ImVec4(1.000f, 1.000f, 1.000f, 0.060f);
    colors[ImGuiCol_TextLink]                  = ImVec4(0.980f, 0.599f, 0.260f, 1.000f);
    colors[ImGuiCol_TextSelectedBg]            = ImVec4(1.000f, 0.809f, 0.560f, 0.350f);
    colors[ImGuiCol_TreeLines]                 = ImVec4(1.000f, 0.514f, 0.178f, 0.927f);
    colors[ImGuiCol_DragDropTarget]            = ImVec4(1.000f, 1.000f, 0.000f, 0.900f);
    colors[ImGuiCol_DragDropTargetBg]          = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
    colors[ImGuiCol_UnsavedMarker]             = ImVec4(1.000f, 0.000f, 0.000f, 1.000f);
    colors[ImGuiCol_NavCursor]                 = ImVec4(0.272f, 1.000f, 0.000f, 1.000f);
    colors[ImGuiCol_NavWindowingHighlight]     = ImVec4(0.000f, 0.000f, 0.000f, 1.000f);
    colors[ImGuiCol_NavWindowingDimBg]         = ImVec4(0.932f, 1.000f, 0.000f, 1.000f);
    colors[ImGuiCol_ModalWindowDimBg]          = ImVec4(0.188f, 0.188f, 0.188f, 0.571f);

    // Reset all custom Oceanode semantic colours to their built-in defaults
    OceanodeColors::reset();
}

