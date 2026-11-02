//
//  ofxOceanodeColors.h
//  ofxOceanode
//
//  Mutable semantic colour palette for ofxOceanode.
//  All colours are runtime-editable inline static ImVec4 values so they can
//  be shown in the Theme Editor, saved with saveTheme() and loaded with
//  loadTheme().
//
//  Usage in draw / GUI code:
//    • ImGui::PushStyleColor(ImGuiCol_Text, OceanodeColors::CanvasInfoText);
//    • draw_list->AddLine(a, b, OceanodeColors::U32(OceanodeColors::GridLine));
//
//  OceanodeColors::reset()     — restore all values to built-in defaults.
//  OceanodeColors::getFields() — iterate {name, ptr} pairs for save/load/editor.
//

#ifndef ofxOceanodeColors_h
#define ofxOceanodeColors_h

#include "imgui.h"
#include <vector>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// OceanodeColors — all custom semantic colours in one place
// ─────────────────────────────────────────────────────────────────────────────
struct OceanodeColors {

    // ── Node appearance ──────────────────────────────────────────────────────
    inline static ImVec4 NodeBg               = {0.118f, 0.118f, 0.118f, 1.000f}; // node body fill
    inline static ImVec4 ConnectionBullet     = {0.000f, 0.000f, 0.000f, 1.000f}; // input/output pin dots
    inline static ImVec4 SelectedBorder       = {1.000f, 0.503f, 0.000f, 1.000f}; // selected comment/node border

    // ── Connection lines ─────────────────────────────────────────────────────
    inline static ImVec4 ConnectionLineLight          = {0.000f, 0.000f, 0.000f, 0.712f};  // wire on a light canvas
    inline static ImVec4 ConnectionLineDark           = {1.000f, 1.000f, 1.000f, 0.220f};  // wire on a dark canvas
    inline static ImVec4 ConnectionDragging           = {0.634f, 0.634f, 0.634f, 1.000f}; // dragging, no valid target
    inline static ImVec4 ConnectionDraggingReachable  = {0.274f, 0.602f, 0.329f, 0.502f}; // dragging, valid target reachable

    // ── Canvas grid ──────────────────────────────────────────────────────────
    inline static ImVec4 GridLine             = {0.157f, 0.157f, 0.157f, 0.524f}; // fine grid lines
    inline static ImVec4 GridCenter           = {0.162f, 0.162f, 0.162f, 1.000f}; // center cross lines

    // ── Canvas overlay text ──────────────────────────────────────────────────
    inline static ImVec4 CanvasInfoText       = {1.000f, 1.000f, 1.000f, 0.500f};  // position readout
    inline static ImVec4 FpsGood              = {0.000f, 1.000f, 0.000f, 0.500f};  // FPS >= 60
    inline static ImVec4 FpsWarning           = {1.000f, 0.500f, 0.000f, 0.500f};  // FPS > 30
    inline static ImVec4 FpsBad              = {1.000f, 0.000f, 0.000f, 0.500f};  // FPS <= 30

    // ── Selection rectangles ─────────────────────────────────────────────────
    inline static ImVec4 SelectionRectEntire    = {1.000f, 0.498f, 0.000f, 0.118f}; // top→bottom (full enclosure)
    inline static ImVec4 SelectionRectIntersect = {0.000f, 0.490f, 1.000f, 0.118f}; // bottom→top (intersection)

    // ── App-level UI ─────────────────────────────────────────────────────────
    inline static ImVec4 ActiveItemHighlight  = {0.000f, 1.000f, 0.000f, 1.000f}; // "currently selected" green tick

    // ── Popups & context menus ───────────────────────────────────────────────
    // Legacy field kept for source compatibility; popup backgrounds use ImGuiCol_WindowBg.
    inline static ImVec4 PopupBg             = {0.12f, 0.12f, 0.12f, 1.00f};
    inline static ImVec4 PopupDimmedText     = {1.000f, 1.000f, 1.000f, 0.733f}; // dimmed text in context popups
    inline static ImVec4 TransparentButton   = {0.000f, 0.000f, 0.000f, 0.000f}; // invisible-background buttons

    // ── Search & filtering ───────────────────────────────────────────────────
    inline static ImVec4 SearchMatchHighlight  = {1.000f, 0.863f, 0.000f, 0.176f}; // row highlight for search match
    inline static ImVec4 SearchSelectedHeader  = {0.430f, 0.190f, 0.000f, 0.800f};  // selected result header
    inline static ImVec4 SearchSelectedHovered = {0.860f, 0.380f, 0.000f, 0.600f};  // selected result header hover
    inline static ImVec4 SearchSelectedActive  = {0.800f, 0.300f, 0.000f, 0.600f};  // selected result header pressed

    // ── Nodes list tree view ─────────────────────────────────────────────────
    inline static ImVec4 SelectedNodeText     = {1.000f, 0.346f, 0.000f, 0.393f};  // selected node label (orange)
    inline static ImVec4 ZebraEven            = {1.000f, 1.000f, 1.000f, 0.052f}; // even-row alternating stripe
    inline static ImVec4 ZebraOdd             = {0.000f, 0.000f, 0.000f, 0.031f}; // odd-row alternating stripe

    // ── Snapshot matrix (MacroGui) ────────────────────────────────────────────
    inline static ImVec4 SnapshotActive         = {0.400f, 0.000f, 0.000f, 1.000f}; // active slot button
    inline static ImVec4 SnapshotActiveHovered  = {0.700f, 0.000f, 0.000f, 1.000f};
    inline static ImVec4 SnapshotActivePressed  = {1.000f, 0.000f, 0.000f, 1.000f};
    inline static ImVec4 SnapshotFilled         = {0.200f, 0.400f, 0.000f, 1.000f}; // slot has stored data
    inline static ImVec4 SnapshotFilledHovered  = {0.250f, 0.500f, 0.000f, 1.000f};
    inline static ImVec4 SnapshotFilledPressed  = {0.300f, 0.600f, 0.000f, 1.000f};
    inline static ImVec4 MorphProgressBar       = {0.800f, 0.400f, 0.100f, 1.000f}; // morph progress bar

    // ── Hierarchy controller ─────────────────────────────────────────────────
    inline static ImVec4 HierarchyLineActive    = {0.863f, 0.863f, 0.863f, 1.000f}; // connector: active node pair
    inline static ImVec4 HierarchyLineInactive  = {0.000f, 0.000f, 0.000f, 1.000f}; // connector: inactive node
    inline static ImVec4 HierarchyActiveBorder  = {0.000f, 0.824f, 0.824f, 1.000f}; // active canvas cyan border
    inline static ImVec4 HierarchySelectedBorder= {1.000f, 0.549f, 0.000f, 1.000f}; // selected-node orange border
    inline static ImVec4 HierarchyRootFill      = {0.824f, 0.824f, 0.824f, 1.000f}; // root canvas box fill (light)
    inline static ImVec4 HierarchyRootBorder    = {1.000f, 1.000f, 1.000f, 1.000f}; // root canvas box border (white)

    // ── MiniMap ───────────────────────────────────────────────────────────────
    inline static ImVec4 MiniMapBg             = {0.173f, 0.173f, 0.173f, 0.863f}; // minimap panel background
    inline static ImVec4 MiniMapViewportFill   = {1.000f, 1.000f, 1.000f, 0.078f}; // viewport rect fill
    inline static ImVec4 MiniMapViewportBorder = {1.000f, 1.000f, 1.000f, 0.627f}; // viewport rect border

    // ── Curve editors (curve.cpp & curve2.cpp) ────────────────────────────────
    inline static ImVec4 CurveEditorBg         = {0.196f, 0.196f, 0.196f, 0.784f}; // curve editor child background
    inline static ImVec4 CurveGridLine         = {0.353f, 0.353f, 0.353f, 0.196f}; // grid lines inside curve editor
    inline static ImVec4 CurveExtensionLine    = {0.039f, 0.039f, 0.039f, 1.000f};  // flat extension beyond endpoints
    inline static ImVec4 CurveControlPointLine = {0.059f, 0.059f, 0.059f, 1.000f};  // Bezier handle lines
    inline static ImVec4 CurvePointDot         = {0.000f, 0.000f, 0.000f, 1.000f};  // vertex dot
    inline static ImVec4 CurveCP1              = {0.000f, 1.000f, 1.000f, 0.498f}; // control point 1 (cyan)
    inline static ImVec4 CurveCP2              = {1.000f, 0.000f, 1.000f, 0.498f}; // control point 2 (magenta)
    inline static ImVec4 CurveInputIndicator   = {0.498f, 0.498f, 0.498f, 0.498f}; // current-value vertical line
    inline static ImVec4 CurveLabelBg          = {0.000f, 0.000f, 0.000f, 0.471f}; // background behind curve name
    inline static ImVec4 CurvePointHoverHighlight = {1.000f, 1.000f, 1.000f, 1.000f}; // hovered point white highlight
    // Semantic guide lines for curve2 (distinct colours for visual differentiation)
    inline static ImVec4 CurveInflectionGuide  = {1.000f, 1.000f, 0.000f, 0.251f}; // inflection guide (yellow)
    inline static ImVec4 CurveAsymmetryGuide   = {0.000f, 1.000f, 1.000f, 0.251f}; // asymmetry guide (cyan)
    inline static ImVec4 CurveBParameterGuide  = {1.000f, 0.000f, 0.000f, 0.251f}; // B-parameter guide (red)

    // ── Floating windows & popups (ofxOceanodeFloatingWindowDecor.h) ──────────
    inline static ImVec4 FloatingWindowBorder  = {0.209f, 0.209f, 0.209f, 1.000f};  // outline of undocked windows and popups
    inline static ImVec4 FloatingWindowShadow  = {0.000f, 0.000f, 0.000f, 0.382f};  // shared grey shadow (alpha = strength)

    // ── Docking ──────────────────────────────────────────────────────────────
    inline static ImVec4 DockGutter            = {0.070f, 0.070f, 0.070f, 1.000f};  // gap between neighbouring docked windows
    inline static ImVec4 TabText               = {0.482f, 0.482f, 0.482f, 1.000f};  // unselected tab label
    inline static ImVec4 TabSelectedText       = {0.859f, 0.836f, 0.836f, 1.000f};  // visible tab label, regardless of focus

    // ─────────────────────────────────────────────────────────────────────────
    // Helpers
    // ─────────────────────────────────────────────────────────────────────────

    /// Convert ImVec4 → ImU32 for ImDrawList calls (AddLine, AddRect, etc.)
    static ImU32 U32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }

    /// Restore all colours to their built-in defaults.
    static void reset() {
        NodeBg               = {0.118f, 0.118f, 0.118f, 1.000f};
        ConnectionBullet     = {0.000f, 0.000f, 0.000f, 1.000f};
        SelectedBorder       = {1.000f, 0.503f, 0.000f, 1.000f};

        ConnectionLineLight          = {0.000f, 0.000f, 0.000f, 0.712f};
        ConnectionLineDark           = {1.000f, 1.000f, 1.000f, 0.220f};
        ConnectionDragging           = {0.634f, 0.634f, 0.634f, 1.000f};
        ConnectionDraggingReachable  = {0.274f, 0.602f, 0.329f, 0.502f};

        GridLine             = {0.157f, 0.157f, 0.157f, 0.524f};
        GridCenter           = {0.162f, 0.162f, 0.162f, 1.000f};

        CanvasInfoText       = {1.000f, 1.000f, 1.000f, 0.500f};
        FpsGood              = {0.000f, 1.000f, 0.000f, 0.500f};
        FpsWarning           = {1.000f, 0.500f, 0.000f, 0.500f};
        FpsBad              = {1.000f, 0.000f, 0.000f, 0.500f};

        SelectionRectEntire    = {1.000f, 0.498f, 0.000f, 0.118f};
        SelectionRectIntersect = {0.000f, 0.490f, 1.000f, 0.118f};

        ActiveItemHighlight  = {0.000f, 1.000f, 0.000f, 1.000f};

        PopupBg             = {0.12f, 0.12f, 0.12f, 1.00f};
        PopupDimmedText     = {1.000f, 1.000f, 1.000f, 0.733f};
        TransparentButton   = {0.000f, 0.000f, 0.000f, 0.000f};

        SearchMatchHighlight  = {1.000f, 0.863f, 0.000f, 0.176f};
        SearchSelectedHeader  = {0.430f, 0.190f, 0.000f, 0.800f};
        SearchSelectedHovered = {0.860f, 0.380f, 0.000f, 0.600f};
        SearchSelectedActive  = {0.800f, 0.300f, 0.000f, 0.600f};

        SelectedNodeText     = {1.000f, 0.346f, 0.000f, 0.393f};
        ZebraEven            = {1.000f, 1.000f, 1.000f, 0.052f};
        ZebraOdd             = {0.000f, 0.000f, 0.000f, 0.031f};

        SnapshotActive         = {0.400f, 0.000f, 0.000f, 1.000f};
        SnapshotActiveHovered  = {0.700f, 0.000f, 0.000f, 1.000f};
        SnapshotActivePressed  = {1.000f, 0.000f, 0.000f, 1.000f};
        SnapshotFilled         = {0.200f, 0.400f, 0.000f, 1.000f};
        SnapshotFilledHovered  = {0.250f, 0.500f, 0.000f, 1.000f};
        SnapshotFilledPressed  = {0.300f, 0.600f, 0.000f, 1.000f};
        MorphProgressBar       = {0.800f, 0.400f, 0.100f, 1.000f};

        HierarchyLineActive    = {0.863f, 0.863f, 0.863f, 1.000f};
        HierarchyLineInactive  = {0.000f, 0.000f, 0.000f, 1.000f};
        HierarchyActiveBorder  = {0.000f, 0.824f, 0.824f, 1.000f};
        HierarchySelectedBorder= {1.000f, 0.549f, 0.000f, 1.000f};
        HierarchyRootFill      = {0.824f, 0.824f, 0.824f, 1.000f};
        HierarchyRootBorder    = {1.000f, 1.000f, 1.000f, 1.000f};

        MiniMapBg             = {0.173f, 0.173f, 0.173f, 0.863f};
        MiniMapViewportFill   = {1.000f, 1.000f, 1.000f, 0.078f};
        MiniMapViewportBorder = {1.000f, 1.000f, 1.000f, 0.627f};

        CurveEditorBg         = {0.196f, 0.196f, 0.196f, 0.784f};
        CurveGridLine         = {0.353f, 0.353f, 0.353f, 0.196f};
        CurveExtensionLine    = {0.039f, 0.039f, 0.039f, 1.000f};
        CurveControlPointLine = {0.059f, 0.059f, 0.059f, 1.000f};
        CurvePointDot         = {0.000f, 0.000f, 0.000f, 1.000f};
        CurveCP1              = {0.000f, 1.000f, 1.000f, 0.498f};
        CurveCP2              = {1.000f, 0.000f, 1.000f, 0.498f};
        CurveInputIndicator   = {0.498f, 0.498f, 0.498f, 0.498f};
        CurveLabelBg          = {0.000f, 0.000f, 0.000f, 0.471f};
        CurvePointHoverHighlight = {1.000f, 1.000f, 1.000f, 1.000f};
        CurveInflectionGuide  = {1.000f, 1.000f, 0.000f, 0.251f};
        CurveAsymmetryGuide   = {0.000f, 1.000f, 1.000f, 0.251f};
        CurveBParameterGuide  = {1.000f, 0.000f, 0.000f, 0.251f};

        FloatingWindowBorder  = {0.209f, 0.209f, 0.209f, 1.000f};
        FloatingWindowShadow  = {0.000f, 0.000f, 0.000f, 0.382f};

        DockGutter            = {0.070f, 0.070f, 0.070f, 1.000f};
        TabText               = {0.482f, 0.482f, 0.482f, 1.000f};
        TabSelectedText       = {0.859f, 0.836f, 0.836f, 1.000f};
    }

    /// Returns all named colour fields for use by the theme editor / save / load.
    struct ColorField { const char* name; ImVec4* ptr; };
    static std::vector<ColorField> getFields() {
        return {
            // Node appearance
            { "OC_NodeBg",               &NodeBg               },
            { "OC_ConnectionBullet",     &ConnectionBullet     },
            { "OC_SelectedBorder",       &SelectedBorder       },
            // Connection lines
            { "OC_ConnectionLineLight",          &ConnectionLineLight          },
            { "OC_ConnectionLineDark",           &ConnectionLineDark           },
            { "OC_ConnectionDragging",           &ConnectionDragging           },
            { "OC_ConnectionDraggingReachable",  &ConnectionDraggingReachable  },
            // Canvas grid
            { "OC_GridLine",             &GridLine             },
            { "OC_GridCenter",           &GridCenter           },
            // Canvas overlay
            { "OC_CanvasInfoText",       &CanvasInfoText       },
            { "OC_FpsGood",              &FpsGood              },
            { "OC_FpsWarning",           &FpsWarning           },
            { "OC_FpsBad",              &FpsBad              },
            // Selection
            { "OC_SelectionRectEntire",    &SelectionRectEntire    },
            { "OC_SelectionRectIntersect", &SelectionRectIntersect },
            // App UI
            { "OC_ActiveItemHighlight",  &ActiveItemHighlight  },
            // Popups
            { "OC_PopupDimmedText",     &PopupDimmedText     },
            { "OC_TransparentButton",   &TransparentButton   },
            // Search
            { "OC_SearchMatchHighlight",  &SearchMatchHighlight  },
            { "OC_SearchSelectedHeader",  &SearchSelectedHeader  },
            { "OC_SearchSelectedHovered", &SearchSelectedHovered },
            { "OC_SearchSelectedActive",  &SearchSelectedActive  },
            // Nodes list
            { "OC_SelectedNodeText",     &SelectedNodeText     },
            { "OC_ZebraEven",            &ZebraEven            },
            { "OC_ZebraOdd",             &ZebraOdd             },
            // Snapshots
            { "OC_SnapshotActive",         &SnapshotActive         },
            { "OC_SnapshotActiveHovered",  &SnapshotActiveHovered  },
            { "OC_SnapshotActivePressed",  &SnapshotActivePressed  },
            { "OC_SnapshotFilled",         &SnapshotFilled         },
            { "OC_SnapshotFilledHovered",  &SnapshotFilledHovered  },
            { "OC_SnapshotFilledPressed",  &SnapshotFilledPressed  },
            { "OC_MorphProgressBar",       &MorphProgressBar       },
            // Hierarchy
            { "OC_HierarchyLineActive",    &HierarchyLineActive    },
            { "OC_HierarchyLineInactive",  &HierarchyLineInactive  },
            { "OC_HierarchyActiveBorder",  &HierarchyActiveBorder  },
            { "OC_HierarchySelectedBorder",&HierarchySelectedBorder},
            { "OC_HierarchyRootFill",      &HierarchyRootFill      },
            { "OC_HierarchyRootBorder",    &HierarchyRootBorder    },
            // MiniMap
            { "OC_MiniMapBg",             &MiniMapBg             },
            { "OC_MiniMapViewportFill",   &MiniMapViewportFill   },
            { "OC_MiniMapViewportBorder", &MiniMapViewportBorder },
            // Curve editor
            { "OC_CurveEditorBg",         &CurveEditorBg         },
            { "OC_CurveGridLine",         &CurveGridLine         },
            { "OC_CurveExtensionLine",    &CurveExtensionLine    },
            { "OC_CurveControlPointLine", &CurveControlPointLine },
            { "OC_CurvePointDot",         &CurvePointDot         },
            { "OC_CurveCP1",              &CurveCP1              },
            { "OC_CurveCP2",              &CurveCP2              },
            { "OC_CurveInputIndicator",   &CurveInputIndicator   },
            { "OC_CurveLabelBg",          &CurveLabelBg          },
            { "OC_CurvePointHoverHighlight", &CurvePointHoverHighlight },
            { "OC_CurveInflectionGuide",  &CurveInflectionGuide  },
            { "OC_CurveAsymmetryGuide",   &CurveAsymmetryGuide   },
            { "OC_CurveBParameterGuide",  &CurveBParameterGuide  },
            // Floating windows
            { "OC_FloatingWindowBorder",  &FloatingWindowBorder  },
            { "OC_FloatingWindowShadow",  &FloatingWindowShadow  },
            // Docking
            { "OC_DockGutter",            &DockGutter            },
            { "OC_TabText",               &TabText               },
            { "OC_TabSelectedText",       &TabSelectedText       },
        };
    }
};

#endif /* ofxOceanodeColors_h */
