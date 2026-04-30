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
    inline static ImVec4 NodeBg               = {0.157f, 0.157f, 0.157f, 1.00f}; // node body fill
    inline static ImVec4 ConnectionBullet     = {0.00f,  0.00f,  0.00f,  1.00f}; // input/output pin dots
    inline static ImVec4 SelectedBorder       = {1.00f,  0.498f, 0.00f,  1.00f}; // selected comment/node border

    // ── Connection lines ─────────────────────────────────────────────────────
    inline static ImVec4 ConnectionLine               = {0.784f, 0.784f, 0.784f, 0.502f}; // idle Bezier wire
    inline static ImVec4 ConnectionDragging           = {1.00f,  1.00f,  1.00f,  0.251f}; // dragging, no valid target
    inline static ImVec4 ConnectionDraggingReachable  = {1.00f,  1.00f,  1.00f,  0.502f}; // dragging, valid target reachable

    // ── Canvas grid ──────────────────────────────────────────────────────────
    inline static ImVec4 GridLine             = {0.353f, 0.353f, 0.353f, 0.157f}; // fine grid lines
    inline static ImVec4 GridCenter           = {0.118f, 0.118f, 0.118f, 0.314f}; // center cross lines

    // ── Canvas overlay text ──────────────────────────────────────────────────
    inline static ImVec4 CanvasInfoText       = {1.00f, 1.00f, 1.00f, 0.50f};  // position readout
    inline static ImVec4 FpsGood              = {0.00f, 1.00f, 0.00f, 0.50f};  // FPS >= 60
    inline static ImVec4 FpsWarning           = {1.00f, 0.50f, 0.00f, 0.50f};  // FPS > 30
    inline static ImVec4 FpsBad              = {1.00f, 0.00f, 0.00f, 0.50f};  // FPS <= 30

    // ── Selection rectangles ─────────────────────────────────────────────────
    inline static ImVec4 SelectionRectEntire    = {1.00f, 0.498f, 0.00f, 0.118f}; // top→bottom (full enclosure)
    inline static ImVec4 SelectionRectIntersect = {0.00f, 0.490f, 1.00f, 0.118f}; // bottom→top (intersection)

    // ── App-level UI ─────────────────────────────────────────────────────────
    inline static ImVec4 ActiveItemHighlight  = {0.40f, 0.90f, 0.40f, 1.00f}; // "currently selected" green tick

    // ── Popups & context menus ───────────────────────────────────────────────
    inline static ImVec4 PopupBg             = {0.12f, 0.12f, 0.12f, 1.00f}; // darker sub-popup background
    inline static ImVec4 PopupDimmedText     = {0.70f, 0.70f, 0.70f, 0.70f}; // dimmed text in context popups
    inline static ImVec4 TransparentButton   = {0.00f, 0.00f, 0.00f, 0.00f}; // invisible-background buttons

    // ── Search & filtering ───────────────────────────────────────────────────
    inline static ImVec4 SearchMatchHighlight  = {1.00f, 0.863f, 0.00f, 0.176f}; // row highlight for search match
    inline static ImVec4 SearchSelectedHeader  = {0.43f, 0.19f,  0.00f, 0.80f};  // selected result header
    inline static ImVec4 SearchSelectedHovered = {0.86f, 0.38f,  0.00f, 0.60f};  // selected result header hover
    inline static ImVec4 SearchSelectedActive  = {0.80f, 0.30f,  0.00f, 0.60f};  // selected result header pressed

    // ── Nodes list tree view ─────────────────────────────────────────────────
    inline static ImVec4 SelectedNodeText     = {1.00f, 0.55f,  0.00f, 1.00f};  // selected node label (orange)
    inline static ImVec4 ZebraEven            = {1.00f, 1.00f,  1.00f, 0.031f}; // even-row alternating stripe
    inline static ImVec4 ZebraOdd             = {0.00f, 0.00f,  0.00f, 0.031f}; // odd-row alternating stripe

    // ── Snapshot matrix (MacroGui) ────────────────────────────────────────────
    inline static ImVec4 SnapshotActive         = {0.40f, 0.00f, 0.00f, 1.00f}; // active slot button
    inline static ImVec4 SnapshotActiveHovered  = {0.70f, 0.00f, 0.00f, 1.00f};
    inline static ImVec4 SnapshotActivePressed  = {1.00f, 0.00f, 0.00f, 1.00f};
    inline static ImVec4 SnapshotFilled         = {0.20f, 0.40f, 0.00f, 1.00f}; // slot has stored data
    inline static ImVec4 SnapshotFilledHovered  = {0.25f, 0.50f, 0.00f, 1.00f};
    inline static ImVec4 SnapshotFilledPressed  = {0.30f, 0.60f, 0.00f, 1.00f};
    inline static ImVec4 MorphProgressBar       = {0.80f, 0.40f, 0.10f, 1.00f}; // morph progress bar

    // ── Hierarchy controller ─────────────────────────────────────────────────
    inline static ImVec4 HierarchyLineActive    = {0.863f, 0.863f, 0.863f, 1.00f}; // connector: active node pair
    inline static ImVec4 HierarchyLineInactive  = {0.00f,  0.00f,  0.00f,  1.00f}; // connector: inactive node
    inline static ImVec4 HierarchyActiveBorder  = {0.00f,  0.824f, 0.824f, 1.00f}; // active canvas cyan border
    inline static ImVec4 HierarchySelectedBorder= {1.00f,  0.549f, 0.00f,  1.00f}; // selected-node orange border
    inline static ImVec4 HierarchyRootFill      = {0.824f, 0.824f, 0.824f, 1.00f}; // root canvas box fill (light)
    inline static ImVec4 HierarchyRootBorder    = {1.00f,  1.00f,  1.00f,  1.00f}; // root canvas box border (white)

    // ── MiniMap ───────────────────────────────────────────────────────────────
    inline static ImVec4 MiniMapBg             = {0.118f, 0.118f, 0.118f, 0.863f}; // minimap panel background
    inline static ImVec4 MiniMapViewportFill   = {1.00f,  1.00f,  1.00f,  0.078f}; // viewport rect fill
    inline static ImVec4 MiniMapViewportBorder = {1.00f,  1.00f,  1.00f,  0.627f}; // viewport rect border

    // ── Curve editors (curve.cpp & curve2.cpp) ────────────────────────────────
    inline static ImVec4 CurveEditorBg         = {0.196f, 0.196f, 0.196f, 0.784f}; // curve editor child background
    inline static ImVec4 CurveGridLine         = {0.353f, 0.353f, 0.353f, 0.196f}; // grid lines inside curve editor
    inline static ImVec4 CurveExtensionLine    = {0.039f, 0.039f, 0.039f, 1.00f};  // flat extension beyond endpoints
    inline static ImVec4 CurveControlPointLine = {0.059f, 0.059f, 0.059f, 1.00f};  // Bezier handle lines
    inline static ImVec4 CurvePointDot         = {0.00f,  0.00f,  0.00f,  1.00f};  // vertex dot
    inline static ImVec4 CurveCP1              = {0.00f,  1.00f,  1.00f,  0.498f}; // control point 1 (cyan)
    inline static ImVec4 CurveCP2              = {1.00f,  0.00f,  1.00f,  0.498f}; // control point 2 (magenta)
    inline static ImVec4 CurveInputIndicator   = {0.498f, 0.498f, 0.498f, 0.498f}; // current-value vertical line
    inline static ImVec4 CurveLabelBg          = {0.00f,  0.00f,  0.00f,  0.471f}; // background behind curve name
    inline static ImVec4 CurvePointHoverHighlight = {1.00f, 1.00f, 1.00f, 1.00f}; // hovered point white highlight
    // Semantic guide lines for curve2 (distinct colours for visual differentiation)
    inline static ImVec4 CurveInflectionGuide  = {1.00f, 1.00f, 0.00f, 0.251f}; // inflection guide (yellow)
    inline static ImVec4 CurveAsymmetryGuide   = {0.00f, 1.00f, 1.00f, 0.251f}; // asymmetry guide (cyan)
    inline static ImVec4 CurveBParameterGuide  = {1.00f, 0.00f, 0.00f, 0.251f}; // B-parameter guide (red)

    // ─────────────────────────────────────────────────────────────────────────
    // Helpers
    // ─────────────────────────────────────────────────────────────────────────

    /// Convert ImVec4 → ImU32 for ImDrawList calls (AddLine, AddRect, etc.)
    static ImU32 U32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }

    /// Restore all colours to their built-in defaults.
    static void reset() {
        NodeBg               = {0.157f, 0.157f, 0.157f, 1.00f};
        ConnectionBullet     = {0.00f,  0.00f,  0.00f,  1.00f};
        SelectedBorder       = {1.00f,  0.498f, 0.00f,  1.00f};

        ConnectionLine               = {0.784f, 0.784f, 0.784f, 0.502f};
        ConnectionDragging           = {1.00f,  1.00f,  1.00f,  0.251f};
        ConnectionDraggingReachable  = {1.00f,  1.00f,  1.00f,  0.502f};

        GridLine             = {0.353f, 0.353f, 0.353f, 0.157f};
        GridCenter           = {0.118f, 0.118f, 0.118f, 0.314f};

        CanvasInfoText       = {1.00f, 1.00f, 1.00f, 0.50f};
        FpsGood              = {0.00f, 1.00f, 0.00f, 0.50f};
        FpsWarning           = {1.00f, 0.50f, 0.00f, 0.50f};
        FpsBad              = {1.00f, 0.00f, 0.00f, 0.50f};

        SelectionRectEntire    = {1.00f, 0.498f, 0.00f, 0.118f};
        SelectionRectIntersect = {0.00f, 0.490f, 1.00f, 0.118f};

        ActiveItemHighlight  = {0.40f, 0.90f, 0.40f, 1.00f};

        PopupBg             = {0.12f, 0.12f, 0.12f, 1.00f};
        PopupDimmedText     = {0.70f, 0.70f, 0.70f, 0.70f};
        TransparentButton   = {0.00f, 0.00f, 0.00f, 0.00f};

        SearchMatchHighlight  = {1.00f, 0.863f, 0.00f, 0.176f};
        SearchSelectedHeader  = {0.43f, 0.19f,  0.00f, 0.80f};
        SearchSelectedHovered = {0.86f, 0.38f,  0.00f, 0.60f};
        SearchSelectedActive  = {0.80f, 0.30f,  0.00f, 0.60f};

        SelectedNodeText     = {1.00f, 0.55f,  0.00f, 1.00f};
        ZebraEven            = {1.00f, 1.00f,  1.00f, 0.031f};
        ZebraOdd             = {0.00f, 0.00f,  0.00f, 0.031f};

        SnapshotActive         = {0.40f, 0.00f, 0.00f, 1.00f};
        SnapshotActiveHovered  = {0.70f, 0.00f, 0.00f, 1.00f};
        SnapshotActivePressed  = {1.00f, 0.00f, 0.00f, 1.00f};
        SnapshotFilled         = {0.20f, 0.40f, 0.00f, 1.00f};
        SnapshotFilledHovered  = {0.25f, 0.50f, 0.00f, 1.00f};
        SnapshotFilledPressed  = {0.30f, 0.60f, 0.00f, 1.00f};
        MorphProgressBar       = {0.80f, 0.40f, 0.10f, 1.00f};

        HierarchyLineActive    = {0.863f, 0.863f, 0.863f, 1.00f};
        HierarchyLineInactive  = {0.00f,  0.00f,  0.00f,  1.00f};
        HierarchyActiveBorder  = {0.00f,  0.824f, 0.824f, 1.00f};
        HierarchySelectedBorder= {1.00f,  0.549f, 0.00f,  1.00f};
        HierarchyRootFill      = {0.824f, 0.824f, 0.824f, 1.00f};
        HierarchyRootBorder    = {1.00f,  1.00f,  1.00f,  1.00f};

        MiniMapBg             = {0.118f, 0.118f, 0.118f, 0.863f};
        MiniMapViewportFill   = {1.00f,  1.00f,  1.00f,  0.078f};
        MiniMapViewportBorder = {1.00f,  1.00f,  1.00f,  0.627f};

        CurveEditorBg         = {0.196f, 0.196f, 0.196f, 0.784f};
        CurveGridLine         = {0.353f, 0.353f, 0.353f, 0.196f};
        CurveExtensionLine    = {0.039f, 0.039f, 0.039f, 1.00f};
        CurveControlPointLine = {0.059f, 0.059f, 0.059f, 1.00f};
        CurvePointDot         = {0.00f,  0.00f,  0.00f,  1.00f};
        CurveCP1              = {0.00f,  1.00f,  1.00f,  0.498f};
        CurveCP2              = {1.00f,  0.00f,  1.00f,  0.498f};
        CurveInputIndicator   = {0.498f, 0.498f, 0.498f, 0.498f};
        CurveLabelBg          = {0.00f,  0.00f,  0.00f,  0.471f};
        CurvePointHoverHighlight = {1.00f, 1.00f, 1.00f, 1.00f};
        CurveInflectionGuide  = {1.00f, 1.00f, 0.00f, 0.251f};
        CurveAsymmetryGuide   = {0.00f, 1.00f, 1.00f, 0.251f};
        CurveBParameterGuide  = {1.00f, 0.00f, 0.00f, 0.251f};
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
            { "OC_ConnectionLine",               &ConnectionLine               },
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
            { "OC_PopupBg",             &PopupBg             },
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
        };
    }
};

#endif /* ofxOceanodeColors_h */
