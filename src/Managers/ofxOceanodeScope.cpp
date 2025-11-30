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

// https://github.com/ocornut/imgui/issues/1720
bool Splitter(int splitNum, bool split_vertically, float thickness, float* size1, float* size2, float min_size1, float min_size2, float splitter_long_axis_size = -1.0f)
{
    using namespace ImGui;
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiID id = window->GetID(("##Splitter" + ofToString(splitNum)).c_str());
    ImRect bb;
    bb.Min = window->DC.CursorPos + (split_vertically ? ImVec2(*size1, 0.0f) : ImVec2(0.0f, *size1));
    bb.Max = bb.Min + CalcItemSize(split_vertically ? ImVec2(thickness, splitter_long_axis_size) : ImVec2(splitter_long_axis_size, thickness), 0.0f, 0.0f);
    return SplitterBehavior(bb, id, split_vertically ? ImGuiAxis_X : ImGuiAxis_Y, size1, size2, min_size1, min_size2, 4.0f, 0.04f);
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
                    ImGui::PlotHistogram("", &param.get()[0], param->size(), 0, NULL, *std::min_element(param->begin(), param->end()), *std::max_element(param->begin(), param->end()), size);
                }else{
                    ImGui::PlotHistogram("", &param.get()[0], param->size(), 0, NULL, param.getMin()[0], param.getMax()[0], size);
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
        ImGui::Begin("Scopes", NULL, ImGuiWindowFlags_NoScrollbar);
        
        // Apply saved window configuration on first frame after load
        if(windowConfig.hasConfig){
            ImGui::SetWindowPos(ImVec2(windowConfig.posX, windowConfig.posY), ImGuiCond_Once);
            ImGui::SetWindowSize(ImVec2(windowConfig.width, windowConfig.height), ImGuiCond_Once);
            windowConfig.hasConfig = false; // Only apply once
        }
        
        windowWidth = ImGui::GetContentRegionAvail().x;
        windowHeight = ImGui::GetContentRegionAvail().y;
        for(int i = 0; i < scopedParameters.size()-1; i++){
            float topHeight = 0;
            float bottomHeight = 0;
                for(int j = 0; j < i+1; j++) topHeight += (scopedParameters[j].sizeRelative / scopedParameters.size() * windowHeight);
                for(int j = i+1; j < scopedParameters.size(); j++) bottomHeight += (scopedParameters[j].sizeRelative / scopedParameters.size() * windowHeight);
            float oldTopHeight = topHeight;
            float oldBottomHeight = bottomHeight;
            
            bool isShiftPresed = ImGui::GetIO().KeyShift;
            
            float minTop = isShiftPresed ? 10 * (i+1) : 10 + topHeight - scopedParameters[i].sizeRelative / scopedParameters.size() * windowHeight;
            float minBottom = isShiftPresed ? 10 * (scopedParameters.size() - (i+1)) : 10 + bottomHeight - scopedParameters[i+1].sizeRelative / scopedParameters.size() * windowHeight;
            
            if(Splitter(i, false, 1, &topHeight, &bottomHeight, minTop, minBottom)){
                if(isShiftPresed){
                    float topScale = topHeight / oldTopHeight;
                    float bottomScale = bottomHeight / oldBottomHeight;
                    for(int j = 0; j < i+1; j++) scopedParameters[j].sizeRelative *= topScale;
                    for(int j = i+1; j < scopedParameters.size(); j++) scopedParameters[j].sizeRelative *= bottomScale;
                }else{
                    float topInc = topHeight - oldTopHeight;
                    float bottomInc = bottomHeight - oldBottomHeight;
                    scopedParameters[i].sizeRelative += topInc * scopedParameters.size() / windowHeight;
                    scopedParameters[i+1].sizeRelative += bottomInc * scopedParameters.size() / windowHeight;
                }
                
                // Auto-save after splitter change
                notifyScopeChanged();
            }
        }
        for(int i = 0; i < scopedParameters.size(); i++)
        {
            auto &p = scopedParameters[i];
            auto itemHeight = (p.sizeRelative / scopedParameters.size() * windowHeight) - 2.5;

            auto size = ImVec2(ImGui::GetContentRegionAvail().x, itemHeight);
            
            ImGui::PushStyleColor(ImGuiCol_SliderGrab,ImVec4(p.color*0.75f));
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,ImVec4(p.color*0.75f));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram,ImVec4(p.color*0.75f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55,0.55,0.55,1.0));
            ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(0.0,0.0,0.0,0.0));
            
            ImGui::BeginChild(("Child_" + p.parameter->getGroupHierarchyNames().front() + "/" + p.parameter->getName()).c_str(), size, true);
            if(ImGui::Button("x##RemoveScope"))
            {
                ofxOceanodeScope::getInstance()->removeParameter(p.parameter);
                // Auto-save is called inside removeParameter()
            }
            ImGui::SameLine();
            ImGui::Text((p.parameter->getGroupHierarchyNames().front() + "/" + p.parameter->getName()).c_str());
            ImGui::SameLine();
            if(ImGui::Button("[^]##MoveScopeUp"))
            {
                if(i>0){
                    std::swap(scopedParameters[i],scopedParameters[i-1]);
                    // Auto-save after moving parameter up
                    notifyScopeChanged();
                }
            }
            ImGui::SameLine();
            if(ImGui::Button("[v]##MoveScopeDown"))
            {
                if(i<scopedParameters.size()-1){
                    std::swap(scopedParameters[i],scopedParameters[i+1]);
                    // Auto-save after moving parameter down
                    notifyScopeChanged();
                }
            }
//            ImGui::SameLine();
//            bool keepAspectRatio = (p.parameter->getFlags() & ofxOceanodeParameterFlags_ScopeKeepAspectRatio);
//            if(keepAspectRatio)ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0,0.5,0.0,1.0));
//            else ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55,0.55,0.55,1.0));
//            if(ImGui::Button("[AR]##KeepAspectRatio"))
//            {
//                
//                if(keepAspectRatio){
//                    p.parameter->setFlags(p.parameter->getFlags()&~ofxOceanodeParameterFlags_ScopeKeepAspectRatio);
//                }
//                else p.parameter->setFlags(p.parameter->getFlags()|ofxOceanodeParameterFlags_ScopeKeepAspectRatio);
//            }
//            ImGui::PopStyleColor();


            // f() function to properly draw each scope item
            for(auto f : scopeTypes)
            {
                if(f(p.parameter, ImVec2(ImGui::GetContentRegionAvail().x, itemHeight-(ImGui::GetFrameHeight()*2.1)))) break;
            }
            
            ImGui::PopStyleColor(5);
            ImGui::EndChild();
        }
        
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
        
        ImGui::End();
    }
}

void ofxOceanodeScope::addParameter(ofxOceanodeAbstractParameter* p, ofColor _color){
    p->setScoped(true);
    scopedParameters.emplace_back(p,_color);
    
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
        paramData.parameterPath = item.parameter->getGroupHierarchyNames().front() 
                                 + "/" + item.parameter->getName();
        paramData.sizeRelative = item.sizeRelative;
        state.parameters.push_back(paramData);
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
        paramJson["path"] = param.parameterPath;
        paramJson["sizeRelative"] = param.sizeRelative;
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
        for(const auto& paramJson : json["parameters"]) {
            ofxOceanodeScopeParameterData data;
            data.parameterPath = paramJson["path"];
            data.sizeRelative = paramJson["sizeRelative"];
            state.parameters.push_back(data);
        }
    }
    
    return state;
}
