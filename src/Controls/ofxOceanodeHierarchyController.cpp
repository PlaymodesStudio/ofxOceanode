//
//  ofxOceanodeHierarchyController.cpp
//  ofxOceanode
//

#ifndef OFXOCEANODE_HEADLESS

#include "ofxOceanodeHierarchyController.h"
#include "ofxOceanodeNodeMacro.h"
#include "ofxOceanodeNode.h"
#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <functional>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
ofxOceanodeHierarchyController::ofxOceanodeHierarchyController(
    shared_ptr<ofxOceanodeContainer> _container,
    ofxOceanodeCanvas* _canvas)
    : ofxOceanodeBaseController("Hierarchy")
    , rootContainer(_container)
    , rootCanvas(_canvas)
{
}

// ---------------------------------------------------------------------------
// buildHierarchy — recursive DFS
// ---------------------------------------------------------------------------
void ofxOceanodeHierarchyController::buildHierarchy(
    ofxOceanodeContainer* container,
    ofxOceanodeCanvas*    canvas,
    ofxOceanodeNodeMacro* macroNode,
    ofxOceanodeNode*      nodeWrapper,
    ofxOceanodeContainer* hostContainer,
    ofxOceanodeCanvas*    hostCanvas,
    ofxOceanodeNodeMacro* hostMacro,
    const string&         label,
    int                   depth,
    int                   parentIdx,
    vector<HierarchyEntry>& out)
{
    HierarchyEntry entry;
    entry.label         = label;
    entry.container     = container;
    entry.canvas        = canvas;
    entry.macroNode     = macroNode;
    entry.nodeWrapper   = nodeWrapper;
    entry.hostContainer = hostContainer;
    entry.hostCanvas    = hostCanvas;
    entry.hostMacro     = hostMacro;
    entry.depth         = depth;
    entry.parentIndex   = parentIdx;

    int myIndex = (int)out.size();
    out.push_back(entry);

    if (parentIdx >= 0 && parentIdx < (int)out.size()) {
        out[parentIdx].childIndices.push_back(myIndex);
    }

    // Helper: "Macro 1" -> "M1", "Some Long Name 42" -> "SLN42"
    auto abbreviate = [](const string& s) -> string {
        // Split on spaces
        vector<string> tokens;
        {
            size_t start = 0, pos;
            while ((pos = s.find(' ', start)) != string::npos) {
                tokens.push_back(s.substr(start, pos - start));
                start = pos + 1;
            }
            tokens.push_back(s.substr(start));
        }
        if (tokens.empty()) return s;

        // Check if last token is a pure integer
        const string& last = tokens.back();
        bool lastIsNum = !last.empty() &&
            std::all_of(last.begin(), last.end(), [](char c){ return std::isdigit((unsigned char)c); });

        // Build initials from all word-tokens (all except trailing number)
        int wordCount = lastIsNum ? (int)tokens.size() - 1 : (int)tokens.size();
        string abbrev;
        for (int t = 0; t < wordCount; t++) {
            if (!tokens[t].empty())
                abbrev += (char)std::toupper((unsigned char)tokens[t][0]);
        }
        if (lastIsNum) abbrev += last;
        return abbrev.empty() ? s : abbrev;
    };

    // Collect macro children, sort alphabetically by instance name
    struct ChildInfo {
        string name;          // instance name (for sort)
        string displayLabel;  // formatted label
        ofxOceanodeNodeMacro* m;
        ofxOceanodeNode*      node;
    };
    vector<ChildInfo> children;

    for (auto& [id, node] : container->getParameterGroupNodesMap()) {
        if (ofxOceanodeNodeMacro* m = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel())) {
            string instanceName = node->getParameters().getName();
            string shortName    = abbreviate(instanceName);
            string displayLabel;
            if (m->isLocal()) {
                displayLabel = shortName
                    + " [" + m->getInspectorParameter<string>("Local Name").get() + "]";
            } else {
                string macroTypeName = m->getCurrentMacroName();
                if (!macroTypeName.empty()) {
                    displayLabel = shortName + " [" + macroTypeName + "]";
                } else {
                    displayLabel = shortName;
                }
            }
            children.push_back({instanceName, displayLabel, m, node});
        }
    }

    std::sort(children.begin(), children.end(), [](const ChildInfo& a, const ChildInfo& b){
        return a.name < b.name;
    });

    for (auto& child : children) {
        buildHierarchy(
            child.m->getContainer().get(),
            child.m->getCanvas(),
            child.m,
            child.node,
            container,       // hostContainer = this level's container
            canvas,          // hostCanvas    = this level's canvas
            macroNode,       // hostMacro     = this level's macro (nullptr for root)
            child.displayLabel,
            depth + 1,
            myIndex,
            out);
    }
}

// ---------------------------------------------------------------------------
// draw
// ---------------------------------------------------------------------------
void ofxOceanodeHierarchyController::draw()
{
    // -----------------------------------------------------------------------
    // Apply deferred scroll from previous frame (same pattern as NodesController)
    // -----------------------------------------------------------------------
    if (scrollPending && pendingScrollNode && pendingScrollCanvas) {
        glm::vec2 nodeSize = glm::vec2(
            pendingScrollNode->getNodeGui().getRectangle().getWidth(),
            pendingScrollNode->getNodeGui().getRectangle().getHeight());
        glm::vec2 nodePos = pendingScrollNode->getNodeGui().getPosition();
        glm::vec2 center  = pendingScrollCanvas->getContentRegionSize() / 2.0f;
        pendingScrollCanvas->setScrolling(-nodePos - nodeSize / 2.0f + center);
        scrollPending       = false;
        pendingScrollNode   = nullptr;
        pendingScrollCanvas = nullptr;
        if (pendingScrollMacro) {
            pendingScrollMacro->activateWindow();
            pendingScrollMacro = nullptr;
        }
    }

    if (refocusDelay > 0) {
        refocusDelay--;
        if (refocusDelay == 0)
            ImGui::SetWindowFocus("Hierarchy");
    }

    // -----------------------------------------------------------------------
    // Rebuild hierarchy each frame
    // -----------------------------------------------------------------------
    entries.clear();
    buildHierarchy(
        rootContainer.get(), rootCanvas,
        /*macroNode=*/nullptr, /*nodeWrapper=*/nullptr,
        /*hostContainer=*/nullptr, /*hostCanvas=*/nullptr, /*hostMacro=*/nullptr,
        "Canvas",
        0, -1,
        entries);

    // -----------------------------------------------------------------------
    // Layout constants
    // -----------------------------------------------------------------------
    const float NODE_W     = 150.0f;
    const float NODE_H     = 26.0f;
    const float HGAP       = 20.0f;
    const float VGAP       = 8.0f;
    const float INDENT_W   = NODE_W + HGAP;
    const float TEXT_PAD_X = 8.0f;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin  = ImGui::GetCursorScreenPos();
    origin.x += 4.0f;
    origin.y += 4.0f;

    // -----------------------------------------------------------------------
    // Layout pass: assign row indices, centre parents over children
    // -----------------------------------------------------------------------
    int rowCounter = 0;
    vector<float> rowY(entries.size(), 0.0f);

    std::function<void(int)> assignRows = [&](int i) {
        if (entries[i].childIndices.empty()) {
            rowY[i] = (float)rowCounter++;
        } else {
            for (int ci : entries[i].childIndices) assignRows(ci);
            float firstRow = rowY[entries[i].childIndices.front()];
            float lastRow  = rowY[entries[i].childIndices.back()];
            rowY[i] = (firstRow + lastRow) * 0.5f;
        }
    };
    if (!entries.empty()) assignRows(0);

    int totalRows = rowCounter > 0 ? rowCounter : 1;

    // Compute screen positions
    vector<ImVec2> pos(entries.size());
    for (int i = 0; i < (int)entries.size(); i++) {
        pos[i].x = origin.x + entries[i].depth * INDENT_W;
        pos[i].y = origin.y + rowY[i] * (NODE_H + VGAP);
    }

    float totalH = totalRows * (NODE_H + VGAP) + 8.0f;

    // Reserve layout space
    ImGui::Dummy(ImVec2(INDENT_W * 8.0f, totalH));

    // -----------------------------------------------------------------------
    // Determine effective-active state per entry (DFS order, root is always active)
    // -----------------------------------------------------------------------
    vector<bool> effectiveActive(entries.size(), true);
    for (int i = 0; i < (int)entries.size(); i++) {
        if (entries[i].macroNode == nullptr) {
            effectiveActive[i] = true; // root canvas
        } else {
            bool selfActive = entries[i].macroNode->isActive();
            int pi = entries[i].parentIndex;
            bool parentActive = (pi >= 0) ? effectiveActive[pi] : true;
            effectiveActive[i] = parentActive && selfActive;
        }
    }

    // -----------------------------------------------------------------------
    // Helper: draw a dashed line segment
    // -----------------------------------------------------------------------
    auto drawDashedLine = [&](ImVec2 a, ImVec2 b, ImU32 col, float thickness,
                              float dashLen = 4.0f, float gapLen = 4.0f) {
        float dx = b.x - a.x, dy = b.y - a.y;
        float len = std::sqrt(dx*dx + dy*dy);
        if (len < 0.001f) return;
        float nx = dx / len, ny = dy / len;
        float t = 0.0f;
        bool drawing = true;
        while (t < len) {
            float segLen = drawing ? dashLen : gapLen;
            float t2 = std::min(t + segLen, len);
            if (drawing) {
                dl->AddLine(ImVec2(a.x + nx*t, a.y + ny*t),
                            ImVec2(a.x + nx*t2, a.y + ny*t2),
                            col, thickness);
            }
            t = t2;
            drawing = !drawing;
        }
    };

    // -----------------------------------------------------------------------
    // Draw connectors (elbow lines; dotted when parent is inactive)
    // -----------------------------------------------------------------------
    for (int i = 1; i < (int)entries.size(); i++) {
        int pi = entries[i].parentIndex;
        if (pi < 0) continue;

        ImVec2 parentMid = ImVec2(pos[pi].x + NODE_W, pos[pi].y + NODE_H * 0.5f);
        ImVec2 childMid  = ImVec2(pos[i].x,           pos[i].y  + NODE_H * 0.5f);
        float midX = parentMid.x + HGAP * 0.5f;

        bool parentInactive = !effectiveActive[pi];
        ImU32 lineCol = parentInactive ? IM_COL32(120,120,120,100) : IM_COL32(120,120,120,200);

        if (parentInactive) {
            drawDashedLine(parentMid,               ImVec2(midX, parentMid.y), lineCol, 1.5f);
            drawDashedLine(ImVec2(midX, parentMid.y), ImVec2(midX, childMid.y), lineCol, 1.5f);
            drawDashedLine(ImVec2(midX, childMid.y),  childMid,                 lineCol, 1.5f);
        } else {
            dl->AddLine(parentMid,                 ImVec2(midX, parentMid.y), lineCol, 1.5f);
            dl->AddLine(ImVec2(midX, parentMid.y), ImVec2(midX, childMid.y),  lineCol, 1.5f);
            dl->AddLine(ImVec2(midX, childMid.y),  childMid,                  lineCol, 1.5f);
        }
    }

    // -----------------------------------------------------------------------
    // Draw boxes and handle clicks
    // -----------------------------------------------------------------------
    ImVec2 mousePos = ImGui::GetMousePos();
    bool   mouseClicked = ImGui::IsMouseClicked(0);

    for (int i = 0; i < (int)entries.size(); i++) {
        ImVec2 p1 = pos[i];
        ImVec2 p2 = ImVec2(p1.x + NODE_W, p1.y + NODE_H);

        // --- Color ---
        ImU32 fillCol, borderCol;
        ofColor nodeColor;
        if (entries[i].macroNode == nullptr) {
            // Root canvas: white/light
            nodeColor  = ofColor(220, 220, 220);
            fillCol    = IM_COL32(210, 210, 210, 230);
            borderCol  = IM_COL32(255, 255, 255, 255);
        } else {
            nodeColor  = entries[i].nodeWrapper->getColor();
            fillCol    = IM_COL32(nodeColor.r, nodeColor.g, nodeColor.b, 210);
            borderCol  = IM_COL32(
                (int)ofClamp(nodeColor.r * 1.3f, 0, 255),
                (int)ofClamp(nodeColor.g * 1.3f, 0, 255),
                (int)ofClamp(nodeColor.b * 1.3f, 0, 255),
                255);
        }

        // --- Box ---
        dl->AddRectFilled(p1, p2, fillCol,   4.0f);
        dl->AddRect      (p1, p2, borderCol, 4.0f, 0, 1.5f);

        // --- Text color (luminance-adaptive; 50% alpha when effectively inactive) ---
        float lum = (nodeColor.r * 0.299f + nodeColor.g * 0.587f + nodeColor.b * 0.114f) / 255.0f;
        int textAlpha = effectiveActive[i] ? 255 : 128;
        ImU32 textCol = (lum > 0.55f)
            ? IM_COL32(30,  30,  30,  textAlpha)
            : IM_COL32(240, 240, 240, textAlpha);

        // --- Label (clipped) ---
        ImGui::PushClipRect(p1, p2, true);
        float textY = p1.y + (NODE_H - ImGui::GetTextLineHeight()) * 0.5f;
        dl->AddText(ImVec2(p1.x + TEXT_PAD_X, textY), textCol, entries[i].label.c_str());
        ImGui::PopClipRect();

        // --- Click detection ---
        if (mouseClicked && entries[i].macroNode != nullptr) {
            if (mousePos.x >= p1.x && mousePos.x <= p2.x &&
                mousePos.y >= p1.y && mousePos.y <= p2.y)
            {
                ofxOceanodeNode*      targetNode      = entries[i].nodeWrapper;
                ofxOceanodeCanvas*    targetCanvas    = entries[i].hostCanvas;
                ofxOceanodeContainer* targetContainer = entries[i].hostContainer;
                ofxOceanodeNodeMacro* targetMacro     = entries[i].hostMacro;

                // Deselect all nodes in host container, then select this one
                if (targetContainer) {
                    for (auto& pair : targetContainer->getParameterGroupNodesMap())
                        pair.second->getNodeGui().setSelected(false);
                }
                if (targetNode) targetNode->getNodeGui().setSelected(true);

                // Navigate to host canvas
                if (targetMacro != nullptr) {
                    targetMacro->activateWindow();
                    refocusDelay = 4;
                } else if (targetCanvas != nullptr) {
                    targetCanvas->requestFocus();
                    targetCanvas->bringOnTop();
                    refocusDelay = 2;
                }

                // Deferred scroll to centre node on screen
                pendingScrollNode   = targetNode;
                pendingScrollCanvas = targetCanvas;
                pendingScrollMacro  = targetMacro;
                scrollPending       = true;
            }
        }
    }
}

#endif // OFXOCEANODE_HEADLESS
