//
//  ofxOceanodeScope.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 05/05/2020.
//
#define IMGUI_DEFINE_MATH_OPERATORS
#include "ofxOceanodeScope.h"
#include <algorithm>
#include <limits>
#include "imgui.h"
#include "imgui_internal.h"
#include "ofxOceanodeParameter.h"
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeNode.h"
#include "ofxOceanodeShared.h"
#include "ofxOceanodeColors.h"

// ---------------------------------------------------------------------------
// Proportional dock-node resizing helpers
//
// ImGui's default behavior when a docked parent window is resized is to keep
// the absolute pixel size (SizeRef) of one split child and give all the
// remaining space to the other child. This makes scope windows feel "sticky":
// one keeps its size and the other one collapses/expands.
//
// To get a proportional resize (every child keeps its share of the parent),
// we walk the dock node tree under ScopesDockSpace each frame the parent
// changes size, and rewrite each split node's children SizeRef so the ratio
// of (child0 / child1) along the split axis is preserved at the new parent
// size. ImGui will then re-run layout with those new SizeRefs.
// ---------------------------------------------------------------------------
static void RescaleDockNodeProportional(ImGuiDockNode* node, ImVec2 oldSize, ImVec2 newSize)
{
    if(node == NULL) return;

    // For each split node, rescale the two children along the split axis so
    // their ratio is preserved, then recurse into them.
    if(node->ChildNodes[0] != NULL && node->ChildNodes[1] != NULL)
    {
        ImGuiDockNode* c0 = node->ChildNodes[0];
        ImGuiDockNode* c1 = node->ChildNodes[1];

        // SplitAxis: X = horizontal split (children side-by-side, scaled in X),
        //            Y = vertical split (children stacked, scaled in Y).
        if(node->SplitAxis == ImGuiAxis_X)
        {
            float oldParent = oldSize.x;
            float newParent = newSize.x;
            if(oldParent > 0.0f && newParent > 0.0f)
            {
                // Use current SizeRef.x as the source of truth for the ratio.
                // (Fall back to Size.x if SizeRef is zero, which happens just
                //  after a fresh split.)
                float s0 = c0->SizeRef.x > 0.0f ? c0->SizeRef.x : c0->Size.x;
                float s1 = c1->SizeRef.x > 0.0f ? c1->SizeRef.x : c1->Size.x;
                float total = s0 + s1;
                if(total > 0.0f)
                {
                    float ratio0 = s0 / total;
                    c0->SizeRef.x = newParent * ratio0;
                    c1->SizeRef.x = newParent * (1.0f - ratio0);
                    // Keep the orthogonal axis snapped to the parent.
                    c0->SizeRef.y = newSize.y;
                    c1->SizeRef.y = newSize.y;
                }
            }
        }
        else if(node->SplitAxis == ImGuiAxis_Y)
        {
            float oldParent = oldSize.y;
            float newParent = newSize.y;
            if(oldParent > 0.0f && newParent > 0.0f)
            {
                float s0 = c0->SizeRef.y > 0.0f ? c0->SizeRef.y : c0->Size.y;
                float s1 = c1->SizeRef.y > 0.0f ? c1->SizeRef.y : c1->Size.y;
                float total = s0 + s1;
                if(total > 0.0f)
                {
                    float ratio0 = s0 / total;
                    c0->SizeRef.y = newParent * ratio0;
                    c1->SizeRef.y = newParent * (1.0f - ratio0);
                    c0->SizeRef.x = newSize.x;
                    c1->SizeRef.x = newSize.x;
                }
            }
        }

        // Recurse with each child's *current* size as the "old" size and the
        // freshly-written SizeRef as the "new" size, so deeper splits also
        // scale proportionally.
        RescaleDockNodeProportional(c0, c0->Size, c0->SizeRef);
        RescaleDockNodeProportional(c1, c1->Size, c1->SizeRef);
    }
}

static std::string BuildScopeWindowName(const ofxOceanodeScopeItem& item)
{
    const std::string fullPath = item.getFullPath();
    std::string windowName = item.canvasID == "Canvas" ? fullPath : (item.canvasID + " / " + fullPath);
    windowName += "###Scope_" + item.canvasID + "_" + fullPath;
    return windowName;
}

static ImGuiWindow* FindScopeWindowToFillCentralNode(
    const std::vector<ofxOceanodeScopeItem>& scopedParameters,
    ImGuiDockNode* rootNode,
    ImGuiID excludedWindowID
)
{
    ImGuiWindow* bestWindow = NULL;
    ImVec2 bestPosition(0.0f, 0.0f);
    bool bestIsVisible = false;
    const float sameRowTolerance = 1.0f;

    for(const auto& item : scopedParameters)
    {
        ImGuiWindow* window = ImGui::FindWindowByName(item.windowName.c_str());
        if(window == NULL || window->DockNode == NULL || window->ID == excludedWindowID) continue;
        if(ImGui::DockNodeGetRootNode(window->DockNode) != rootNode) continue;

        const ImVec2 position = window->DockNode->Pos;
        const bool isVisible = window->DockNode->VisibleWindow == window;

        bool comesFirst = bestWindow == NULL;
        if(bestWindow != NULL)
        {
            const bool isHigherRow = position.y < bestPosition.y - sameRowTolerance;
            const bool isSameRow = position.y >= bestPosition.y - sameRowTolerance
                && position.y <= bestPosition.y + sameRowTolerance;
            const bool isFurtherLeft = position.x < bestPosition.x;
            const bool isSamePosition = isSameRow
                && position.x >= bestPosition.x - sameRowTolerance
                && position.x <= bestPosition.x + sameRowTolerance;

            comesFirst = isHigherRow
                || (isSameRow && isFurtherLeft)
                || (isSamePosition && isVisible && !bestIsVisible);
        }

        if(comesFirst)
        {
            bestWindow = window;
            bestPosition = position;
            bestIsVisible = isVisible;
        }
    }

    return bestWindow;
}

static bool IsScopeWindowPendingDock(
    const std::vector<ofxOceanodeScopeItem>& scopedParameters,
    ImGuiID dockNodeID
)
{
    for(const auto& item : scopedParameters)
    {
        ImGuiWindow* window = ImGui::FindWindowByName(item.windowName.c_str());
        if(window != NULL && window->DockNode == NULL && window->DockId == dockNodeID) return true;
    }

    return false;
}

static ImGuiWindow* FindCurrentScopeWindowInNode(
    const std::vector<ofxOceanodeScopeItem>& scopedParameters,
    ImGuiDockNode* dockNode
)
{
    if(dockNode == NULL) return NULL;

    ImGuiWindow* firstScopeWindow = NULL;
    for(const auto& item : scopedParameters)
    {
        ImGuiWindow* window = ImGui::FindWindowByName(item.windowName.c_str());
        if(window == NULL || window->DockNode != dockNode) continue;

        if(window == dockNode->VisibleWindow) return window;
        if(firstScopeWindow == NULL) firstScopeWindow = window;
    }

    return firstScopeWindow;
}

static void ReturnScopeWindowToTop(
    ImGuiWindow* window,
    ImGuiID dockspaceID,
    size_t scopeCount
)
{
    if(window == NULL) return;

    // With no other scopes there is no stack to split: fill the dockspace.
    if(scopeCount <= 1)
    {
        ImGui::SetWindowDock(window, dockspaceID, ImGuiCond_Always);
        return;
    }

    // Give the returned scope approximately one equal share of the total
    // height. The exact distribution policy can be refined independently.
    const float topRatio = 1.0f / static_cast<float>(scopeCount);
    ImGuiID topNodeID = 0;
    ImGui::DockBuilderSplitNode(
        dockspaceID,
        ImGuiDir_Up,
        topRatio,
        &topNodeID,
        NULL
    );

    if(topNodeID != 0)
    {
        ImGui::SetWindowDock(window, topNodeID, ImGuiCond_Always);
    }
    else
    {
        ImGui::SetWindowDock(window, dockspaceID, ImGuiCond_Always);
    }
}

static bool DockScopeWindowAtBottom(
    const std::string& windowName,
    ImGuiID dockspaceID,
    size_t scopeCount
)
{
    if(ImGui::DockBuilderGetNode(dockspaceID) == NULL) return false;

    if(scopeCount <= 1)
    {
        ImGui::DockBuilderDockWindow(windowName.c_str(), dockspaceID);
        return true;
    }

    // Splitting the whole existing tree by 1/N scales the previous N-1 rows
    // to (N-1)/N and gives the new row the remaining equal share.
    ImGuiID bottomNodeID = 0;
    ImGui::DockBuilderSplitNode(
        dockspaceID,
        ImGuiDir_Down,
        1.0f / static_cast<float>(scopeCount),
        &bottomNodeID,
        NULL
    );

    if(bottomNodeID == 0) return false;
    ImGui::DockBuilderDockWindow(windowName.c_str(), bottomNodeID);
    return true;
}

static unsigned long long HashScopeLayoutValue(
    unsigned long long hash,
    unsigned long long value
)
{
    // FNV-1a is small and sufficient for detecting changes within one run.
    hash ^= value;
    return hash * 1099511628211ULL;
}

static unsigned long long HashScopeLayoutFloat(
    unsigned long long hash,
    float value
)
{
    const long long quantized = static_cast<long long>(value * 10.0f);
    return HashScopeLayoutValue(hash, static_cast<unsigned long long>(quantized));
}

static unsigned long long GetDockLayoutSignature(
    const ImGuiDockNode* node,
    unsigned long long hash = 1469598103934665603ULL
)
{
    if(node == NULL) return HashScopeLayoutValue(hash, 0);

    hash = HashScopeLayoutValue(hash, node->ID);
    hash = HashScopeLayoutValue(hash, static_cast<unsigned long long>(node->SplitAxis + 1));
    hash = HashScopeLayoutFloat(hash, node->Pos.x);
    hash = HashScopeLayoutFloat(hash, node->Pos.y);
    hash = HashScopeLayoutFloat(hash, node->SizeRef.x);
    hash = HashScopeLayoutFloat(hash, node->SizeRef.y);
    hash = HashScopeLayoutValue(hash, static_cast<unsigned long long>(node->Windows.Size));
    for(int i = 0; i < node->Windows.Size; ++i)
    {
        hash = HashScopeLayoutValue(hash, node->Windows[i] != NULL ? node->Windows[i]->ID : 0);
    }

    hash = GetDockLayoutSignature(node->ChildNodes[0], hash);
    return GetDockLayoutSignature(node->ChildNodes[1], hash);
}

void ofxOceanodeScope::addScopeRenderer(
    const std::string& valueType,
    scopeDrawFunc renderer,
    bool replaceExisting
)
{
    if(valueType.empty() || !renderer) return;

    auto existing = typedRenderers.find(valueType);
    if(existing != typedRenderers.end())
    {
        if(replaceExisting) existing->second->draw = std::move(renderer);
        return;
    }

    auto registeredRenderer = std::make_shared<ofxOceanodeScopeRenderer>();
    registeredRenderer->draw = std::move(renderer);
    typedRenderers.emplace(valueType, std::move(registeredRenderer));
}

void ofxOceanodeScope::addScopeFunc(scopeFunc f)
{
    if(f) legacyScopeTypes.emplace_back(std::move(f));
}

const std::vector<ofxOceanodeScope::scopeFunc>& ofxOceanodeScope::getScopedTypes()
{
    // Preserve the old public API without maintaining duplicate wrappers for
    // every typed renderer. New internal code calls drawParameter() directly.
    if(compatibilityScopeTypes.empty())
    {
        compatibilityScopeTypes.emplace_back([this](ofxOceanodeAbstractParameter* p, ImVec2 size){
            return drawParameter(p, size);
        });
    }
    return compatibilityScopeTypes;
}

std::shared_ptr<ofxOceanodeScopeRenderer> ofxOceanodeScope::findRenderer(
    const ofxOceanodeAbstractParameter* p
) const
{
    if(p == nullptr) return nullptr;
    auto renderer = typedRenderers.find(p->valueType());
    return renderer != typedRenderers.end() ? renderer->second : nullptr;
}

bool ofxOceanodeScope::canScope(const ofxOceanodeAbstractParameter* p) const
{
    if(p == nullptr) return false;
    if(findRenderer(p) != nullptr) return true;

    // A legacy callback combines matching and drawing, so probing it would
    // render into the menu. Preserve source compatibility by treating legacy
    // registrations as potentially capable; typed registrations are exact.
    return !legacyScopeTypes.empty();
}

bool ofxOceanodeScope::drawParameter(
    ofxOceanodeAbstractParameter* p,
    ImVec2 size,
    const std::shared_ptr<ofxOceanodeScopeRenderer>& cachedRenderer
) const
{
    if(p == nullptr) return false;

    if(cachedRenderer != nullptr && cachedRenderer->draw)
    {
        cachedRenderer->draw(p, size);
        return true;
    }

    const auto renderer = findRenderer(p);
    if(renderer != nullptr && renderer->draw)
    {
        renderer->draw(p, size);
        return true;
    }

    for(const auto& legacyRenderer : legacyScopeTypes)
    {
        if(legacyRenderer(p, size)) return true;
    }
    return false;
}

bool ofxOceanodeScope::drawParameter(ofxOceanodeAbstractParameter* p, ImVec2 size) const
{
    return drawParameter(p, size, nullptr);
}

void ofxOceanodeScope::setup(){
    addScopeRenderer(typeid(std::vector<float>).name(), [](ofxOceanodeAbstractParameter *p, ImVec2 size){
        auto& param = p->cast<std::vector<float>>().getParameter();

        if(param->size() == 1 && size.x > size.y)
        {
            ImGui::ProgressBar((param.get()[0] - param.getMin()[0]) / (param.getMax()[0] - param.getMin()[0]), size, "");

            if(ImGui::IsItemHovered()){
                ImGui::BeginTooltip();
                ImGui::Text("%3f", param.get()[0]);
                ImGui::EndTooltip();
            }
        }else if(param->size()>0){
            if(param.getMin()[0] == std::numeric_limits<float>::lowest() || param.getMax()[0] == std::numeric_limits<float>::max()){
                ImGui::PlotHistogram("##histplot", &param.get()[0], param->size(), 0, NULL, *std::min_element(param->begin(), param->end()), *std::max_element(param->begin(), param->end()), size);
            }else{
                ImGui::PlotHistogram("##histplot", &param.get()[0], param->size(), 0, NULL, param.getMin()[0], param.getMax()[0], size);
            }
        }
    }, false);

    addScopeRenderer(typeid(float).name(), [](ofxOceanodeAbstractParameter *p, ImVec2 size){
        auto& param = p->cast<float>().getParameter();
        ImGui::ProgressBar((param.get() - param.getMin()) / (param.getMax() - param.getMin()), size, "");

        if(ImGui::IsItemHovered()){
            ImGui::BeginTooltip();
            ImGui::Text("%3f", param.get());
            ImGui::EndTooltip();
        }
    }, false);
}

void ofxOceanodeScope::draw(){

    if(scopedParameters.size() > 0){
        const bool mouseDown = ImGui::IsMouseDown(0);
        const bool runDockMaintenance = !mouseDown
            && (dockMaintenancePending || scopeInteractionInProgress);

        ImGuiWindowClass window_class;
        window_class.ClassId = ImGui::GetID("ScopesClass");
        window_class.DockingAllowUnclassed = false;

        // Do NOT set the window class for the main "Scopes" window
        // so it can be docked anywhere in the main application
        ImGui::Begin("Scopes", NULL, ImGuiWindowFlags_NoScrollbar);
        
        // Apply saved window configuration on first frame after load
        if(windowConfig.hasConfig){
            ImGui::SetWindowPos(ImVec2(windowConfig.posX, windowConfig.posY), ImGuiCond_Always);
            ImGui::SetWindowSize(ImVec2(windowConfig.width, windowConfig.height), ImGuiCond_Always);
            windowConfig.hasConfig = false; // Only apply once
        }
        
        ImGuiID dockspace_id = ImGui::GetID("ScopesDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None, &window_class);

        // Manual additions can happen before this dockspace exists. Process
        // one queued split now. One per frame ensures any earlier queued
        // window exists before the next split has to move it.
        if(!pendingDockWindows.empty() && !isLoadingFromPreset)
        {
            const auto& pending = pendingDockWindows.front();
            if(DockScopeWindowAtBottom(
                pending.windowName,
                dockspace_id,
                pending.scopeCountAtAdd
            ))
            {
                pendingDockWindows.erase(pendingDockWindows.begin());
                dockMaintenancePending = true;
            }
        }

        ImGuiDockNode* dockRootNode = ImGui::DockBuilderGetNode(dockspace_id);

        // Take a dock-tree snapshot only when an interaction begins. Comparing
        // it on release detects split/dock changes without an O(N) tree walk in
        // steady-state frames.
        if(mouseDown
            && !scopeInteractionInProgress
            && ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows))
        {
            scopeInteractionInProgress = true;
            scopeWindowRectChangedDuringInteraction = false;
            dockLayoutSignatureAtInteractionStart = GetDockLayoutSignature(dockRootNode);
        }

        ImVec2 currentPos = ImGui::GetWindowPos();
        ImVec2 currentSize = ImGui::GetWindowSize();
        
        if(lastWindowConfig.hasConfig){
            bool posChanged = (currentPos.x != lastWindowConfig.posX || currentPos.y != lastWindowConfig.posY);
            bool sizeChanged = (currentSize.x != lastWindowConfig.width || currentSize.y != lastWindowConfig.height);
            
            // Proportional dock layout rescaling.
            // When the Scopes window is resized, walk the dock-tree and rewrite
            // each split node's SizeRef so the children keep their share of the
            // parent. Without this, ImGui keeps one child at its absolute pixel
            // size and dumps all extra/missing space onto the other child.
            if(sizeChanged){
                ImGuiDockNode* rootNode = ImGui::DockBuilderGetNode(dockspace_id);
                if(rootNode != NULL){
                    ImVec2 oldSize(lastWindowConfig.width, lastWindowConfig.height);
                    ImVec2 newSize = currentSize;
                    RescaleDockNodeProportional(rootNode, oldSize, newSize);
                }
            }
            
            if(posChanged || sizeChanged){
                if(scopeInteractionInProgress)
                {
                    scopeWindowRectChangedDuringInteraction = true;
                }
            }
        }
        
        // Update last window config for next frame
        lastWindowConfig.hasConfig = true;
        lastWindowConfig.posX = currentPos.x;
        lastWindowConfig.posY = currentPos.y;
        lastWindowConfig.width = currentSize.x;
        lastWindowConfig.height = currentSize.y;
        
        ImGui::End();

        for(int i = 0; i < scopedParameters.size(); i++)
        {
            auto &p = scopedParameters[i];
            
            const std::string& windowName = p.windowName;

            bool open = true;
            
            // We want them to be dockable within the class, but not become floating windows outside the main app.
            // ImGuiDockNodeFlags_NoUndocking prevents them from being moved AT ALL once docked.
            // Instead, we rely on DockingAlwaysTabBar and DockingAllowUnclassed=false to keep them contained.
            // To prevent floating, we can use ImGuiWindowFlags_NoMove on the window itself, but that might prevent dragging tabs.
            // Actually, ImGui handles this: if DockingAllowUnclassed is false, it can only dock into nodes of the same class.
            // If we want to prevent it from being dragged outside to become a floating window, we can set DockingAlwaysTabBar.
            window_class.DockingAlwaysTabBar = false;
            window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_None; // Remove NoUndocking so they can be rearranged
            
            ImGui::SetNextWindowClass(&window_class);
            
            // Guide first-time windows into the dockspace initially.
            ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);

            if(ImGui::Begin(windowName.c_str(), &open))
            {
                if(mouseDown
                    && !scopeInteractionInProgress
                    && ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows))
                {
                    scopeInteractionInProgress = true;
                    scopeWindowRectChangedDuringInteraction = false;
                    dockLayoutSignatureAtInteractionStart = GetDockLayoutSignature(
                        ImGui::DockBuilderGetNode(dockspace_id)
                    );
                }

                ImGui::PushStyleColor(ImGuiCol_SliderGrab,ImVec4(p.color*0.75f));
                ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,ImVec4(p.color*0.75f));
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram,ImVec4(p.color*0.75f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                ImGui::PushStyleColor(ImGuiCol_Border, OceanodeColors::TransparentButton);
                
                drawParameter(p.parameter, ImGui::GetContentRegionAvail(), p.renderer);
                
                ImGui::PopStyleColor(5);
            }
            ImGui::End();

            if(!open)
            {
                ofxOceanodeScope::getInstance()->removeParameter(p.parameter);
                i--; // Adjust index since we removed an element
            }
            else if(runDockMaintenance)
            {
                // After the window has been drawn, check if it ended up floating outside
                // the Scopes dockspace. If so (and the user is not actively dragging it),
                // force it back into the Scopes dockspace for the next frame.
                // We use the internal SetWindowDock() directly rather than SetNextWindowDockID()
                // to avoid racing with ImGui's own drag-and-drop docking requests.
                ImGuiWindow* scopeWindow = ImGui::FindWindowByName(windowName.c_str());
                if(scopeWindow)
                {
                    bool isFloatingOutsideScopes = false;
                    if (scopeWindow->DockNode == NULL)
                    {
                        isFloatingOutsideScopes = true;
                    }
                    else
                    {
                        ImGuiDockNode* rootNode = ImGui::DockNodeGetRootNode(scopeWindow->DockNode);
                        if (rootNode->ID != dockspace_id)
                        {
                            isFloatingOutsideScopes = true;
                        }
                    }
                    
                    if (isFloatingOutsideScopes)
                    {
                        ReturnScopeWindowToTop(
                            scopeWindow,
                            dockspace_id,
                            scopedParameters.size()
                        );
                    }
                }
            }
        }

        // ImGui deliberately keeps the central dock node alive when its last
        // window is dragged elsewhere. For the Scopes dockspace that leaves a
        // permanent patch of empty background. Once the drag has finished,
        // move the first remaining visible scope in spatial reading order into
        // the empty central node: top to bottom, and left to right within each
        // row. Moving a single-window non-central leaf makes ImGui merge that
        // leaf with its sibling automatically.
        if(runDockMaintenance)
        {
            ImGuiDockNode* rootNode = ImGui::DockBuilderGetNode(dockspace_id);
            ImGuiDockNode* centralNode = rootNode != NULL ? rootNode->CentralNode : NULL;
            ImGuiWindow* centralScopeWindow = FindCurrentScopeWindowInNode(
                scopedParameters,
                centralNode
            );

            if(centralScopeWindow != NULL)
            {
                lastCentralScopeWindowID = centralScopeWindow->ID;
            }
            else if(
                centralNode != NULL
                && centralNode->IsLeafNode()
                && !scopedParameters.empty()
                && !IsScopeWindowPendingDock(scopedParameters, centralNode->ID)
            )
            {
                ImGuiWindow* donor = FindScopeWindowToFillCentralNode(
                    scopedParameters,
                    rootNode,
                    lastCentralScopeWindowID
                );

                // If the remembered central window is the only available
                // candidate, filling the hole is preferable to leaving it.
                if(donor == NULL)
                {
                    donor = FindScopeWindowToFillCentralNode(
                        scopedParameters,
                        rootNode,
                        0
                    );
                }

                if(donor != NULL)
                {
                    const ImGuiID donorWindowID = donor->ID;

                    // Removing a single-window donor may merge its old node
                    // and invalidate the previous central-node pointer/ID.
                    // Undock first, then reacquire the central node before
                    // assigning the new dock destination.
                    ImGui::DockContextProcessUndockWindow(GImGui, donor, true);
                    rootNode = ImGui::DockBuilderGetNode(dockspace_id);
                    centralNode = rootNode != NULL ? rootNode->CentralNode : NULL;
                    if(centralNode != NULL)
                    {
                        ImGui::SetWindowDock(donor, centralNode->ID, ImGuiCond_Always);
                        lastCentralScopeWindowID = donorWindowID;
                    }
                }
            }

            dockMaintenancePending = false;

            if(scopeInteractionInProgress)
            {
                const unsigned long long currentSignature = GetDockLayoutSignature(
                    ImGui::DockBuilderGetNode(dockspace_id)
                );
                if(scopeWindowRectChangedDuringInteraction
                    || currentSignature != dockLayoutSignatureAtInteractionStart)
                {
                    notifyScopeChanged();
                }

                scopeInteractionInProgress = false;
                scopeWindowRectChangedDuringInteraction = false;
            }
        }
    }
}

bool ofxOceanodeScope::addParameter(
    ofxOceanodeAbstractParameter* p,
    ofColor _color,
    const std::string& canvasID,
    const std::string& nodeName
){
    if(p == nullptr || scopedParameterSet.count(p) != 0 || !canScope(p)) return false;
    
    // If canvasID/nodeName not provided, extract from parameter
    std::string actualCanvasID = canvasID;
    std::string actualNodeName = nodeName;
    
    if(actualCanvasID.empty() || actualNodeName.empty()) {
        if(p->getNodeModel() != nullptr) {
            actualCanvasID = p->getNodeModel()->getParents();
            
            auto hierarchyNames = p->getGroupHierarchyNames();
            if(!hierarchyNames.empty()) {
                actualNodeName = hierarchyNames.front();
            }
        }
    }
    
    auto renderer = findRenderer(p);
    ofxOceanodeScopeItem item(
        p,
        _color,
        actualCanvasID,
        actualNodeName,
        "",
        renderer
    );
    item.windowName = BuildScopeWindowName(item);
    scopedParameters.emplace_back(std::move(item));
    scopedParameterSet.insert(p);
    p->setScoped(true);

    // DockBuilder may not exist at add time. Queue one cold-path action for
    // the next draw, where the dockspace is guaranteed to have been submitted.
    if(!isLoadingFromPreset)
    {
        PendingDockWindow pending;
        pending.windowName = scopedParameters.back().windowName;
        pending.scopeCountAtAdd = scopedParameters.size();
        pendingDockWindows.emplace_back(std::move(pending));
        dockMaintenancePending = true;
    }
    
    notifyScopeChanged();
    return true;
}

bool ofxOceanodeScope::removeParameter(ofxOceanodeAbstractParameter* p){
    if(p == nullptr || scopedParameterSet.count(p) == 0) return false;

    auto scopeToRemove = std::find_if(scopedParameters.begin(), scopedParameters.end(), [p](const ofxOceanodeScopeItem& i){return i.parameter == p;});
    if(scopeToRemove == scopedParameters.end())
    {
        scopedParameterSet.erase(p);
        p->setScoped(false);
        return false;
    }

    const std::string removedWindowName = scopeToRemove->windowName;
    p->setScoped(false);
    scopedParameters.erase(scopeToRemove);
    scopedParameterSet.erase(p);
    pendingDockWindows.erase(
        std::remove_if(
            pendingDockWindows.begin(),
            pendingDockWindows.end(),
            [&removedWindowName](const PendingDockWindow& pending){
                return pending.windowName == removedWindowName;
            }
        ),
        pendingDockWindows.end()
    );
    dockMaintenancePending = true;
	notifyScopeChanged();
    return true;
}

ofxOceanodeScopeState ofxOceanodeScope::getScopeState() const {
    ofxOceanodeScopeState state;

    // Never ask ImGui for the "current" window here: save calls may originate
    // from any UI context. draw() is the only place that samples Scopes itself.
    state.windowConfig = lastWindowConfig.hasConfig ? lastWindowConfig : windowConfig;

    state.parameters.reserve(scopedParameters.size());
    for(const auto& item : scopedParameters) {
        if(item.parameter == nullptr) continue;

        ofxOceanodeScopeParameterData paramData;
        paramData.canvasID = item.canvasID;
        paramData.nodeName = item.cachedNodeName;
        paramData.paramName = item.parameter->getName();
        paramData.parameterPath = item.cachedNodeName + "/" + item.parameter->getName();
        state.parameters.emplace_back(std::move(paramData));
    }
	return state;
}

void ofxOceanodeScope::setScopeState(const ofxOceanodeScopeState& state) {
    // Clear existing parameters
    clearScopedParameters();
    
    // Set window config for next frame
    setWindowConfig(state.windowConfig);
    
    // NOTE: Parameters are NOT resolved here!
    // Container will call addParameter() for each resolved parameter
}

void ofxOceanodeScope::clearScopedParameters() {
    for(auto& item : scopedParameters) {
        if(item.parameter != nullptr) item.parameter->setScoped(false);
    }
    scopedParameters.clear();
    scopedParameterSet.clear();
    pendingDockWindows.clear();
    lastCentralScopeWindowID = 0;
    dockMaintenancePending = true;
    scopeInteractionInProgress = false;
    scopeWindowRectChangedDuringInteraction = false;
}

ofxOceanodeScopeWindowConfig ofxOceanodeScope::getWindowConfig() const {
    return lastWindowConfig.hasConfig ? lastWindowConfig : windowConfig;
}

// Helper method implementations for full path display
std::string ofxOceanodeScopeItem::getFullPath() const {
    std::string fullPath;
    
    // Build path from canvasID hierarchy
    if(!canvasID.empty() && canvasID != "0") {
        // Parse canvasID to build readable macro path
        // Example: "0.2.5" -> "Macro2 > Macro5 > "
        vector<string> levels = ofSplitString(canvasID, ".");
        
        // Skip the root "0" level
        for(size_t i = 1; i < levels.size(); i++) {
            fullPath += "Macro" + levels[i] + " > ";
        }
    }
    
    // Append node name and parameter name
    fullPath += cachedNodeName + " / " + parameter->getName();
    
    return fullPath;
}

std::string ofxOceanodeScopeParameterData::getFullPath() const {
    std::string fullPath;
    
    if(!canvasID.empty() && canvasID != "0") {
        vector<string> levels = ofSplitString(canvasID, ".");
        for(size_t i = 1; i < levels.size(); i++) {
            fullPath += "Macro" + levels[i] + " > ";
        }
    }
    
    fullPath += nodeName + " / " + paramName;
    return fullPath;
}

void ofxOceanodeScope::setWindowConfig(const ofxOceanodeScopeWindowConfig& config) {
    windowConfig = config;
    lastWindowConfig.hasConfig = false;
}

void ofxOceanodeScope::setScopeChangedCallback(ScopeChangedCallback callback) {
    scopeChangedCallback = callback;
}

void ofxOceanodeScope::notifyScopeChanged() {
    if(scopeChangedCallback) {
        scopeChangedCallback();
    }
}

// Serialization helpers for ofxOceanodeScopeState
ofJson ofxOceanodeScopeState::toJson() const {
    ofJson json;
    json["version"] = 2;
    
    // Window config
    if(windowConfig.hasConfig) {
        json["window"]["posX"] = windowConfig.posX;
        json["window"]["posY"] = windowConfig.posY;
        json["window"]["width"] = windowConfig.width;
        json["window"]["height"] = windowConfig.height;
    }
    
    // Parameters
    json["parameters"] = ofJson::array();
    for(const auto& param : parameters) {
        ofJson paramJson;
        
        // New format
        paramJson["canvasID"] = param.canvasID;
        paramJson["nodeName"] = param.nodeName;
        paramJson["paramName"] = param.paramName;
        
        // Older builds can still consume newly-written scope files.
        paramJson["path"] = param.getLegacyPath();
        
        json["parameters"].push_back(paramJson);
    }
    
    return json;
}

ofxOceanodeScopeState ofxOceanodeScopeState::fromJson(const ofJson& json) {
    ofxOceanodeScopeState state;
        
    // Window config
    if(json.contains("window") && json["window"].is_object()) {
        const auto& windowJson = json["window"];
        auto readNumber = [&windowJson](const char* key, float fallback) {
            auto value = windowJson.find(key);
            return value != windowJson.end() && value->is_number()
                ? value->get<float>()
                : fallback;
        };

        state.windowConfig.hasConfig = true;
        state.windowConfig.posX = readNumber("posX", state.windowConfig.posX);
        state.windowConfig.posY = readNumber("posY", state.windowConfig.posY);
        state.windowConfig.width = readNumber("width", state.windowConfig.width);
        state.windowConfig.height = readNumber("height", state.windowConfig.height);
    }
    
    // Parameters
    if(json.contains("parameters") && json["parameters"].is_array()) {
        for(const auto& paramJson : json["parameters"]) {
            if(!paramJson.is_object()) continue;

            ofxOceanodeScopeParameterData data;
            bool valid = false;

            // Try new format first
            if(paramJson.contains("canvasID") && paramJson["canvasID"].is_string()
                && paramJson.contains("nodeName") && paramJson["nodeName"].is_string()
                && paramJson.contains("paramName") && paramJson["paramName"].is_string())
            {
                data.canvasID = paramJson["canvasID"].get<std::string>();
                data.nodeName = paramJson["nodeName"].get<std::string>();
                data.paramName = paramJson["paramName"].get<std::string>();
                data.parameterPath = data.nodeName + "/" + data.paramName;
                valid = !data.nodeName.empty() && !data.paramName.empty();
            }

            // Legacy files contain only "path". Split on the first slash so
            // the old node/parameter resolution path remains usable.
            if(!valid && paramJson.contains("path") && paramJson["path"].is_string())
            {
                data.parameterPath = paramJson["path"].get<std::string>();
                const size_t slash = data.parameterPath.find('/');
                if(slash != std::string::npos && slash > 0 && slash + 1 < data.parameterPath.size())
                {
                    data.nodeName = data.parameterPath.substr(0, slash);
                    data.paramName = data.parameterPath.substr(slash + 1);
                    data.canvasID.clear();
                    valid = true;
                }
            }

            if(valid) state.parameters.emplace_back(std::move(data));
        }
    }
    return state;
}
