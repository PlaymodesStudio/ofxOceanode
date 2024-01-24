//
//  ofxOceanodeGlobalVariablesController.cpp
//  example
//
//  Created by Eduard Frigola on 18/12/23.
//

#include "ofxOceanodeGlobalVariablesController.h"
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeNodeRegistry.h"
#include "globalVariables.h"
#include "imgui.h"

globalVariablesGroup::globalVariablesGroup(){
    
}

globalVariablesGroup::globalVariablesGroup(string _name, shared_ptr<ofxOceanodeContainer> _container) : name(_name), container(_container){
}

globalVariablesGroup::~globalVariablesGroup(){
    for(auto &node : nodes){
        node->deleteSelf();
    }
    container->getRegistry()->unregisterModel<globalVariables>("Global Variables", name, std::weak_ptr<globalVariablesGroup>());
}

void globalVariablesGroup::registerModule(){
    container->getRegistry()->registerModel<globalVariables>("Global Variables", name, std::weak_ptr<globalVariablesGroup>(shared_from_this()));
}

void globalVariablesGroup::addNode(globalVariables *node){
    nodes.push_back(node);
    for(int i = 0; i < parameters.size(); i++){
        node->addParameter(*parameters[i], ofxOceanodeParameterFlags_ReadOnly);
    }
}

void globalVariablesGroup::removeNode(globalVariables *node){
    nodes.erase(std::remove(nodes.begin(), nodes.end(), node), nodes.end());
}

void globalVariablesGroup::addFloatParameter(std::string parameterName, float value){
    ofParameter<float> tempParam;
    parameters.push_back(tempParam.set(parameterName, value, -FLT_MAX, FLT_MAX).newReference());
    for(auto &node : nodes){
        node->addParameter(tempParam, ofxOceanodeParameterFlags_ReadOnly);
    }
}

void globalVariablesGroup::addIntParameter(std::string parameterName, int value){
    ofParameter<int> tempParam;
    parameters.push_back(tempParam.set(parameterName, value, INT_MIN, INT_MAX).newReference());
    for(auto &node : nodes){
        node->addParameter(tempParam, ofxOceanodeParameterFlags_ReadOnly);
    }
}

void globalVariablesGroup::addBoolParameter(std::string parameterName, bool value){
    ofParameter<bool> tempParam;
    parameters.push_back(tempParam.set(parameterName, value).newReference());
    for(auto &node : nodes){
        node->addParameter(tempParam, ofxOceanodeParameterFlags_ReadOnly);
    }
}

void globalVariablesGroup::addStringParameter(std::string parameterName, std::string value){
    ofParameter<std::string> tempParam;
    parameters.push_back(tempParam.set(parameterName, value).newReference());
    for(auto &node : nodes){
        node->addParameter(tempParam, ofxOceanodeParameterFlags_ReadOnly);
    }
}

void globalVariablesGroup::addOfColorParameter(std::string parameterName, ofColor value){
    ofParameter<ofColor> tempParam;
    parameters.push_back(tempParam.set(parameterName, value).newReference());
    for(auto &node : nodes){
        node->addParameter(tempParam, ofxOceanodeParameterFlags_ReadOnly);
    }
}

void globalVariablesGroup::addOfFloatColorParameter(std::string parameterName, ofFloatColor value){
    ofParameter<ofFloatColor> tempParam;
    parameters.push_back(tempParam.set(parameterName, value).newReference());
    for(auto &node : nodes){
        node->addParameter(tempParam, ofxOceanodeParameterFlags_ReadOnly);
    }
}


void globalVariablesGroup::removeParameter(std::string parameterName){
    for(auto &node : nodes){
        node->removeParameter(parameterName);
    }
    parameters.erase(std::remove_if(parameters.begin(), parameters.end(), [parameterName](auto &parameter){return parameter->getName() == parameterName;}), parameters.end());
}

ofxOceanodeGlobalVariablesController::ofxOceanodeGlobalVariablesController(shared_ptr<ofxOceanodeContainer> _container) : container(_container), ofxOceanodeBaseController("Global Variables"){
    load();
}

void ofxOceanodeGlobalVariablesController::draw(){
    string groupToDelete = "";
    for(auto &group : groups){
        auto &groupParams = group->parameters;
        ImGui::PushID(group->name.c_str());
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        bool openGroup = true;
        if(ImGui::CollapsingHeader(group->name.c_str(), &openGroup)){
//            ImGui::Separator();
            for(int i = 0; i < groupParams.size(); i++){
                ofAbstractParameter &absParam = *groupParams[i];
                std::string uniqueId = absParam.getName();
                ImGui::PushID(uniqueId.c_str());
                
                ImGui::Text("  %s", uniqueId.c_str());
                ImGui::SameLine(150);
                ImGui::SetNextItemWidth(60);
                
                std::string hiddenUniqueId = "##" + uniqueId;
                if(absParam.isOfType<float>()){
                    auto tempCast = absParam.cast<float>();
                    auto temp = tempCast.get();
                    if(ImGui::DragFloat(hiddenUniqueId.c_str(), &temp, 0.001, tempCast.getMin(), tempCast.getMax())){
                        tempCast = temp;
                    }
                }else if(absParam.isOfType<int>()){
                    auto tempCast = absParam.cast<int>();
                    auto temp = tempCast.get();

                    if(ImGui::DragInt(hiddenUniqueId.c_str(), &temp, 1, tempCast.getMin(), tempCast.getMax())){
                        tempCast = temp;
                    }
                }else if(absParam.isOfType<bool>()){
                    auto tempCast = absParam.cast<bool>();

                    if (ImGui::Checkbox(hiddenUniqueId.c_str(), (bool *)&tempCast.get())){
                        tempCast = tempCast;
                    }
                }else if(absParam.isOfType<string>()){
                    auto tempCast = absParam.cast<std::string>();
                    char * cString = new char[256];
                    strcpy(cString, tempCast.get().c_str());
                    auto result = false;
                    if (ImGui::InputText(hiddenUniqueId.c_str(), cString, 256, ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        tempCast = cString;
                    }
                    delete[] cString;
                }else if(absParam.isOfType<ofColor>()){
                    auto tempCast = absParam.cast<ofColor>();

                    ofFloatColor floatColor(tempCast.get());

                    if (ImGui::ColorEdit3(hiddenUniqueId.c_str(), &floatColor.r)){
                        tempCast = ofColor(floatColor);
                    }
                }else if(absParam.isOfType<ofFloatColor>()){
                    auto tempCast = absParam.cast<ofFloatColor>();
                    
                    if (ImGui::ColorEdit4(hiddenUniqueId.c_str(), (float*)&tempCast.get().r, ImGuiColorEditFlags_Float)){
                        tempCast = tempCast;
                    }
                }
                
                ImGui::SameLine(150 + 60);
                if(ImGui::Button("[-]")){
                    group->removeParameter(absParam.getName());
                }
                
                ImGui::PopID();
            }

            if(ImGui::Button("[+]")){
                ImGui::OpenPopup("New Variable");
            }
            
            // Always center this window when appearing
            ImVec2 center = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            if(ImGui::BeginPopupModal("New Variable", NULL, ImGuiWindowFlags_AlwaysAutoResize)){
                ImGui::Text("%s %s %s", "New variable for", group->name.c_str(), "group");
                ImGui::Separator();
                enum Types { Type_Float, Type_Int, Type_Bool, Type_String, Type_OfColor, Type_OfFloatColor, Type_COUNT };
                static int type = Type_Float;
                const char* types_names[Type_COUNT] = { "Float", "Int", "Bool", "String", "ofColor", "ofFloatColor" };
                const char* type_name = (type >= 0 && type < Type_COUNT) ? types_names[type] : "Unknown";
                ImGui::SliderInt("Type", &type, 0, Type_COUNT - 1, type_name); // Use ImGuiSliderFlags_NoInput flag to 
                static char cString[255];
                bool enterPressed = false;
                if(ImGui::InputText("Name", cString, 255, ImGuiInputTextFlags_EnterReturnsTrue)){
                    enterPressed = true;
                }
                
                if (ImGui::Button("OK", ImVec2(120, 0)) || enterPressed) {
                    string proposedNewName(cString);
                    if(proposedNewName != "" && find_if(groupParams.begin(), groupParams.end(), [proposedNewName](const auto &param){return param->getName() == proposedNewName;}) == groupParams.end()){
                        switch(type){
                            case Type_Float:
                                group->addFloatParameter(proposedNewName);
                                break;
                            case Type_Int:
                                group->addIntParameter(proposedNewName);
                                break;
                            case Type_Bool:
                                group->addBoolParameter(proposedNewName);
                                break;
                            case Type_String:
                                group->addStringParameter(proposedNewName);
                                break;
                            case Type_OfColor:
                                group->addOfColorParameter(proposedNewName);
                                break;
                            case Type_OfFloatColor:
                                group->addOfFloatColorParameter(proposedNewName);
                                break;
                            default:
                                ofLog() << "Cannot create variable of unknown type";
                        }
                        
                        ImGui::CloseCurrentPopup();
                    }
                    strcpy(cString, "");
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                    strcpy(cString, "");
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
    
        if(!openGroup){
            groupToDelete = group->name;
        }
        
        ImGui::PopID();
    }
    
    if(groupToDelete != ""){
        groups.erase(std::remove_if(groups.begin(), groups.end(), [groupToDelete](auto &group){return group->name == groupToDelete;}), groups.end());
    }
    
    ImGui::Separator();
    
    if(ImGui::Button("[New Group]")){
        ImGui::OpenPopup("New Global Variables Group");
    }
    ImGui::SameLine();
    if(ImGui::Button("[Save]")){
        save();
    }
    ImGui::SameLine();
    if(ImGui::Button("[Load]")){
        load();
    }
    
    bool unusedOpen = true;
    // Always center this window when appearing
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if(ImGui::BeginPopupModal("New Global Variables Group", &unusedOpen, ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::Text("%s", "Type the global variable group name");
        static char cString[255];

        bool enterPressed = false;
        if(ImGui::InputText("Name", cString, 255, ImGuiInputTextFlags_EnterReturnsTrue)){
            enterPressed = true;
        }
        
        if (ImGui::Button("OK", ImVec2(120, 0)) || enterPressed) {
            string proposedNewName(cString);
            if(proposedNewName != "" && find_if(groups.begin(), groups.end(), [proposedNewName](const auto &group){return group->name == proposedNewName;}) == groups.end()){
                groups.emplace_back(std::make_shared<globalVariablesGroup>(proposedNewName, container))->registerModule();
            }
            ImGui::CloseCurrentPopup();
            strcpy(cString, "");
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            strcpy(cString, "");
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void ofxOceanodeGlobalVariablesController::save(){
    ofJson json;
    for(auto &group : groups){
        auto &parameterGroup = group->parameters;
        for(int i = 0; i < parameterGroup.size(); i++){
            auto &parameter = *parameterGroup[i];
            json[group->name][i]["Name"] = parameter.getName();
            json[group->name][i]["Type"] = parameter.valueType();
            json[group->name][i]["Value"] = parameter.toString();
        }
    }
    ofSavePrettyJson("globalVars.json", json);
}

void ofxOceanodeGlobalVariablesController::load(){
    ofJson json = ofLoadJson("globalVars.json");
    vector<string> groupsUpdated;
    if(!json.empty()){
        for(auto &jsonGroup : json.items()){
            shared_ptr<globalVariablesGroup> group;
            
            auto foundGroup = find_if(groups.begin(), groups.end(), [jsonGroup](const auto &group){return group->name == jsonGroup.key();});
            if(foundGroup == groups.end()){
                group = groups.emplace_back(std::make_shared<globalVariablesGroup>(jsonGroup.key(), container));
                group->registerModule();
            }else{
                group = *foundGroup;
            }
            for(auto &jsonParameter : jsonGroup.value()){
                string parameterType = jsonParameter.value("Type", "err");
                string parameterName = jsonParameter.value("Name", "");
                
                auto foundParameter = find_if(group->parameters.begin(), group->parameters.end(), [parameterName](const auto &parameter){return parameter->getName() == parameterName;});
                
                bool create = false;
                if(foundParameter != group->parameters.end()){ // Parameter found
                    if((*foundParameter)->valueType() == parameterType){ //Same type, update
                        if(parameterType == typeid(float).name()){
                            (*foundParameter)->cast<float>().fromString(jsonParameter.value("Value", "0"));
                        }
                        else if(parameterType == typeid(int).name()){
                            (*foundParameter)->cast<float>().fromString(jsonParameter.value("Value", "0"));
                        }
                        else if(parameterType == typeid(bool).name()){
                            (*foundParameter)->cast<float>().fromString(jsonParameter.value("Value", "0"));
                        }
                        else if(parameterType == typeid(string).name()){
                            (*foundParameter)->cast<float>().fromString(jsonParameter.value("Value", ""));
                        }
                        else if(parameterType == typeid(ofColor).name()){
                            (*foundParameter)->cast<float>().fromString(jsonParameter.value("Value", "0, 0, 0, 0"));
                        }
                        else if(parameterType == typeid(ofFloatColor).name()){
                            (*foundParameter)->cast<float>().fromString(jsonParameter.value("Value", "0, 0, 0, 0"));
                        }
                    }else{ //Diferent type, remove and recreate
                        group->removeParameter(parameterName);
                        create = true;
                    }
                }
                else{
                    create = true;
                }
                
                if(create){
                    if(parameterType == typeid(float).name()){
                        group->addFloatParameter(parameterName, ofFromString<float>(jsonParameter.value("Value", "0")));
                    }
                    else if(parameterType == typeid(int).name()){
                        group->addIntParameter(parameterName, ofFromString<int>(jsonParameter.value("Value", "0")));
                    }
                    else if(parameterType == typeid(bool).name()){
                        group->addBoolParameter(parameterName, ofFromString<bool>(jsonParameter.value("Value", "0")));
                    }
                    else if(parameterType == typeid(string).name()){
                        group->addStringParameter(parameterName, ofFromString<string>(jsonParameter.value("Value", "")));
                    }
                    else if(parameterType == typeid(ofColor).name()){
                        group->addOfColorParameter(parameterName, ofFromString<ofColor>(jsonParameter.value("Value", "0, 0, 0, 0")));
                    }
                    else if(parameterType == typeid(ofFloatColor).name()){
                        group->addOfFloatColorParameter(parameterName, ofFromString<ofFloatColor>(jsonParameter.value("Value", "0, 0, 0, 0")));
                    }
                }
            }
            groupsUpdated.push_back(group->name);
        }
    }

    //Remove all groups that are not created nor updated
    groups.erase(std::remove_if(groups.begin(), groups.end(),
        [groupsUpdated](auto &group) {
            return std::find(groupsUpdated.begin(), groupsUpdated.end(), group->name) == groupsUpdated.end();
        }),
    groups.end());
}
