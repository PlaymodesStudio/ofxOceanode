//
//  ofxOceanodeMiniMapController.cpp
//  ofxOceanode
//

#ifndef OFXOCEANODE_HEADLESS

#include "ofxOceanodeMiniMapController.h"
#include "ofxOceanodeNodeMacro.h"
#include "ofxOceanodeNode.h"
#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>

ofxOceanodeMiniMapController::ofxOceanodeMiniMapController(
    shared_ptr<ofxOceanodeContainer> _container,
    ofxOceanodeCanvas* _canvas)
    : ofxOceanodeBaseController("MiniMap")
    , rootContainer(_container)
    , rootCanvas(_canvas)
    , selectedScopeIndex(0)
    , treeHeight(120.0f)
{
    searchBuf[0] = '\0';
}

// ---------------------------------------------------------------------------
// buildScopeList — recursive, fills all new fields including childIndices
// ---------------------------------------------------------------------------
void ofxOceanodeMiniMapController::buildScopeList(
    ofxOceanodeContainer* container,
    ofxOceanodeCanvas*    canvas,
    const string&         path,
    const string&         label,
    ofxOceanodeNodeMacro* macroNode,
    ofxOceanodeNode*      parentNode,
    ofxOceanodeContainer* parentContainer,
    ofxOceanodeCanvas*    parentCanvas,
    int                   depth,
    int                   parentIndex,
    vector<MiniMapScopeEntry>& out)
{
    MiniMapScopeEntry entry;
    entry.label           = label.empty() ? "Root" : label;
    entry.fullPath        = path;
    entry.container       = container;
    entry.canvas          = canvas;
    entry.macroNode       = macroNode;
    entry.parentNode      = parentNode;
    entry.parentContainer = parentContainer;
    entry.parentCanvas    = parentCanvas;
    entry.depth           = depth;

    int myIndex = (int)out.size();
    out.push_back(entry);

    // Register this index as a child of the parent entry
    if (parentIndex >= 0 && parentIndex < (int)out.size()) {
        out[parentIndex].childIndices.push_back(myIndex);
    }

    // Collect macro children first, then sort alphabetically by label
    struct ChildInfo {
        string macroName;
        string childLabel;
        string childPath;
        ofxOceanodeNodeMacro* m;
        ofxOceanodeNode*      node;
    };
    vector<ChildInfo> children;

    for (auto& [id, node] : container->getParameterGroupNodesMap()) {
        if (ofxOceanodeNodeMacro* m = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel())) {
            string nodeInstanceName = node->getParameters().getName();
            string macroTypeName    = m->getCurrentMacroName();
            string childLabel;
            if (!m->isLocal() && !macroTypeName.empty()) {
                childLabel = nodeInstanceName + " - " + macroTypeName;
            } else {
                childLabel = nodeInstanceName;
            }
            string childPath = path.empty() ? nodeInstanceName : (path + " / " + nodeInstanceName);
            children.push_back({nodeInstanceName, childLabel, childPath, m, node});
        }
    }

    // Sort children alphabetically by instance name at this depth level
    std::sort(children.begin(), children.end(), [](const ChildInfo& a, const ChildInfo& b) {
        return a.macroName < b.macroName;
    });

    for (auto& child : children) {
        buildScopeList(
            child.m->getContainer().get(),
            child.m->getCanvas(),
            child.childPath,
            child.childLabel,
            child.m,
            child.node,
            container,
            canvas,
            depth + 1,
            myIndex,
            out);
    }
}

// ---------------------------------------------------------------------------
// renderScopeTree — recursive ImGui tree, replaces the old BeginCombo block
// ---------------------------------------------------------------------------
void ofxOceanodeMiniMapController::renderScopeTree(int entryIndex)
{
    if (entryIndex < 0 || entryIndex >= (int)scopeList.size()) return;

    const MiniMapScopeEntry& entry = scopeList[entryIndex];
    bool hasChildren = !entry.childIndices.empty();

    // Search filter logic
    string lowerQuery = "";
    bool hasSearch = (searchBuf[0] != '\0');
    if (hasSearch) {
        lowerQuery = string(searchBuf);
        std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(),
                       [](unsigned char c){ return (char)std::tolower(c); });
        // If neither this entry nor any descendant matches, skip rendering
        if (!entryMatchesSearch(entryIndex, lowerQuery)) return;
    }

    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_SpanAvailWidth;

    if (!hasChildren) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (entryIndex == selectedScopeIndex) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    // Auto-expand during search so matching children are visible
    if (hasSearch && hasChildren) {
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
    }

    // Enhancement 3: color global (non-local) macro entries
    bool isGlobal = (entry.macroNode != nullptr && !entry.macroNode->isLocal());
    if (isGlobal) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 0.85f, 0.25f, 1.0f));
    }

    // Use a unique ID per entry to avoid ImGui ID collisions when labels repeat
    ImGui::PushID(entryIndex);
    bool nodeOpen = ImGui::TreeNodeEx(entry.label.c_str(), flags);
    ImGui::PopID();

    if (isGlobal) {
        ImGui::PopStyleColor();
    }

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        selectedScopeIndex = entryIndex;
    }

    if (hasChildren && nodeOpen) {
        for (int childIdx : entry.childIndices) {
            renderScopeTree(childIdx);
        }
        ImGui::TreePop();
    }
}

// ---------------------------------------------------------------------------
// entryMatchesSearch — returns true if entry's label or any descendant matches
// ---------------------------------------------------------------------------
bool ofxOceanodeMiniMapController::entryMatchesSearch(int entryIndex, const string& lowerQuery) const
{
    if (entryIndex < 0 || entryIndex >= (int)scopeList.size()) return false;
    const MiniMapScopeEntry& entry = scopeList[entryIndex];

    // Check own label (case-insensitive)
    string lowerLabel = entry.label;
    std::transform(lowerLabel.begin(), lowerLabel.end(), lowerLabel.begin(),
                   [](unsigned char c){ return (char)std::tolower(c); });
    if (lowerLabel.find(lowerQuery) != string::npos) return true;

    // Check descendants recursively
    for (int childIdx : entry.childIndices) {
        if (entryMatchesSearch(childIdx, lowerQuery)) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// draw
// ---------------------------------------------------------------------------
void ofxOceanodeMiniMapController::draw()
{
    // 1. Rebuild scope list each frame, preserve selection by path
    string previousPath = "";
    if (!scopeList.empty() && selectedScopeIndex < (int)scopeList.size()) {
        previousPath = scopeList[selectedScopeIndex].fullPath;
    }

    scopeList.clear();
    buildScopeList(
        rootContainer.get(), rootCanvas,
        /*path=*/"", /*label=*/"Root",
        /*macroNode=*/nullptr, /*parentNode=*/nullptr,
        /*parentContainer=*/nullptr, /*parentCanvas=*/nullptr,
        /*depth=*/0, /*parentIndex=*/-1,
        scopeList);

    // Clamp / restore selection index
    selectedScopeIndex = ofClamp(selectedScopeIndex, 0, (int)scopeList.size() - 1);
    if (!previousPath.empty()) {
        for (int i = 0; i < (int)scopeList.size(); i++) {
            if (scopeList[i].fullPath == previousPath) {
                selectedScopeIndex = i;
                break;
            }
        }
    }

    // 2. Search field + tree scope selector with user-resizable height
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##minimapSearch", "Search...", searchBuf, sizeof(searchBuf));

    if (ImGui::BeginChild("##scopeTree", ImVec2(0.0f, treeHeight), false)) {
        renderScopeTree(0);
    }
    ImGui::EndChild();

    // Draggable horizontal separator to resize the tree panel
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Separator));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_SeparatorHovered));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_SeparatorActive));
    ImGui::Button("##treeSeparator", ImVec2(-1, 4));
    ImGui::PopStyleColor(3);

    if (ImGui::IsItemActive()) {
        treeHeight += ImGui::GetIO().MouseDelta.y;
        treeHeight = std::max(40.0f, std::min(treeHeight, 600.0f));  // clamp between 40 and 600
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }

    // 3. Locate / Focus buttons (disabled when root is selected)
    bool isRoot = (selectedScopeIndex == 0);
    if (isRoot) ImGui::BeginDisabled(true);

    if (ImGui::Button("Locate") && !isRoot) {
        const MiniMapScopeEntry& sel = scopeList[selectedScopeIndex];
        if (sel.parentNode && sel.parentCanvas) {
            glm::vec2 pos     = sel.parentNode->getNodeGui().getPosition();
            glm::vec2 winSize = sel.parentCanvas->getContentRegionSize();
            sel.parentCanvas->setScrolling(glm::vec2(
                -pos.x + winSize.x * 0.5f,
                -pos.y + winSize.y * 0.5f));
        }
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Scroll parent canvas to where this macro node is located");
    }

    ImGui::SameLine();

    if (ImGui::Button("Focus") && !isRoot) {
        const MiniMapScopeEntry& sel = scopeList[selectedScopeIndex];
        if (sel.macroNode) {
            sel.macroNode->activateWindow();
        }
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Open this macro's window to view its canvas");
    }

    if (isRoot) ImGui::EndDisabled();

    // 4. Render the minimap for the selected scope
    renderMinimap(scopeList[selectedScopeIndex].container,
                  scopeList[selectedScopeIndex].canvas);
}

// ---------------------------------------------------------------------------
// renderMinimap — unchanged from original
// ---------------------------------------------------------------------------
void ofxOceanodeMiniMapController::renderMinimap(
    ofxOceanodeContainer* activeContainer,
    ofxOceanodeCanvas*    activeCanvas)
{
    if (!activeContainer || !activeCanvas) return;

    // --- Helper: return effective rectangle for a node.
    // When the macro window is closed (showWindow=false), the canvas never renders,
    // so constructGui() is never called and nodeGui.setSize() is never invoked.
    // guiRect.width/height stay 0. However, setPosition() IS called from
    // loadPreset_loadNodes() when reading modules.json, so position is valid.
    // We fall back to position + estimated size so closed-macro scopes still show.
    const float estimatedNodeW = (float)(activeCanvas->getNodeWidth() + 16); // +2*NODE_WINDOW_PADDING.x
    auto effectiveRect = [&](ofxOceanodeNode* node) -> ofRectangle {
        ofRectangle r = node->getNodeGui().getRectangle();
        if (r.width > 0.0f && r.height > 0.0f) return r;
        // Size is zero — use stored position + estimated dimensions
        glm::vec2 pos = node->getNodeGui().getPosition();
        bool expanded = node->getNodeGui().getExpanded();
        int numParams = (int)node->getParameters().size();
        // header row ≈ 29px; each expanded parameter row ≈ 20px; padding top+bottom ≈ 14px
        float estimatedH = expanded
            ? (29.0f + numParams * 20.0f + 14.0f)
            : 29.0f;
        return ofRectangle(pos.x, pos.y, estimatedNodeW, estimatedH);
    };

    // --- Compute bounding box in canvas-space ---
    const float PADDING = 40.0f;
    float minX =  FLT_MAX, minY =  FLT_MAX;
    float maxX = -FLT_MAX, maxY = -FLT_MAX;

    for (auto& [id, node] : activeContainer->getParameterGroupNodesMap()) {
        ofRectangle r = effectiveRect(node);
        minX = min(minX, (float)r.x);
        minY = min(minY, (float)r.y);
        maxX = max(maxX, (float)(r.x + r.width));
        maxY = max(maxY, (float)(r.y + r.height));
    }
    for (auto& c : activeContainer->getComments()) {
        minX = min(minX, c.position.x);
        minY = min(minY, c.position.y);
        maxX = max(maxX, c.position.x + c.size.x);
        maxY = max(maxY, c.position.y + c.size.y);
    }

    // --- Get minimap screen area ---
    ImVec2 mmSize = ImGui::GetContentRegionAvail();
    if (mmSize.x < 4.0f) mmSize.x = 4.0f;
    if (mmSize.y < 4.0f) mmSize.y = 4.0f;

    // Reserve full available space as an invisible button for interaction
    ImGui::InvisibleButton("##minimap", mmSize);
    ImVec2 mmPos = ImGui::GetItemRectMin();

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Background
    draw_list->AddRectFilled(
        mmPos,
        ImVec2(mmPos.x + mmSize.x, mmPos.y + mmSize.y),
        IM_COL32(30, 30, 30, 220));

    // --- Handle empty patch ---
    bool hasContent = (minX != FLT_MAX && maxX != -FLT_MAX);
    if (!hasContent) {
        const char* emptyMsg = "Empty";
        draw_list->AddText(
            ImVec2(mmPos.x + mmSize.x * 0.5f - 18.0f,
                   mmPos.y + mmSize.y * 0.5f - 6.0f),
            IM_COL32(128, 128, 128, 200),
            emptyMsg);
        return;
    }

    // --- Compute scale to fit bounding box in mmSize while preserving aspect ratio ---
    float canvasW = (maxX - minX) + 2.0f * PADDING;
    float canvasH = (maxY - minY) + 2.0f * PADDING;

    float scaleX = mmSize.x / canvasW;
    float scaleY = mmSize.y / canvasH;
    float scale  = min(scaleX, scaleY);

    float offsetX = mmPos.x + (mmSize.x - canvasW * scale) * 0.5f;
    float offsetY = mmPos.y + (mmSize.y - canvasH * scale) * 0.5f;

    // Mapping lambda: canvas-space → minimap screen-space
    auto toMinimap = [&](float cx, float cy) -> ImVec2 {
        return ImVec2(
            offsetX + (cx - minX + PADDING) * scale,
            offsetY + (cy - minY + PADDING) * scale);
    };

    // --- Draw comments (background layer) ---
    for (auto& c : activeContainer->getComments()) {
        ImVec2 p1 = toMinimap(c.position.x, c.position.y);
        ImVec2 p2 = toMinimap(c.position.x + c.size.x, c.position.y + c.size.y);
        // Clamp to minimap bounds
        p1.x = ofClamp(p1.x, mmPos.x, mmPos.x + mmSize.x);
        p1.y = ofClamp(p1.y, mmPos.y, mmPos.y + mmSize.y);
        p2.x = ofClamp(p2.x, mmPos.x, mmPos.x + mmSize.x);
        p2.y = ofClamp(p2.y, mmPos.y, mmPos.y + mmSize.y);
        if (p2.x <= p1.x || p2.y <= p1.y) continue;

        ImU32 fillCol = IM_COL32(
            (int)(c.color.r * 255),
            (int)(c.color.g * 255),
            (int)(c.color.b * 255),
            80);
        ImU32 borderCol = IM_COL32(
            (int)(c.color.r * 255),
            (int)(c.color.g * 255),
            (int)(c.color.b * 255),
            180);
        draw_list->AddRectFilled(p1, p2, fillCol, 1.0f);
        draw_list->AddRect(p1, p2, borderCol);
    }

    // --- Draw nodes ---
    for (auto& [id, node] : activeContainer->getParameterGroupNodesMap()) {
        ofRectangle r = effectiveRect(node);

        // Skip transparent nodes
        bool isTransparent = (node->getNodeModel().getFlags() & ofxOceanodeNodeModelFlags_TransparentNode);
        if (isTransparent) continue;

        ofColor col = node->getColor();
        ImVec2 p1 = toMinimap((float)r.x, (float)r.y);
        ImVec2 p2 = toMinimap((float)(r.x + r.width), (float)(r.y + r.height));

        // Dark body
        draw_list->AddRectFilled(p1, p2, IM_COL32(40, 40, 40, 200), 1.0f);

        // Colored header strip (top ~29 canvas-space px, scaled)
        float headerH = 29.0f * scale;
        ImVec2 headerBR = ImVec2(p2.x, p1.y + headerH);
        draw_list->AddRectFilled(
            p1, headerBR,
            IM_COL32(col.r, col.g, col.b, 160), 1.0f);
    }

    // --- Viewport indicator ---
    glm::vec2 scroll   = activeCanvas->getScrolling();
    glm::vec2 winSize  = activeCanvas->getContentRegionSize();

    // Visible top-left in canvas-space: when scrolling=(sx,sy), nodes at (0,0) appear at
    // screenPos + scrolling, so canvas origin is at screen scrolling offset.
    // Visible canvas top-left = -scroll, bottom-right = -scroll + winSize
    float vpLeft  = -scroll.x;
    float vpTop   = -scroll.y;
    float vpRight = vpLeft  + winSize.x;
    float vpBot   = vpTop   + winSize.y;

    ImVec2 vp1 = toMinimap(vpLeft,  vpTop);
    ImVec2 vp2 = toMinimap(vpRight, vpBot);

    draw_list->AddRectFilled(vp1, vp2, IM_COL32(255, 255, 255, 20));
    draw_list->AddRect(vp1, vp2, IM_COL32(255, 255, 255, 160), 0.0f, 0, 1.5f);

    // --- Click-to-navigate ---
    bool itemHovered = ImGui::IsItemHovered();
    bool mouseClicked  = ImGui::IsMouseClicked(0);
    bool mouseDragging = ImGui::IsMouseDragging(0);

    if (itemHovered && (mouseClicked || mouseDragging)) {
        ImVec2 mousePos = ImGui::GetMousePos();
        float cx = (mousePos.x - offsetX) / scale + minX - PADDING;
        float cy = (mousePos.y - offsetY) / scale + minY - PADDING;

        // Center view on the clicked canvas point
        activeCanvas->setScrolling(glm::vec2(
            winSize.x * 0.5f - cx,
            winSize.y * 0.5f - cy));
    }
}

#endif // OFXOCEANODE_HEADLESS
