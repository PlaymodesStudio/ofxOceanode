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

#include <algorithm>

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
    for(auto &p : floatParameters) node->addFloatParameter(p);
    for(auto &p : intParameters) node->addIntParameter(p);
    for(auto &p : boolParameters) node->addBoolParameter(p);
    for(auto &p : stringParameters) node->addStringParameter(p);
    for(auto &p : colorParameters) node->addOfColorParameter(p);
    for(auto &p : fcolorParameters) node->addOfFloatColorParameter(p);
}

void globalVariablesGroup::removeNode(globalVariables *node){
    nodes.erase(std::remove(nodes.begin(), nodes.end(), node), nodes.end());
}

void globalVariablesGroup::addFloatParameter(std::string parameterName, float value){
    auto createdParam = floatParameters.emplace_back(new ofParameter<float>(parameterName, value, -FLT_MAX, FLT_MAX));
    parameters.push_back(createdParam);
    for(auto &node : nodes){
        node->addFloatParameter(createdParam);
    }
}

void globalVariablesGroup::addIntParameter(std::string parameterName, int value){
    auto createdParam = intParameters.emplace_back(new ofParameter<int>(parameterName, value, INT_MIN, INT_MAX));
    parameters.push_back(createdParam);
    for(auto &node : nodes){
        node->addIntParameter(createdParam);
    }
}

void globalVariablesGroup::addBoolParameter(std::string parameterName, bool value){
    auto createdParam = boolParameters.emplace_back(new ofParameter<bool>(parameterName, value));
    parameters.push_back(createdParam);
    for(auto &node : nodes){
        node->addBoolParameter(createdParam);
    }
}

void globalVariablesGroup::addStringParameter(std::string parameterName, std::string value){
    auto createdParam = stringParameters.emplace_back(new ofParameter<std::string>(parameterName, value));
    parameters.push_back(createdParam);
    for(auto &node : nodes){
        node->addStringParameter(createdParam);
    }
}

void globalVariablesGroup::addOfColorParameter(std::string parameterName, ofColor value){
    auto createdParam = colorParameters.emplace_back(new ofParameter<ofColor>(parameterName, value));
    parameters.push_back(createdParam);
    for(auto &node : nodes){
        node->addOfColorParameter(createdParam);
    }
}

void globalVariablesGroup::addOfFloatColorParameter(std::string parameterName, ofFloatColor value){
    auto createdParam = fcolorParameters.emplace_back(new ofParameter<ofFloatColor>(parameterName, value));
    parameters.push_back(createdParam);
    for(auto &node : nodes){
        node->addOfFloatColorParameter(createdParam);
    }
}


void globalVariablesGroup::removeParameter(std::string parameterName){
    for(auto &node : nodes){
        node->removeParameter(parameterName);
    }
    parameters.erase(std::remove_if(parameters.begin(), parameters.end(), [parameterName](auto &parameter){return parameter->getName() == parameterName;}), parameters.end());
    floatParameters.erase(std::remove_if(floatParameters.begin(), floatParameters.end(), [parameterName](auto &parameter){return parameter->getName() == parameterName;}), floatParameters.end());
    intParameters.erase(std::remove_if(intParameters.begin(), intParameters.end(), [parameterName](auto &parameter){return parameter->getName() == parameterName;}), intParameters.end());
    boolParameters.erase(std::remove_if(boolParameters.begin(), boolParameters.end(), [parameterName](auto &parameter){return parameter->getName() == parameterName;}), boolParameters.end());
    stringParameters.erase(std::remove_if(stringParameters.begin(), stringParameters.end(), [parameterName](auto &parameter){return parameter->getName() == parameterName;}), stringParameters.end());
    colorParameters.erase(std::remove_if(colorParameters.begin(), colorParameters.end(), [parameterName](auto &parameter){return parameter->getName() == parameterName;}), colorParameters.end());
    fcolorParameters.erase(std::remove_if(fcolorParameters.begin(), fcolorParameters.end(), [parameterName](auto &parameter){return parameter->getName() == parameterName;}), fcolorParameters.end());
}

ofxOceanodeGlobalVariablesController::ofxOceanodeGlobalVariablesController(shared_ptr<ofxOceanodeContainer> _container) : container(_container), ofxOceanodeBaseController("Global Variables"){
    load();
}

void ofxOceanodeGlobalVariablesController::draw(){
    if(ImGui::MenuItem("Save Global Variables")) save();
    if(ImGui::MenuItem("Load Global Variables")) load();
    if(ImGui::MenuItem("New Group...")) newGroupRequested = true;
    ImGui::Separator();
    ImGui::TextUnformatted("Groups");

    string groupToDelete;
    for(const auto& group : groups){
        ImGui::PushID(group.get());
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        const bool open = ImGui::TreeNode("##group", "%s", group->name.c_str());
        ImGui::SameLine();
        if(ImGui::SmallButton("[-]##group")) groupToDelete = group->name;

        if(open){
            string parameterToDelete;
            for(const auto& parameter : group->parameters){
                ofAbstractParameter& absParam = *parameter;
                ImGui::PushID(parameter.get());
                ImGui::TextColored(ImVec4(150.0f / 255.0f, 150.0f / 255.0f, 150.0f / 255.0f, 1.0f),
                                   "%s", absParam.getName().c_str());
                ImGui::SameLine(175.0f);
                ImGui::SetNextItemWidth(135.0f);

                if(absParam.isOfType<float>()){
                    auto value = absParam.cast<float>();
                    float edited = value.get();
                    if(ImGui::DragFloat("##value", &edited, 0.001f, value.getMin(), value.getMax())) value = edited;
                }else if(absParam.isOfType<int>()){
                    auto value = absParam.cast<int>();
                    int edited = value.get();
                    if(ImGui::DragInt("##value", &edited, 1.0f, value.getMin(), value.getMax())) value = edited;
                }else if(absParam.isOfType<bool>()){
                    auto value = absParam.cast<bool>();
                    bool edited = value.get();
                    if(ImGui::Checkbox("##value", &edited)) value = edited;
                }else if(absParam.isOfType<string>()){
                    auto value = absParam.cast<string>();
                    const string current = value.get();
                    vector<char> edited(current.size() + 256, '\0');
                    std::copy(current.begin(), current.end(), edited.begin());
                    if(ImGui::InputText("##value", edited.data(), edited.size())) value = string(edited.data());
                }else if(absParam.isOfType<ofColor>()){
                    auto value = absParam.cast<ofColor>();
                    ofFloatColor edited(value.get());
                    if(ImGui::ColorEdit3("##value", &edited.r)) value = ofColor(edited);
                }else if(absParam.isOfType<ofFloatColor>()){
                    auto value = absParam.cast<ofFloatColor>();
                    ofFloatColor edited = value.get();
                    if(ImGui::ColorEdit4("##value", &edited.r, ImGuiColorEditFlags_Float)) value = edited;
                }

                ImGui::SameLine();
                if(ImGui::SmallButton("[-]##parameter")) parameterToDelete = absParam.getName();
                ImGui::PopID();
            }
            if(!parameterToDelete.empty()) group->removeParameter(parameterToDelete);
            if(ImGui::Button("New Variable...")){
                newVariableGroupName = group->name;
                newVariableRequested = true;
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if(!groupToDelete.empty()){
        groups.erase(std::remove_if(groups.begin(), groups.end(), [&](const auto& group){
            return group->name == groupToDelete;
        }), groups.end());
    }
}

void ofxOceanodeGlobalVariablesController::drawPopups(){
    if(newGroupRequested){
        newGroupName[0] = '\0';
        ImGui::OpenPopup("New Global Variables Group");
        newGroupRequested = false;
    }
    if(newVariableRequested){
        newVariableName[0] = '\0';
        newVariableType = 0;
        ImGui::OpenPopup("New Variable");
        newVariableRequested = false;
    }

    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if(ImGui::BeginPopupModal("New Global Variables Group", nullptr, ImGuiWindowFlags_AlwaysAutoResize)){
        if(ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 22.0f);
        const bool enter = ImGui::InputText("Name", newGroupName, sizeof(newGroupName), ImGuiInputTextFlags_EnterReturnsTrue);
        const string proposedName(newGroupName);
        const bool duplicate = std::any_of(groups.begin(), groups.end(), [&](const auto& group){
            return group->name == proposedName;
        });
        const bool canCreate = !proposedName.empty() && !duplicate;
        if(duplicate) ImGui::TextDisabled("A group with this name already exists.");
        ImGui::BeginDisabled(!canCreate);
        const bool create = ImGui::Button("Create");
        ImGui::EndDisabled();
        if(canCreate && (enter || create)){
            auto group = std::make_shared<globalVariablesGroup>(proposedName, container);
            groups.push_back(group);
            group->registerModule();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if(ImGui::BeginPopupModal("New Variable", nullptr, ImGuiWindowFlags_AlwaysAutoResize)){
        auto groupIt = std::find_if(groups.begin(), groups.end(), [&](const auto& group){
            return group->name == newVariableGroupName;
        });
        if(groupIt == groups.end()){
            ImGui::TextUnformatted("This group is no longer available.");
            if(ImGui::Button("Close")) ImGui::CloseCurrentPopup();
        }else{
            const auto& group = *groupIt;
            ImGui::Text("New variable for %s", group->name.c_str());
            static const char* types[] = {"Float", "Int", "Bool", "String", "ofColor", "ofFloatColor"};
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 16.0f);
            ImGui::Combo("Type", &newVariableType, types, 6);
            if(ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 22.0f);
            const bool enter = ImGui::InputText("Name", newVariableName, sizeof(newVariableName), ImGuiInputTextFlags_EnterReturnsTrue);
            const string proposedName(newVariableName);
            const bool duplicate = std::any_of(group->parameters.begin(), group->parameters.end(), [&](const auto& parameter){
                return parameter->getName() == proposedName;
            });
            const bool canCreate = !proposedName.empty() && !duplicate;
            if(duplicate) ImGui::TextDisabled("A variable with this name already exists.");
            ImGui::BeginDisabled(!canCreate);
            const bool create = ImGui::Button("Create");
            ImGui::EndDisabled();
            if(canCreate && (enter || create)){
                switch(newVariableType){
                    case 0: group->addFloatParameter(proposedName); break;
                    case 1: group->addIntParameter(proposedName); break;
                    case 2: group->addBoolParameter(proposedName); break;
                    case 3: group->addStringParameter(proposedName); break;
                    case 4: group->addOfColorParameter(proposedName); break;
                    case 5: group->addOfFloatColorParameter(proposedName); break;
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if(ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
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
                            (*foundParameter)->cast<int>().fromString(jsonParameter.value("Value", "0"));
                        }
                        else if(parameterType == typeid(bool).name()){
                            (*foundParameter)->cast<bool>().fromString(jsonParameter.value("Value", "0"));
                        }
                        else if(parameterType == typeid(string).name()){
                            (*foundParameter)->cast<string>().fromString(jsonParameter.value("Value", ""));
                        }
                        else if(parameterType == typeid(ofColor).name()){
                            (*foundParameter)->cast<ofColor>().fromString(jsonParameter.value("Value", "0, 0, 0, 0"));
                        }
                        else if(parameterType == typeid(ofFloatColor).name()){
                            (*foundParameter)->cast<ofFloatColor>().fromString(jsonParameter.value("Value", "0, 0, 0, 0"));
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
