//
//  ofxOceanodeNodesController.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 13/03/2018.
//

#include "ofxOceanodeNodesController.h"
#include "ofxOceanodeContainer.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "ofxOceanodeShared.h"
#include "ofxOceanodeCanvas.h"
#include "ofxOceanodeNodeMacro.h"
#include "portal.h"
#include "router.h"
#include "ofxOceanodeColors.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <numeric>

namespace {
string lowercase(string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

// Compare digit runs without allocating substrings or overflowing stoi().
bool naturalLess(const string& a, const string& b) {
    size_t i = 0, j = 0;
    while(i < a.size() && j < b.size()) {
        if(std::isdigit(static_cast<unsigned char>(a[i])) &&
           std::isdigit(static_cast<unsigned char>(b[j]))) {
            size_t endA = i, endB = j;
            while(endA < a.size() && std::isdigit(static_cast<unsigned char>(a[endA]))) ++endA;
            while(endB < b.size() && std::isdigit(static_cast<unsigned char>(b[endB]))) ++endB;
            size_t digitsA = i, digitsB = j;
            while(digitsA < endA && a[digitsA] == '0') ++digitsA;
            while(digitsB < endB && b[digitsB] == '0') ++digitsB;
            if(endA - digitsA != endB - digitsB) return endA - digitsA < endB - digitsB;
            const int comparison = a.compare(digitsA, endA - digitsA, b, digitsB, endB - digitsB);
            if(comparison != 0) return comparison < 0;
            i = endA;
            j = endB;
        } else {
            const int ca = std::tolower(static_cast<unsigned char>(a[i]));
            const int cb = std::tolower(static_cast<unsigned char>(b[j]));
            if(ca != cb) return ca < cb;
            ++i;
            ++j;
        }
    }
    return a.size() < b.size();
}

void queueCanvasLayout(ofxOceanodeCanvas* canvas) {
    if(!ofxOceanodeShared::getGuiLayoutChangesWithMacros()) return;
    if(canvas->getLayoutIniPath().empty()) return;
    const string path = ofToDataPath(canvas->getLayoutIniPath());
    string& activePath = ofxOceanodeShared::getActiveCanvasLayoutPath();
    if(!path.empty() && path != activePath) {
        ofxOceanodeShared::getPendingLayoutSavePath() = activePath;
        ofxOceanodeShared::getPendingLayoutLoadPath() = path;
        activePath = path;
    }
}
}

ofxOceanodeNodesController::ofxOceanodeNodesController(shared_ptr<ofxOceanodeContainer> _container,
                                                     ofxOceanodeCanvas* _canvas)
    : ofxOceanodeBaseController("Nodes"), container(_container), canvas(_canvas)
{
    nodeSelectedListener = ofxOceanodeShared::getNodeSelectedInCanvasEvent().newListener(
        [this](ofxOceanodeNode* node) {
            selectedNode = node;
            if(node != nullptr) {
                scrollTreeToSelected = true;
                forceExpandAll = true;
            } else {
                pendingScrollNode = nullptr;
                scrollPendingFrames = 0;
                scrollTreeToSelected = false;
            }
        });
}

ofxOceanodeNodesController::CachedContainer& ofxOceanodeNodesController::refreshCache(
    const shared_ptr<ofxOceanodeContainer>& target, bool expandAll)
{
    auto& cache = nodeCaches[target.get()];
    bool sortDirty = false;
    if(!cache.initialized || cache.owner.expired() || cache.revision != target->getNodesRevision()) {
        // Preserve expansion for surviving macros. No old node is dereferenced:
        // a preset may already have destroyed every node in the previous cache.
        std::unordered_map<ofxOceanodeNode*, bool> previousOpen;
        if(!cache.owner.expired())
            for(const auto& entry : cache.nodes)
                if(entry.macro != nullptr) previousOpen[entry.node] = entry.open;
        cache.nodes.clear();
        for(auto* node : target->getAllModules()) {
            CachedNode entry;
            entry.node = node;
            auto& model = node->getNodeModel();
            entry.macro = dynamic_cast<ofxOceanodeNodeMacro*>(&model);
            if(entry.macro != nullptr) {
                entry.type = 0;
                auto old = previousOpen.find(node);
                if(old != previousOpen.end()) entry.open = old->second;
            } else if((entry.portal = dynamic_cast<abstractPortal*>(&model)) != nullptr) {
                entry.type = 1;
            } else if((entry.router = dynamic_cast<abstractRouter*>(&model)) != nullptr) {
                entry.type = 2;
            }
            cache.nodes.push_back(std::move(entry));
        }
        cache.owner = target;
        cache.revision = target->getNodesRevision();
        cache.initialized = true;
        cache.order.resize(cache.nodes.size());
        std::iota(cache.order.begin(), cache.order.end(), 0);
        sortDirty = rowsDirty = topologyChanged = true;
    }

    const bool searchChanged = cache.search != lowercaseSearch;
    cache.hasSearchMatch = false;
    cache.typeMask = 0;
    for(auto& entry : cache.nodes) {
        auto& model = entry.node->getNodeModel();
        string baseName = (entry.portal || entry.router)
            ? model.nodeName() : entry.node->getParameters().getName();
        string detailName;
        if(entry.macro) {
            detailName = entry.macro->isLocal()
                ? entry.macro->getLocalMacroName() : entry.macro->getCurrentMacroName();
            const bool active = entry.macro->isActive();
            if(entry.active != active) rowsDirty = true;
            entry.active = active;
            if(expandAll && !entry.open) {
                entry.open = true;
                rowsDirty = true;
            }
        } else if(entry.portal) {
            detailName = entry.portal->getName();
        } else if(entry.router) {
            detailName = entry.router->getNameParam().get();
        }
        const unsigned int identifier = model.getNumIdentifier();
        const bool nameChanged = entry.displayName.empty() || entry.baseName != baseName ||
            entry.detailName != detailName || entry.identifier != identifier;
        if(nameChanged) {
            entry.baseName = std::move(baseName);
            entry.detailName = std::move(detailName);
            entry.identifier = identifier;
            entry.displayName = entry.baseName;
            if(entry.macro) entry.displayName += " [" + entry.detailName + "]";
            else if(entry.portal || entry.router)
                entry.displayName += " " + ofToString(identifier) + " [" + entry.detailName + "]";
            entry.sortKey = entry.portal
                ? entry.detailName + " " + ofToString(identifier) : entry.displayName;
            entry.lowercaseName = lowercase(entry.displayName);
            sortDirty = rowsDirty = true;
        }
        if(nameChanged || searchChanged) {
            entry.matchesSearch = !lowercaseSearch.empty() &&
                entry.lowercaseName.find(lowercaseSearch) != string::npos;
        }

        unsigned int childTypes = 0;
        entry.hasMatchingDescendant = false;
        // Without filters, closed macros don't require walking their contents.
        if(entry.macro && (entry.open || !lowercaseSearch.empty() || nodeTypeFilter != 0)) {
            auto& children = refreshCache(entry.macro->getContainer(), expandAll);
            childTypes = children.typeMask;
            entry.hasMatchingDescendant = children.hasSearchMatch;
            if(!lowercaseSearch.empty() && entry.hasMatchingDescendant && !entry.open) {
                entry.open = true;
                rowsDirty = true;
            }
        }
        const bool passesName = lowercaseSearch.empty() || entry.matchesSearch || entry.hasMatchingDescendant;
        // Retain the existing type filter: own type or a macro's immediate children.
        const bool passesType = nodeTypeFilter == 0 || entry.type == nodeTypeFilter - 1 ||
            (entry.macro && (childTypes & (1u << (nodeTypeFilter - 1))) != 0);
        const bool visible = passesName && passesType;
        if(entry.visible != visible || searchChanged) rowsDirty = true;
        entry.visible = visible;
        cache.hasSearchMatch |= entry.matchesSearch || entry.hasMatchingDescendant;
        cache.typeMask |= 1u << entry.type;
    }
    cache.search = lowercaseSearch;
    if(sortDirty) {
        std::sort(cache.order.begin(), cache.order.end(), [&cache](size_t a, size_t b) {
            const auto& left = cache.nodes[a];
            const auto& right = cache.nodes[b];
            if(left.type != right.type) return left.type < right.type;
            if(left.type == 1 || left.type == 3) return naturalLess(left.sortKey, right.sortKey);
            return left.displayName < right.displayName;
        });
    }
    return cache;
}

void ofxOceanodeNodesController::appendRows(CachedContainer& cache, ofxOceanodeCanvas* hostCanvas,
                                         ofxOceanodeNodeMacro* hostMacro, int depth, bool parentActive)
{
    for(size_t index : cache.order) {
        auto& entry = cache.nodes[index];
        if(!entry.visible) continue;
        visibleRows.push_back({&entry, hostCanvas, hostMacro, depth, parentActive});
        navigableNodes.push_back({entry.node, hostCanvas, hostMacro, entry.matchesSearch});
        if(entry.displayName.find('\n') != string::npos) uniformRowHeight = false;
        if(entry.macro && entry.open) {
            auto found = nodeCaches.find(entry.macro->getContainer().get());
            if(found != nodeCaches.end())
                appendRows(found->second, entry.macro->getCanvas(), entry.macro,
                           depth + 1, parentActive && entry.active);
        }
    }
}

bool ofxOceanodeNodesController::findNode(ofxOceanodeNode* target,
    const shared_ptr<ofxOceanodeContainer>& host, ofxOceanodeCanvas* hostCanvas,
    ofxOceanodeNodeMacro* hostMacro, NavigableNode& location)
{
    for(auto* node : host->getAllModules()) {
        if(node == target) {
            location = {node, hostCanvas, hostMacro, false};
            return true;
        }
        if(auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel())) {
            if(findNode(target, macro->getContainer(), macro->getCanvas(), macro, location)) return true;
        }
    }
    return false;
}

void ofxOceanodeNodesController::navigateTo(const NavigableNode& target)
{
    selectedNode = target.node;
    auto host = target.macro ? target.macro->getContainer() : container;
    for(auto& pair : host->getParameterGroupNodesMap()) pair.second->getNodeGui().setSelected(false);
    target.node->getNodeGui().setSelected(true);
    if(target.macro) target.macro->activateWindow();
    else {
        target.canvas->requestFocus();
        target.canvas->bringOnTop();
    }
    ofxOceanodeShared::setActiveCanvasUniqueID(target.canvas->getUniqueID());
    // This target is already in the expanded tree. Keep canvas-selection sync
    // from expanding unrelated macros during tree mouse/keyboard navigation.
    const bool expandWasRequested = forceExpandAll;
    ofxOceanodeShared::nodeSelectedInCanvas(target.node);
    forceExpandAll = expandWasRequested;
    pendingScrollNode = target.node;
    scrollPendingFrames = target.macro ? 2 : 1;
    refocusNodesDelay = target.macro ? 4 : 2;
    ofxOceanodeShared::getLayoutSwitchSuppressFrames() = 4;
    queueCanvasLayout(target.canvas);
}

void ofxOceanodeNodesController::drawRow(VisibleRow& row, int rowIndex)
{
    auto& entry = *row.entry;
    const bool effectiveActive = row.parentActive && entry.active;
    if(!effectiveActive) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
    ImVec4 textColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    if(entry.node == selectedNode) textColor = OceanodeColors::SelectedNodeText;
    else if(!entry.macro) {
        textColor.x *= 0.75f;
        textColor.y *= 0.75f;
        textColor.z *= 0.75f;
    }
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    // Match the previous recursive TreePush (16 px) plus depth-dependent indent.
    const float indentation = row.depth * 16.0f + 6.0f * row.depth * (row.depth + 1);
    if(indentation > 0) ImGui::Indent(indentation);
    const ImVec2 swatchPos = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(9.0f, ImGui::GetTextLineHeight()));
    ImGui::SameLine(0, 0);
    ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
    bool selected = false;
    if(entry.macro) {
        ImGui::SetNextItemOpen(entry.open, ImGuiCond_Always);
        const bool open = ImGui::TreeNodeEx(static_cast<void*>(entry.node),
            ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
            ImGuiTreeNodeFlags_NoTreePushOnOpen, "%s", entry.displayName.c_str());
        if(entry.open != open) {
            entry.open = open;
            rowsDirty = true; // rebuild after this frame's clipper has finished
        }
    } else {
        ImGui::PushID(entry.node);
        selected = ImGui::Selectable(entry.displayName.c_str(), false, ImGuiSelectableFlags_SpanAvailWidth);
        ImGui::PopID();
    }
    ImGui::PopItemFlag();
    if(scrollTreeToSelected && entry.node == selectedNode) {
        ImGui::SetScrollHereY(0.5f);
        scrollTreeToSelected = false;
    }
    ImGui::PopStyleColor();
    if(!effectiveActive) ImGui::PopStyleVar();

    // AddRectFilled doesn't CPU-clip its geometry. Keep decorations inside the viewport.
    const ImVec2 rowMin(ImGui::GetWindowPos().x, ImGui::GetItemRectMin().y);
    const ImVec2 rowMax(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetItemRectMax().y);
    if(ImGui::IsRectVisible(rowMin, rowMax)) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(rowMin, rowMax, OceanodeColors::U32(
            rowIndex % 2 == 0 ? OceanodeColors::ZebraEven : OceanodeColors::ZebraOdd));
        if(entry.matchesSearch)
            drawList->AddRectFilled(rowMin, rowMax, OceanodeColors::U32(OceanodeColors::SearchMatchHighlight));
        const ImVec2 swatchMax(swatchPos.x + 4.0f, swatchPos.y + ImGui::GetTextLineHeight());
        if(ImGui::IsRectVisible(swatchPos, swatchMax)) {
            const ofColor color = entry.node->getColor();
            drawList->AddRectFilled(swatchPos, swatchMax,
                ImGui::ColorConvertFloat4ToU32(ImVec4(color.r/255.0f, color.g/255.0f, color.b/255.0f, 1.0f)));
        }
    }
    if(entry.macro) {
        if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && !ImGui::IsItemToggledOpen()) {
            entry.macro->activateWindow();
            ofxOceanodeShared::setActiveCanvasUniqueID(entry.macro->getCanvas()->getUniqueID());
            ofxOceanodeShared::nodeSelectedInCanvas(nullptr);
            refocusNodesDelay = 4;
            ofxOceanodeShared::getLayoutSwitchSuppressFrames() = 4;
            queueCanvasLayout(entry.macro->getCanvas());
            pendingScrollNode = nullptr;
            scrollPendingFrames = 0;
        } else if(ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            navigateTo({entry.node, row.canvas, row.macro, entry.matchesSearch});
        }
    } else if(selected) {
        navigateTo({entry.node, row.canvas, row.macro, entry.matchesSearch});
    }
    if(indentation > 0) ImGui::Unindent(indentation);
}

void ofxOceanodeNodesController::draw()
{
    if(refocusNodesDelay > 0 && --refocusNodesDelay == 0) ImGui::SetWindowFocus("Nodes");
    if(ofxOceanodeShared::getLayoutSwitchSuppressFrames() > 0)
        --ofxOceanodeShared::getLayoutSwitchSuppressFrames();

    if(scrollPendingFrames > 0 && --scrollPendingFrames == 0) {
        // Validate against the live graph before dereferencing deferred raw pointers.
        NavigableNode location;
        if(pendingScrollNode && findNode(pendingScrollNode, container, canvas, nullptr, location)) {
            const float zoom = location.canvas->getZoomLevel();
            auto& gui = location.node->getNodeGui();
            const glm::vec2 size(gui.getRectangle().getWidth(), gui.getRectangle().getHeight());
            const glm::vec2 center = location.canvas->getContentRegionSize() / (2.0f * zoom);
            location.canvas->setScrolling(-gui.getPosition() - size / 2.0f + center);
            if(location.macro) location.macro->activateWindow();
        }
        pendingScrollNode = nullptr;
    }

    // The search buffer is bounded and stack-owned; no per-frame heap allocation.
    char searchBuffer[256] = {};
    std::snprintf(searchBuffer, sizeof(searchBuffer), "%s", searchFieldMyNodes.c_str());
    if(ImGui::Button("x##clearSearch")) {
        searchFieldMyNodes.clear();
        searchBuffer[0] = '\0';
        selectedNode = nullptr;
        scrollTreeToSelected = false;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 60);
    if(ImGui::InputText("?##searchMyNodes", searchBuffer, sizeof(searchBuffer))) searchFieldMyNodes = searchBuffer;
    ImGui::SameLine();
    if(ImGui::Button("<##collapseAll")) {
        forceExpandAll = false;
        forceCollapseAll = true;
    }
    ImGui::SameLine();
    if(ImGui::Button(">##expandAll")) forceExpandAll = true;
    ImGui::Text("Filter:");
    ImGui::SameLine();
    ImGui::RadioButton("All", &nodeTypeFilter, 0); ImGui::SameLine();
    ImGui::RadioButton("Macros", &nodeTypeFilter, 1); ImGui::SameLine();
    ImGui::RadioButton("Portals", &nodeTypeFilter, 2); ImGui::SameLine();
    ImGui::RadioButton("Routers", &nodeTypeFilter, 3);
    ImGui::Separator();

    lowercaseSearch = lowercase(searchFieldMyNodes);
    if(cachedTypeFilter != nodeTypeFilter) rowsDirty = true;
    cachedTypeFilter = nodeTypeFilter;
    for(auto it = nodeCaches.begin(); it != nodeCaches.end();) {
        if(it->second.owner.expired()) {
            it = nodeCaches.erase(it);
            rowsDirty = topologyChanged = true;
        } else ++it;
    }
    if(forceCollapseAll) {
        for(auto& pair : nodeCaches)
            for(auto& entry : pair.second.nodes) entry.open = false;
        rowsDirty = true;
    }
    const bool expandAll = forceExpandAll;
    forceExpandAll = forceCollapseAll = false;
    auto& root = refreshCache(container, expandAll);
    if(topologyChanged) {
        NavigableNode location;
        if(selectedNode && !findNode(selectedNode, container, canvas, nullptr, location)) {
            selectedNode = nullptr;
            scrollTreeToSelected = false;
        }
        topologyChanged = false;
    }
    if(rowsDirty) {
        visibleRows.clear();
        navigableNodes.clear();
        uniformRowHeight = true;
        appendRows(root, canvas, nullptr, 0, true);
        rowsDirty = false;
    }

    if(ImGui::BeginChild("##nodesListChild", ImVec2(0, 0), false, ImGuiWindowFlags_NoNav)) {
        if(uniformRowHeight) {
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(visibleRows.size()), ImGui::GetTextLineHeightWithSpacing());
            if(scrollTreeToSelected && selectedNode) {
                for(size_t i = 0; i < visibleRows.size(); ++i) {
                    if(visibleRows[i].entry->node == selectedNode) {
                        clipper.IncludeItemByIndex(static_cast<int>(i));
                        break;
                    }
                }
            }
            while(clipper.Step())
                for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) drawRow(visibleRows[i], i);
        } else {
            // User-defined multiline names need variable-height layout.
            for(size_t i = 0; i < visibleRows.size(); ++i) drawRow(visibleRows[i], static_cast<int>(i));
        }
    }
    ImGui::EndChild();

    const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    const bool up = focused && ImGui::IsKeyPressed(ImGuiKey_UpArrow, true);
    const bool down = focused && ImGui::IsKeyPressed(ImGuiKey_DownArrow, true);
    if((up || down) && !navigableNodes.empty()) {
        vector<const NavigableNode*> pool;
        if(!searchFieldMyNodes.empty())
            for(const auto& node : navigableNodes) if(node.matchesSearch) pool.push_back(&node);
        if(pool.empty())
            for(const auto& node : navigableNodes) pool.push_back(&node);
        for(size_t i = 0; i < pool.size(); ++i) {
            if(pool[i]->node != selectedNode) continue;
            const size_t next = down ? (i + 1) % pool.size() : (i + pool.size() - 1) % pool.size();
            if(next != i) navigateTo(*pool[next]);
            break;
        }
    }
}
