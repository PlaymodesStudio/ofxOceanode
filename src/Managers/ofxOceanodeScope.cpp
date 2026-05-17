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

// Helper to recursively scale SizeRef of all dock nodes in a tree.
// This ensures docked scope windows maintain their relative proportions
// when the parent "Scopes" window is resized.
static void ScaleDockNodeSizeRef(ImGuiDockNode* node, float scaleX, float scaleY) {
    if (!node) return;
    node->SizeRef.x *= scaleX;
    node->SizeRef.y *= scaleY;
    if (node->ChildNodes[0]) ScaleDockNodeSizeRef(node->ChildNodes[0], scaleX, scaleY);
    if (node->ChildNodes[1]) ScaleDockNodeSizeRef(node->ChildNodes[1], scaleX, scaleY);
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
        
        // Proportional scaling: when the Scopes window is resized, scale all dock nodes'
        // SizeRef so that the internal layout maintains its relative proportions.
        ImVec2 currentWindowSize = ImGui::GetWindowSize();
        if (lastDockspaceWidth > 0.0f && lastDockspaceHeight > 0.0f) {
            float scaleX = currentWindowSize.x / lastDockspaceWidth;
            float scaleY = currentWindowSize.y / lastDockspaceHeight;
            if (scaleX != 1.0f || scaleY != 1.0f) {
                ImGuiID dockspace_id = ImGui::GetID("ScopesDockSpace");
                if (ImGuiDockNode* rootNode = ImGui::DockBuilderGetNode(dockspace_id)) {
                    ScaleDockNodeSizeRef(rootNode, scaleX, scaleY);
                }
            }
        }
        
        ImGuiID dockspace_id = ImGui::GetID("ScopesDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None, &window_class);

        // Check for window position/size changes and auto-save
        ImVec2 currentPos = ImGui::GetWindowPos();
        ImVec2 currentSize = ImGui::GetWindowSize();
        
        if(lastWindowConfig.hasConfig){
            bool posChanged = (currentPos.x != lastWindowConfig.posX || currentPos.y != lastWindowConfig.posY);
            bool sizeChanged = (currentSize.x != lastWindowConfig.width || currentSize.y != lastWindowConfig.height);
            
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
        
        // Update last dockspace size for proportional scaling on next resize
        lastDockspaceWidth = currentWindowSize.x;
        lastDockspaceHeight = currentWindowSize.y;
        
        ImGui::End();

        for(int i = 0; i < scopedParameters.size(); i++)
        {
            auto &p = scopedParameters[i];
            
            std::string fullPath = p.getFullPath();
            std::string windowName = p.canvasID == "Canvas" ? fullPath : (p.canvasID + " / " + fullPath);
            windowName += "###Scope_" + p.canvasID + "_" + fullPath;

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
            
            // We also need to ensure it docks into the dockspace initially if it's not already docked
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
    
    // When adding a new scope, we want to split the dockspace vertically
    // so it appears below the existing ones instead of as a tab.
    if(scopedParameters.size() > 1) {
        ImGuiID dockspace_id = ImGui::GetID("ScopesDockSpace");
        
        // Get the full path of the newly added parameter to construct its window name
        std::string fullPath = scopedParameters.back().getFullPath();
        std::string windowName = actualCanvasID == "Canvas" ? fullPath : (actualCanvasID + " / " + fullPath);
        windowName += "###Scope_" + actualCanvasID + "_" + fullPath;
        
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
