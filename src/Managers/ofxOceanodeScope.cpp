//
//  ofxOceanodeScope.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 05/05/2020.
//
#define IMGUI_DEFINE_MATH_OPERATORS
#include "ofxOceanodeScope.h"
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

static std::string GetScopeWindowName(const ofxOceanodeScopeItem& item)
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
        ImGuiWindow* window = ImGui::FindWindowByName(GetScopeWindowName(item).c_str());
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
        ImGuiWindow* window = ImGui::FindWindowByName(GetScopeWindowName(item).c_str());
        if(window != NULL && window->DockNode == NULL && window->DockId == dockNodeID) return true;
    }

    return false;
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
    ImGuiID remainingNodeID = 0;
    ImGui::DockBuilderSplitNode(
        dockspaceID,
        ImGuiDir_Up,
        topRatio,
        &topNodeID,
        &remainingNodeID
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

void ofxOceanodeScope::setup(){
    scopeTypes.push_back([](ofxOceanodeAbstractParameter *p, ImVec2 size) -> bool{
        // VECTOR FLOAT PARAM
        if(p->valueType() == typeid(std::vector<float>).name())
        {
            auto param = p->cast<std::vector<float>>().getParameter();
            auto size2 = ImGui::GetContentRegionAvail();

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
            return true;
        }

        return false;
    });
    scopeTypes.push_back([](ofxOceanodeAbstractParameter *p, ImVec2 size) -> bool{
        // FLOAT PARAM
        if(p->valueType() == typeid(float).name())
        {
            auto param = p->cast<float>().getParameter();
            auto size2 = ImGui::GetContentRegionAvail();

            ImGui::ProgressBar((param.get() - param.getMin()) / (param.getMax() - param.getMin()), size, "");
            
            if(ImGui::IsItemHovered()){
                ImGui::BeginTooltip();
                ImGui::Text("%3f", param.get());
                ImGui::EndTooltip();
            }
            return true;
        }
        return false;
    });
}

void ofxOceanodeScope::draw(){

    if(scopedParameters.size() > 0){
        ImGuiWindowClass window_class;
        window_class.ClassId = ImGui::GetID("ScopesClass");
        window_class.DockingAllowUnclassed = false;

        // Do NOT set the window class for the main "Scopes" window
        // so it can be docked anywhere in the main application
        ImGui::Begin("Scopes", NULL, ImGuiWindowFlags_NoScrollbar);
        
        // Apply saved window configuration on first frame after load
        if(windowConfig.hasConfig){
            ImGui::SetWindowPos(ImVec2(windowConfig.posX, windowConfig.posY), ImGuiCond_Once);
            ImGui::SetWindowSize(ImVec2(windowConfig.width, windowConfig.height), ImGuiCond_Once);
            windowConfig.hasConfig = false; // Only apply once
        }
        
        ImGuiID dockspace_id = ImGui::GetID("ScopesDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None, &window_class);

        // Check for window position/size changes and auto-save
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
                // Auto-save after window position/size change
                notifyScopeChanged();
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
            
            std::string windowName = GetScopeWindowName(p);

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
                ImGui::PushStyleColor(ImGuiCol_SliderGrab,ImVec4(p.color*0.75f));
                ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,ImVec4(p.color*0.75f));
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram,ImVec4(p.color*0.75f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                ImGui::PushStyleColor(ImGuiCol_Border, OceanodeColors::TransparentButton);
                
                // f() function to properly draw each scope item
                for(auto f : scopeTypes)
                {
                    if(f(p.parameter, ImGui::GetContentRegionAvail())) break;
                }
                
                ImGui::PopStyleColor(5);
            }
            ImGui::End();

            if(!open)
            {
                ofxOceanodeScope::getInstance()->removeParameter(p.parameter);
                // Auto-save is called inside removeParameter()
                i--; // Adjust index since we removed an element
            }
            else
            {
                // After the window has been drawn, check if it ended up floating outside
                // the Scopes dockspace. If so (and the user is not actively dragging it),
                // force it back into the Scopes dockspace for the next frame.
                // We use the internal SetWindowDock() directly rather than SetNextWindowDockID()
                // to avoid racing with ImGui's own drag-and-drop docking requests.
                ImGuiWindow* scopeWindow = ImGui::FindWindowByName(windowName.c_str());
                if (scopeWindow && !ImGui::IsMouseDown(0))
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
        if(!ImGui::IsMouseDown(0))
        {
            ImGuiDockNode* rootNode = ImGui::DockBuilderGetNode(dockspace_id);
            ImGuiDockNode* centralNode = rootNode != NULL ? rootNode->CentralNode : NULL;

            if(centralNode != NULL && centralNode->Windows.Size > 0)
            {
                ImGuiWindow* centralWindow = centralNode->VisibleWindow != NULL
                    ? centralNode->VisibleWindow
                    : centralNode->Windows[0];
                lastCentralScopeWindowID = centralWindow->ID;
            }
            else if(
                centralNode != NULL
                && centralNode->IsLeafNode()
                && scopedParameters.size() > 1
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
        }
    }
}

void ofxOceanodeScope::addParameter(
    ofxOceanodeAbstractParameter* p,
    ofColor _color,
    const std::string& canvasID,
    const std::string& nodeName
){
    p->setScoped(true);
    
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
    
    scopedParameters.emplace_back(p, _color, 1.0f, actualCanvasID, actualNodeName);
    
    // When adding a new scope manually, split the dockspace vertically
    // so it appears below the existing ones instead of as a tab.
    // Skip this during preset loading: ImGuiLayout.ini already contains
    // the correct dock-tree, and rebuilding it here would corrupt the layout.
    if(!isLoadingFromPreset && scopedParameters.size() > 1) {
        ImGuiID dockspace_id = ImGui::GetID("ScopesDockSpace");
        
        // Get the full path of the newly added parameter to construct its window name
        std::string windowName = GetScopeWindowName(scopedParameters.back());
        
        // Only split if the dockspace node actually exists (it might not exist yet if the window hasn't been drawn)
        if(ImGui::DockBuilderGetNode(dockspace_id) != NULL) {
            // Split the dockspace node downwards
            ImGuiID dock_main_id = dockspace_id;
            ImGuiID dock_id_bottom;
            ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.5f, &dock_id_bottom, &dock_main_id);
            
            // Dock the new window into the bottom split
            ImGui::DockBuilderDockWindow(windowName.c_str(), dock_id_bottom);
        }
    }
    
    // Auto-save after adding parameter
    notifyScopeChanged();
}

void ofxOceanodeScope::removeParameter(ofxOceanodeAbstractParameter* p){
    p->setScoped(false);
    auto scopeToRemove = std::find_if(scopedParameters.begin(), scopedParameters.end(), [p](const ofxOceanodeScopeItem& i){return i.parameter == p;});
    float sizeBackup = scopeToRemove->sizeRelative;
    scopedParameters.erase(scopeToRemove);
    for (auto &sp : scopedParameters) {
        sp.sizeRelative += ((sizeBackup - 1) / scopedParameters.size());
    }
	// Auto-save after adding parameter
	notifyScopeChanged();
}

ofxOceanodeScopeState ofxOceanodeScope::getScopeState() const {
    ofxOceanodeScopeState state;
	ImGuiContext& g = *GImGui;
	
	// to avoid bad access when closing app, we check if there is an ImGui context window before getting scope state as it might be non existing
	if(g.CurrentWindow!=NULL)
	{
		// Export window config (from ImGui if window is open)
		if(scopedParameters.size() > 0) {
			state.windowConfig.hasConfig = true;
			state.windowConfig.posX = ImGui::GetWindowPos().x;
			state.windowConfig.posY = ImGui::GetWindowPos().y;
			state.windowConfig.width = ImGui::GetWindowSize().x;
			state.windowConfig.height = ImGui::GetWindowSize().y;
		} else {
			state.windowConfig = windowConfig;
		}
		
		// Export parameter data
		for(const auto& item : scopedParameters) {
			ofxOceanodeScopeParameterData paramData;
			
			// New format with full path
			paramData.canvasID = item.canvasID;
			paramData.nodeName = item.cachedNodeName;
			paramData.paramName = item.parameter->getName();
			paramData.sizeRelative = item.sizeRelative;
			
			// Backward compatibility: also set legacy path
			paramData.parameterPath = item.cachedNodeName + "/" + item.parameter->getName();
			
			state.parameters.push_back(paramData);
		}
		
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
        item.parameter->setScoped(false);
    }
    scopedParameters.clear();
    lastCentralScopeWindowID = 0;
}

ofxOceanodeScopeWindowConfig ofxOceanodeScope::getWindowConfig() const {
    if(scopedParameters.size() > 0) {
        // Get current window state from ImGui
        ofxOceanodeScopeWindowConfig config;
        config.hasConfig = true;
        config.posX = ImGui::GetWindowPos().x;
        config.posY = ImGui::GetWindowPos().y;
        config.width = ImGui::GetWindowSize().x;
        config.height = ImGui::GetWindowSize().y;
        return config;
    }
    return windowConfig;
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
        paramJson["sizeRelative"] = param.sizeRelative;
        
        // Backward compatibility: also save legacy path
        paramJson["path"] = param.getLegacyPath();
        
        json["parameters"].push_back(paramJson);
    }
    
    return json;
}

ofxOceanodeScopeState ofxOceanodeScopeState::fromJson(const ofJson& json) {
    ofxOceanodeScopeState state;
        
    // Window config
    if(json.contains("window")) {
        state.windowConfig.hasConfig = true;
        state.windowConfig.posX = json["window"]["posX"];
        state.windowConfig.posY = json["window"]["posY"];
        state.windowConfig.width = json["window"]["width"];
        state.windowConfig.height = json["window"]["height"];
    }
    
    // Parameters
    if(json.contains("parameters") && json["parameters"].is_array()) {
        int paramIndex = 0;
        for(const auto& paramJson : json["parameters"]) {
            ofxOceanodeScopeParameterData data;
            // Try new format first
            if(paramJson.contains("canvasID") && paramJson.contains("nodeName") && paramJson.contains("paramName")) {
                data.canvasID = paramJson["canvasID"];
                data.nodeName = paramJson["nodeName"];
                data.paramName = paramJson["paramName"];
                // Populate parameterPath for backward compatibility with resolution logic
                data.parameterPath = data.nodeName + "/" + data.paramName;
			}
            
            if(paramJson.contains("sizeRelative")) {
                data.sizeRelative = paramJson["sizeRelative"];
            } else {
                data.sizeRelative = 1.0f;
            }
            
            state.parameters.push_back(data);
            paramIndex++;
        }
    }
    return state;
}
