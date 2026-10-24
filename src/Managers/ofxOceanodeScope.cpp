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
#include "ofxOceanodeScopeProportions.h"

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

// CentralNode is a cached pointer: a merge can delete that node before ImGui
// refreshes the cache in DockSpace(). Walk the live tree after our own edits.
static ImGuiDockNode* FindLiveScopeCentralNode(ImGuiDockNode* node)
{
    if(node == NULL) return NULL;
    if(node->IsCentralNode()) return node;
    if(ImGuiDockNode* central = FindLiveScopeCentralNode(node->ChildNodes[0])) return central;
    return FindLiveScopeCentralNode(node->ChildNodes[1]);
}

static ImGuiDockNode* RefreshScopeCentralNode(ImGuiID dockspaceID)
{
    ImGuiDockNode* root = ImGui::DockBuilderGetNode(dockspaceID);
    if(root == NULL) return NULL;
    root->CentralNode = FindLiveScopeCentralNode(root);
    return root->CentralNode;
}

static bool IsScopeDockingBusy()
{
    // A delivered drop still has a request pending until the next NewFrame().
    // Mouse-up alone is not permission to split, merge or move its nodes.
    return ImGui::IsMouseDown(0) || GImGui->MovingWindow != NULL
        || ImGui::IsDragDropActive() || GImGui->DockContext.Requests.Size != 0;
}

static bool IsScopeWindowContained(ImGuiWindow* window, ImGuiID dockspaceID)
{
    return window != NULL && window->DockNode != NULL
        && ImGui::DockNodeGetRootNode(window->DockNode)->ID == dockspaceID;
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
    if(!visible || scopedParameters.empty()) return;

    // Restored scopes must not dismiss the menu used to load their preset.
    const ImGuiWindowFlags focusFlags = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)
        ? ImGuiWindowFlags_NoFocusOnAppearing : ImGuiWindowFlags_None;
    ImGuiWindowClass window_class;
    window_class.ClassId = ImGui::GetID("ScopesClass");
    window_class.DockingAllowUnclassed = false;

    // The outer container remains dockable in the main application.
    ImGui::Begin("Scopes", NULL, ImGuiWindowFlags_NoScrollbar | focusFlags);
    if(windowConfig.hasConfig){
        ImGui::SetWindowPos(ImVec2(windowConfig.posX, windowConfig.posY), ImGuiCond_Always);
        ImGui::SetWindowSize(ImVec2(windowConfig.width, windowConfig.height), ImGuiCond_Always);
        windowConfig.hasConfig = false;
    }

    const ImGuiID dockspace_id = ImGui::GetID("ScopesDockSpace");
    const bool canMaintainDocking = !isLoadingFromPreset && !IsScopeDockingBusy();
    bool topologyChanged = false;

    // All topology edits happen before DockSpace() and before any scope Begin().
    // Check containment on every idle frame, including changes applied by ImGui
    // after our previous pass. Healthy layouts are never rebuilt.
    if(canMaintainDocking)
    {
        if(ImGui::DockBuilderGetNode(dockspace_id) == NULL)
        {
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            const ImVec2 available = ImGui::GetContentRegionAvail();
            ImGui::DockBuilderSetNodeSize(dockspace_id,
                ImVec2(std::max(available.x, 4.0f), std::max(available.y, 4.0f)));
            topologyChanged = true;
        }
        RefreshScopeCentralNode(dockspace_id);

        // Bind one addition per frame so its window exists before another split.
        if(!pendingDockWindows.empty())
        {
            const auto& pending = pendingDockWindows.front();
            if(DockScopeWindowAtBottom(pending.windowName, dockspace_id,
                std::min(pending.scopeCountAtAdd, scopedParameters.size())))
            {
                pendingDockWindows.erase(pendingDockWindows.begin());
                topologyChanged = true;
                ImGui::DockBuilderFinish(dockspace_id);
            }
        }

        // Wait for queued additions before enforcing containment; their first
        // Begin() also needs a chance to restore a saved DockId.
        if(pendingDockWindows.empty())
        {
            for(const auto& item : scopedParameters)
            {
                ImGuiWindow* window = ImGui::FindWindowByName(item.windowName.c_str());
                if(window == NULL || IsScopeWindowContained(window, dockspace_id)) continue;

                // SetWindowDock() can leave a valid destination awaiting Begin().
                // Preserve that assignment rather than adding another split.
                ImGuiDockNode* destination = ImGui::DockBuilderGetNode(window->DockId);
                if(window->DockNode == NULL && destination != NULL
                    && ImGui::DockNodeGetRootNode(destination)->ID == dockspace_id) continue;

                ReturnScopeWindowToTop(window, dockspace_id, scopedParameters.size());
                RefreshScopeCentralNode(dockspace_id);
                ImGui::DockBuilderFinish(dockspace_id);
                topologyChanged = true;
            }

            ImGuiDockNode* root = ImGui::DockBuilderGetNode(dockspace_id);
            ImGuiDockNode* central = RefreshScopeCentralNode(dockspace_id);
            ImGuiWindow* centralWindow = FindCurrentScopeWindowInNode(scopedParameters, central);
            if(centralWindow != NULL)
            {
                lastCentralScopeWindowID = centralWindow->ID;
            }
            else if(central != NULL && central->IsLeafNode() && central->Windows.empty())
            {
                // A central node survives when its last tab moves away. Fill it
                // using the first remaining scope in spatial reading order.
                ImGuiWindow* donor = FindScopeWindowToFillCentralNode(
                    scopedParameters, root, lastCentralScopeWindowID);
                if(donor == NULL) donor = FindScopeWindowToFillCentralNode(scopedParameters, root, 0);
                if(donor != NULL)
                {
                    ImGui::DockContextProcessUndockWindow(GImGui, donor, true);
                    central = RefreshScopeCentralNode(dockspace_id);
                    if(central != NULL)
                    {
                        ImGui::SetWindowDock(donor, central->ID, ImGuiCond_Always);
                        lastCentralScopeWindowID = donor->ID;
                    }
                    ImGui::DockBuilderFinish(dockspace_id);
                    topologyChanged = true;
                }
            }
        }
    }

    const ImVec2 currentPos = ImGui::GetWindowPos();
    const ImVec2 currentSize = ImGui::GetWindowSize();
    // Observe user layout edits every frame, but retain the desired ratios when
    // minimum sizes clamp the internal dock tree during a host collapse.
    if(!dockProportions) dockProportions = std::make_shared<ofxOceanodeScopeProportions>();
    const ImVec2 available = ImGui::GetContentRegionAvail();
    dockProportions->prepare(ImGui::DockBuilderGetNode(dockspace_id),
        ImVec2(std::max(available.x, 4.0f), std::max(available.y, 4.0f)));
    lastWindowConfig.hasConfig = true;
    lastWindowConfig.posX = currentPos.x;
    lastWindowConfig.posY = currentPos.y;
    lastWindowConfig.width = currentSize.x;
    lastWindowConfig.height = currentSize.y;

    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None, &window_class);
    dockProportions->finish();
    ImGui::End();

    for(int i = 0; i < scopedParameters.size(); i++)
    {
        auto& p = scopedParameters[i];
        bool open = true;
        // Classes restrict destinations; floating during a drag is permitted.
        // The idle maintenance phase returns scopes released outside Scopes.
        ImGui::SetNextWindowClass(&window_class);
        ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
        if(ImGui::Begin(p.windowName.c_str(), &open, focusFlags))
        {
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(p.color*0.75f));
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(p.color*0.75f));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(p.color*0.75f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::PushStyleColor(ImGuiCol_Border, OceanodeColors::TransparentButton);
            drawParameter(p.parameter, ImGui::GetContentRegionAvail(), p.renderer);
            ImGui::PopStyleColor(5);
        }
        ImGui::End();
        if(!open)
        {
            removeParameter(p.parameter);
            topologyChanged = true;
            i--;
        }
    }

    // Observe only here: drawing may have just queued a new drop. Never edit its
    // source/destination nodes, and require another idle frame after any repair.
    bool settled = canMaintainDocking && !IsScopeDockingBusy() && !topologyChanged
        && pendingDockWindows.empty();
    for(const auto& item : scopedParameters)
    {
        if(!IsScopeWindowContained(ImGui::FindWindowByName(item.windowName.c_str()), dockspace_id))
            settled = false;
    }
    ImGuiDockNode* root = ImGui::DockBuilderGetNode(dockspace_id);
    ImGuiDockNode* central = FindLiveScopeCentralNode(root);
    if(!scopedParameters.empty() && FindCurrentScopeWindowInNode(scopedParameters, central) == NULL)
        settled = false;

    if(settled)
    {
        const unsigned long long signature = GetDockLayoutSignature(root);
        if(signature != lastSettledDockLayoutSignature
            && (dockMaintenancePending || lastSettledDockLayoutSignature != 0))
            notifyScopeChanged();
        lastSettledDockLayoutSignature = signature;
    }
    dockMaintenancePending = !settled;
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
    // the next idle draw, before the dockspace is submitted.
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
    dockProportions.reset();
    dockMaintenancePending = true;
    lastSettledDockLayoutSignature = 0;
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
