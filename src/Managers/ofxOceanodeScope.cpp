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
#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeNode.h"
#include "ofUtils.h"
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
                saveScope();
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
                    saveScope();
                }
            }
            ImGui::SameLine();
            if(ImGui::Button("[v]##MoveScopeDown"))
            {
                if(i<scopedParameters.size()-1){
                    std::swap(scopedParameters[i],scopedParameters[i+1]);
                    // Auto-save after moving parameter down
                    saveScope();
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
                saveScope();
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
    saveScope();
}

void ofxOceanodeScope::removeParameter(ofxOceanodeAbstractParameter* p){
    p->setScoped(false);
    auto scopeToRemove = std::find_if(scopedParameters.begin(), scopedParameters.end(), [p](const ofxOceanodeScopeItem& i){return i.parameter == p;});
    float sizeBackup = scopeToRemove->sizeRelative;
    scopedParameters.erase(scopeToRemove);
    for (auto &sp : scopedParameters) {
        sp.sizeRelative += ((sizeBackup - 1) / scopedParameters.size());
    }
    
    // Auto-save after removing parameter
    //saveScope();
}


void ofxOceanodeScope::saveScope(const std::string& filepath){
    // Use preset path if no filepath provided
    std::string actualPath;
    if(filepath.empty()){
        std::string presetPath = ofxOceanodeShared::getCurrentPresetPath();
        if(!presetPath.empty()){
            actualPath = presetPath + "/scope_config.json";
        } else {
            actualPath = ofToDataPath(saveFilePath); // Fallback to default
        }
    } else {
        actualPath = filepath;
    }
    
    ofLogNotice("ofxOceanodeScope") << "Starting save operation to: " << actualPath;
    
    ofJson json;
    
    // Save window configuration
    json["window"]["posX"] = ImGui::GetWindowPos().x;
    json["window"]["posY"] = ImGui::GetWindowPos().y;
    json["window"]["width"] = ImGui::GetWindowSize().x;
    json["window"]["height"] = ImGui::GetWindowSize().y;
    
    ofLogNotice("ofxOceanodeScope") << "Window config - Position: ("
        << json["window"]["posX"] << ", " << json["window"]["posY"]
        << "), Size: (" << json["window"]["width"] << "x" << json["window"]["height"] << ")";
    
    // Save scoped parameters
    json["parameters"] = ofJson::array();
    ofLogNotice("ofxOceanodeScope") << "Saving " << scopedParameters.size() << " parameter(s)";
    
    for(const auto& item : scopedParameters){
        ofJson paramJson;
        paramJson["path"] = item.parameter->getGroupHierarchyNames().front() + "/" + item.parameter->getName();
        paramJson["sizeRelative"] = item.sizeRelative;
        json["parameters"].push_back(paramJson);
        
        ofLogNotice("ofxOceanodeScope") << "  Parameter: " << paramJson["path"]
            << " (sizeRelative: " << item.sizeRelative << ")";
    }
    
    // Save to file (silently - no error messages for auto-save)
    ofSavePrettyJson(actualPath, json);
    
    ofLogNotice("ofxOceanodeScope") << "Save operation completed successfully";
}

void ofxOceanodeScope::loadScope(const std::string& filepath, ofxOceanodeContainer* container){
    ofLogNotice("ofxOceanodeScope") << "loadScope called with container: "
        << (container != nullptr ? "PROVIDED" : "NULL");
    
    // Use preset path if no filepath provided
    std::string actualPath;
    if(filepath.empty()){
        std::string presetPath = ofxOceanodeShared::getCurrentPresetPath();
        if(!presetPath.empty()){
            actualPath = presetPath + "/scope_config.json";
        } else {
            actualPath = ofToDataPath(saveFilePath); // Fallback to default
        }
    } else {
        actualPath = filepath;
    }
    
    // Check if file exists
    ofFile file(actualPath);
    if(!file.exists()){
        ofLogError("ofxOceanodeScope") << "Cannot load scope: file does not exist: " << actualPath;
        return;
    }
    
    // Load JSON
    ofJson json;
    try {
        json = ofLoadJson(actualPath);
    } catch (const std::exception& e) {
        ofLogError("ofxOceanodeScope") << "Error loading scope file: " << e.what();
        return;
    }
    
    // Load window configuration
    if(json.contains("window")){
        windowConfig.hasConfig = true;
        windowConfig.posX = json["window"]["posX"];
        windowConfig.posY = json["window"]["posY"];
        windowConfig.width = json["window"]["width"];
        windowConfig.height = json["window"]["height"];
    }
    
    // Clear existing scoped parameters
    for(auto& item : scopedParameters){
        item.parameter->setScoped(false);
    }
    scopedParameters.clear();
    
    // Clear and load parameter data
    loadedParameterData.clear();
    
    if(json.contains("parameters") && json["parameters"].is_array()){
        for(const auto& paramJson : json["parameters"]){
            ofxOceanodeScopeParameterData data;
            data.parameterPath = paramJson["path"];
            data.sizeRelative = paramJson["sizeRelative"];
            loadedParameterData.push_back(data);
            
            ofLogNotice("ofxOceanodeScope") << "Loaded scope parameter config: " << data.parameterPath
                                            << " (size: " << data.sizeRelative << ")";
        }
    }
    
    ofLogNotice("ofxOceanodeScope") << "Loaded scope configuration from: " << actualPath
                                    << " (" << loadedParameterData.size() << " parameters)";
    
    ofLogNotice("ofxOceanodeScope") << "Checking container recreation: container is "
        << (container != nullptr ? "NOT NULL" : "NULL")
        << ", loadedParameterData has " << loadedParameterData.size() << " items";
    
    // If container is provided, automatically recreate scoped parameters
    if(container != nullptr && !loadedParameterData.empty()){
        ofLogNotice("ofxOceanodeScope") << "Attempting to recreate " << loadedParameterData.size() << " scoped parameters from container";
        
        int successCount = 0;
        int failureCount = 0;
        
        // Get all nodes from container
        vector<ofxOceanodeNode*> allNodes = container->getAllModules();
        
        ofLogNotice("ofxOceanodeScope") << "=== Starting parameter recreation ===";
        ofLogNotice("ofxOceanodeScope") << "Total nodes in container: " << allNodes.size();
        
        // For each loaded parameter data
        for(const auto& paramData : loadedParameterData){
            // Parse the parameter path (format: "group/paramName")
            size_t slashPos = paramData.parameterPath.find('/');
            if(slashPos == std::string::npos){
                ofLogWarning("ofxOceanodeScope") << "Invalid parameter path format: " << paramData.parameterPath;
                failureCount++;
                continue;
            }
            
            std::string groupName = paramData.parameterPath.substr(0, slashPos);
            std::string paramName = paramData.parameterPath.substr(slashPos + 1);
            
            ofLogNotice("ofxOceanodeScope") << "\n--- Searching for parameter: " << paramData.parameterPath;
            ofLogNotice("ofxOceanodeScope") << "    Looking for groupName: '" << groupName << "' and paramName: '" << paramName << "'";
            
            // Search for the node with matching group hierarchy
            bool found = false;
            for(auto* node : allNodes){
                ofParameterGroup& params = node->getParameters();
                
                ofLogNotice("ofxOceanodeScope") << "  Checking node with parameter group name: '" << params.getName() << "'";
                
                // The key insight: we need to check if ANY parameter in this node has
                // getGroupHierarchyNames().front() == groupName
                // So let's iterate through all parameters and check their hierarchy
                for(int i = 0; i < params.size(); i++){
                    ofAbstractParameter& absParam = params.get(i);
                    
                    // Try to cast to oceanode parameter to access getGroupHierarchyNames()
                    auto* oceanodeParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&absParam);
                    if(oceanodeParam != nullptr){
                        // Get the group hierarchy for this parameter
                        vector<string> hierarchyNames = oceanodeParam->getGroupHierarchyNames();
                        
                        if(!hierarchyNames.empty()){
                            std::string paramHierarchyName = hierarchyNames.front();
                            
                            // Debug: log the first parameter we check in each node
                            if(i == 0){
                                ofLogNotice("ofxOceanodeScope") << "    First param hierarchy name: '" << paramHierarchyName
                                    << "' (param: '" << absParam.getName() << "')";
                            }
                            
                            // Check if this parameter's hierarchy matches what we're looking for
                            if(paramHierarchyName == groupName){
                                ofLogNotice("ofxOceanodeScope") << "    ✓ Found matching hierarchy name: '" << paramHierarchyName << "'";
                                
                                // Now search for the specific parameter by name in this node
                                for(int j = 0; j < params.size(); j++){
                                    ofAbstractParameter& targetParam = params.get(j);
                                    ofLogNotice("ofxOceanodeScope") << "      Checking param: '" << targetParam.getName() << "'";
                                    
                                    if(targetParam.getName() == paramName){
                                        ofLogNotice("ofxOceanodeScope") << "      ✓ Found matching parameter name: '" << paramName << "'";
                                        
                                        // Found the parameter! Add it to scope
                                        auto* targetOceanodeParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&targetParam);
                                        if(targetOceanodeParam != nullptr){
                                            // Add parameter with node color
                                            addParameter(targetOceanodeParam, node->getColor());
                                            
                                            // Set the sizeRelative from loaded data
                                            // Find the just-added parameter in scopedParameters
                                            if(!scopedParameters.empty()){
                                                scopedParameters.back().sizeRelative = paramData.sizeRelative;
                                            }
                                            
                                            ofLogNotice("ofxOceanodeScope") << "      ✓✓✓ Successfully recreated scoped parameter: "
                                                << paramData.parameterPath << " (sizeRelative: " << paramData.sizeRelative << ")";
                                            successCount++;
                                            found = true;
                                            break;
                                        } else {
                                            ofLogWarning("ofxOceanodeScope") << "      ✗ Parameter is not an ofxOceanodeAbstractParameter";
                                        }
                                    }
                                }
                                
                                // If we found the matching node, break out of parameter iteration
                                if(found) break;
                            }
                        }
                    }
                }
                
                if(found) break;
            }
            
            if(!found){
                ofLogWarning("ofxOceanodeScope") << "Could not find parameter in container: " << paramData.parameterPath;
                failureCount++;
            }
        }
        
        ofLogNotice("ofxOceanodeScope") << "Parameter recreation complete: " << successCount
            << " succeeded, " << failureCount << " failed";
    } else {
        if(container == nullptr){
            ofLogNotice("ofxOceanodeScope") << "Skipping parameter recreation: container is NULL";
        } else if(loadedParameterData.empty()){
            ofLogNotice("ofxOceanodeScope") << "Skipping parameter recreation: no loaded parameter data";
        }
    }
}
