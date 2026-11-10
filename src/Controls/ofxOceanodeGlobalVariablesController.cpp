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
#include "ofxOceanodeMenu.h"

#include <algorithm>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstring>
#include <set>

namespace {
const char* const variableTypeNames[globalVariableType_Count] = {"Float", "Int", "Bool", "String", "ofColor", "ofFloatColor"};

int typeFromValueType(const std::string& valueType){
    if(valueType == typeid(float).name()) return globalVariableType_Float;
    if(valueType == typeid(int).name()) return globalVariableType_Int;
    if(valueType == typeid(bool).name()) return globalVariableType_Bool;
    if(valueType == typeid(std::string).name()) return globalVariableType_String;
    if(valueType == typeid(ofColor).name()) return globalVariableType_Color;
    if(valueType == typeid(ofFloatColor).name()) return globalVariableType_FloatColor;
    return -1;
}

// Node parameters are looked up by escaped name ("My Var" is "My_Var").
std::string escapedName(const std::string& name){
    ofParameter<float> probe;
    probe.setName(name);
    return probe.getEscapedName();
}

void copyName(char* buffer, size_t size, const std::string& name){
    const size_t length = std::min(name.size(), size - 1);
    std::memcpy(buffer, name.data(), length);
    buffer[length] = '\0';
}
}

const char* globalVariableTypeName(int type){
    return type >= 0 && type < globalVariableType_Count ? variableTypeNames[type] : "?";
}

int globalVariableTypeOf(const ofAbstractParameter& parameter){
    return typeFromValueType(parameter.valueType());
}

std::shared_ptr<ofAbstractParameter> createGlobalVariable(int type, const std::string& name, const ofAbstractParameter* source){
    const int sourceType = source != nullptr ? globalVariableTypeOf(*source) : -1;
    bool hasNumber = true;
    double number = 0;
    switch(sourceType){
        case globalVariableType_Float: number = source->cast<float>().get(); break;
        case globalVariableType_Int: number = source->cast<int>().get(); break;
        case globalVariableType_Bool: number = source->cast<bool>().get() ? 1 : 0; break;
        case globalVariableType_String: number = ofToDouble(source->cast<std::string>().get()); break;
        default: hasNumber = false; break;
    }
    if(!std::isfinite(number)) number = 0;

    switch(type){
        case globalVariableType_Float:
            return std::make_shared<ofParameter<float>>(name, hasNumber ? float(std::clamp(number, double(-FLT_MAX), double(FLT_MAX))) : 0.0f, -FLT_MAX, FLT_MAX);
        case globalVariableType_Int:
            return std::make_shared<ofParameter<int>>(name, hasNumber ? int(std::lround(std::clamp(number, double(INT_MIN), double(INT_MAX)))) : 0, INT_MIN, INT_MAX);
        case globalVariableType_Bool:{
            bool value = false;
            if(sourceType == globalVariableType_String) value = ofToBool(source->cast<std::string>().get()) || number != 0;
            else if(hasNumber) value = number != 0;
            return std::make_shared<ofParameter<bool>>(name, value);
        }
        case globalVariableType_String:
            return std::make_shared<ofParameter<std::string>>(name, source != nullptr && sourceType != -1 ? source->toString() : "");
        case globalVariableType_Color:{
            ofColor value = ofColor::black;
            if(sourceType == globalVariableType_Color) value = source->cast<ofColor>().get();
            else if(sourceType == globalVariableType_FloatColor) value = ofColor(source->cast<ofFloatColor>().get());
            else if(sourceType == globalVariableType_String) value = ofFromString<ofColor>(source->cast<std::string>().get());
            return std::make_shared<ofParameter<ofColor>>(name, value);
        }
        case globalVariableType_FloatColor:{
            ofFloatColor value = ofFloatColor::black;
            if(sourceType == globalVariableType_FloatColor) value = source->cast<ofFloatColor>().get();
            else if(sourceType == globalVariableType_Color) value = ofFloatColor(source->cast<ofColor>().get());
            else if(sourceType == globalVariableType_String) value = ofFromString<ofFloatColor>(source->cast<std::string>().get());
            return std::make_shared<ofParameter<ofFloatColor>>(name, value);
        }
    }
    return nullptr;
}

void globalVariables::replaceVariable(const ofAbstractParameter* oldGroupParameter, const std::shared_ptr<ofAbstractParameter>& newGroupParameter){
    auto it = findVariable(oldGroupParameter);
    if(it == variables.end()) return;
    const std::string parameterName = it->localParameter->getName();

    struct Sink { ofxOceanodeAbstractParameter* parameter; bool persistent; };
    std::vector<Sink> sinks;
    if(getParameterGroup().contains(parameterName)){
        auto& oldParameter = static_cast<ofxOceanodeAbstractParameter&>(getParameterGroup().get(parameterName));
        for(auto* connection : oldParameter.getOutConnections()){
            sinks.push_back({&connection->getSinkParameter(), connection->getIsPersistent()});
        }
    }
    std::vector<std::string> order;
    for(int i = 0; i < getParameterGroup().size(); i++) order.push_back(getParameterGroup().get(i).getName());

    removeLocal(*it);
    it->groupParameter = newGroupParameter;
    if(!createLocal(*it)){
        variables.erase(it);
        parameterGroupChanged.notify();
        return;
    }
    getParameterGroup().reorder(order);

    // Connections the new type can't feed are dropped.
    auto* host = getHostContainer();
    if(host != nullptr && getParameterGroup().contains(parameterName)){
        auto& newParameter = static_cast<ofxOceanodeAbstractParameter&>(getParameterGroup().get(parameterName));
        for(const auto& sink : sinks){
            auto* connection = host->createConnection(newParameter, *sink.parameter);
            if(connection != nullptr) connection->setIsPersistent(sink.persistent);
        }
    }
    parameterGroupChanged.notify();
}

void globalVariables::sortVariables(const std::vector<std::shared_ptr<ofAbstractParameter>>& groupParameters){
    auto row = [&](const Variable& variable){
        return std::find(groupParameters.begin(), groupParameters.end(), variable.groupParameter) - groupParameters.begin();
    };
    std::stable_sort(variables.begin(), variables.end(), [&](const Variable& a, const Variable& b){ return row(a) < row(b); });

    // Refill the slots the variable pins hold, so any other pin stays put.
    std::set<std::string> variablePins;
    for(const auto& variable : variables) variablePins.insert(variable.localParameter->getEscapedName());
    std::vector<std::string> order;
    size_t next = 0;
    auto& nodeParameters = getParameterGroup();
    for(int i = 0; i < nodeParameters.size(); i++){
        auto& parameter = nodeParameters.get(i);
        if(variablePins.count(parameter.getEscapedName()) != 0 && next < variables.size()){
            order.push_back(variables[next++].localParameter->getName());
        }else{
            order.push_back(parameter.getName());
        }
    }
    nodeParameters.reorder(order);
    parameterGroupChanged.notify();
}

globalVariablesGroup::globalVariablesGroup(){

}

globalVariablesGroup::globalVariablesGroup(string _name, shared_ptr<ofxOceanodeContainer> _container) : name(_name), container(_container){
}

globalVariablesGroup::~globalVariablesGroup(){
    for(auto &node : nodes){
        node->deleteSelf();
    }
    unregisterModule();
}

void globalVariablesGroup::registerModule(){
    container->getRegistry()->registerModel<globalVariables>("Global Variables", name, std::weak_ptr<globalVariablesGroup>(shared_from_this()));
}

void globalVariablesGroup::unregisterModule(){
    container->getRegistry()->unregisterModel<globalVariables>("Global Variables", name, std::weak_ptr<globalVariablesGroup>());
}

void globalVariablesGroup::addNode(globalVariables *node){
    nodes.push_back(node);
    for(auto &p : parameters) node->addVariable(p);
}

void globalVariablesGroup::removeNode(globalVariables *node){
    nodes.erase(std::remove(nodes.begin(), nodes.end(), node), nodes.end());
}

void globalVariablesGroup::addParameter(std::shared_ptr<ofAbstractParameter> parameter){
    if(parameter == nullptr) return;
    parameters.push_back(parameter);
    for(auto &node : nodes){
        node->addVariable(parameter);
    }
}

void globalVariablesGroup::addFloatParameter(std::string parameterName, float value){
    addParameter(std::make_shared<ofParameter<float>>(parameterName, value, -FLT_MAX, FLT_MAX));
}

void globalVariablesGroup::addIntParameter(std::string parameterName, int value){
    addParameter(std::make_shared<ofParameter<int>>(parameterName, value, INT_MIN, INT_MAX));
}

void globalVariablesGroup::addBoolParameter(std::string parameterName, bool value){
    addParameter(std::make_shared<ofParameter<bool>>(parameterName, value));
}

void globalVariablesGroup::addStringParameter(std::string parameterName, std::string value){
    addParameter(std::make_shared<ofParameter<std::string>>(parameterName, value));
}

void globalVariablesGroup::addOfColorParameter(std::string parameterName, ofColor value){
    addParameter(std::make_shared<ofParameter<ofColor>>(parameterName, value));
}

void globalVariablesGroup::addOfFloatColorParameter(std::string parameterName, ofFloatColor value){
    addParameter(std::make_shared<ofParameter<ofFloatColor>>(parameterName, value));
}

void globalVariablesGroup::removeParameter(std::string parameterName){
    auto it = std::find_if(parameters.begin(), parameters.end(), [&](const auto &parameter){return parameter->getName() == parameterName;});
    if(it == parameters.end()) return;
    auto parameter = *it;
    for(auto &node : nodes){
        node->removeVariable(parameter.get());
    }
    parameters.erase(std::remove(parameters.begin(), parameters.end(), parameter), parameters.end());
}

bool globalVariablesGroup::isParameterNameAvailable(const std::string& parameterName, const ofAbstractParameter* ignore) const{
    if(parameterName.empty()) return false;
    const std::string escaped = escapedName(parameterName);
    return std::none_of(parameters.begin(), parameters.end(), [&](const auto &parameter){
        return parameter.get() != ignore && parameter->getEscapedName() == escaped;
    });
}

bool globalVariablesGroup::renameParameter(const std::string& oldName, const std::string& newName){
    auto it = std::find_if(parameters.begin(), parameters.end(), [&](const auto &parameter){return parameter->getName() == oldName;});
    if(it == parameters.end()) return false;
    if(newName == oldName) return true;
    // ofParameter refuses a new name that escapes to the current one.
    if(escapedName(newName) == escapedName(oldName)) return false;
    if(!isParameterNameAvailable(newName, it->get())) return false;
    auto parameter = *it;
    if(!parameter->setName(newName)) return false;
    for(auto &node : nodes){
        node->renameVariable(parameter.get(), newName);
    }
    return true;
}

bool globalVariablesGroup::setParameterType(const std::string& parameterName, int type){
    auto it = std::find_if(parameters.begin(), parameters.end(), [&](const auto &parameter){return parameter->getName() == parameterName;});
    if(it == parameters.end()) return false;
    if(globalVariableTypeOf(**it) == type) return true;
    auto oldParameter = *it;
    auto newParameter = createGlobalVariable(type, parameterName, oldParameter.get());
    if(newParameter == nullptr) return false;
    *it = newParameter;
    for(auto &node : nodes){
        node->replaceVariable(oldParameter.get(), newParameter);
    }
    return true;
}

bool globalVariablesGroup::moveParameter(const std::string& parameterName, int newIndex){
    auto it = std::find_if(parameters.begin(), parameters.end(), [&](const auto &parameter){return parameter->getName() == parameterName;});
    if(it == parameters.end()) return false;
    const int from = int(it - parameters.begin());
    const int to = std::clamp(newIndex, 0, int(parameters.size()) - 1);
    if(from == to) return false;
    auto parameter = *it;
    parameters.erase(it);
    parameters.insert(parameters.begin() + to, parameter);
    for(auto &node : nodes){
        node->sortVariables(parameters);
    }
    return true;
}

bool globalVariablesGroup::rename(const std::string& newName){
    if(newName.empty() || newName == name) return false;
    auto registry = container->getRegistry();
    const std::string newTypeName = "Globals " + newName;
    if(registry->getRegisteredModels().count(newTypeName) != 0) return false;

    struct Link { std::string parameter; ofxOceanodeAbstractParameter* sink; bool persistent; };
    struct Instance { ofxOceanodeContainer* owner; ofxOceanodeNode* node; unsigned int identifier; std::vector<Link> links; };
    std::vector<Instance> instances;
    // Globals nodes have only outputs, so every link ends on another node,
    // which survives the swap. Instances inside macros live in their own container.
    for(auto* model : nodes){
        auto* owner = model->getHostContainer();
        if(owner == nullptr) continue;
        const auto& nodesMap = owner->getParameterGroupNodesMap();
        auto found = nodesMap.find(model->getParameterGroup().getEscapedName());
        if(found == nodesMap.end()) continue;
        Instance instance{owner, found->second, model->getNumIdentifier(), {}};
        auto& nodeParameters = model->getParameterGroup();
        for(int i = 0; i < nodeParameters.size(); i++){
            auto& parameter = static_cast<ofxOceanodeAbstractParameter&>(nodeParameters.get(i));
            for(auto* connection : parameter.getOutConnections()){
                instance.links.push_back({parameter.getName(), &connection->getSinkParameter(), connection->getIsPersistent()});
            }
        }
        instances.push_back(std::move(instance));
    }

    unregisterModule();
    name = newName;
    registerModule();

    bool success = true;
    for(auto& instance : instances){
        auto* oldNode = instance.node;
        auto& oldGui = oldNode->getNodeGui();
        const glm::vec2 position = oldGui.getPosition();
        const bool expanded = oldGui.getExpanded();
        const bool visible = oldGui.getVisibility();
        const bool selected = oldGui.getSelected();
        const bool active = oldNode->getActive();
        const bool persistent = oldNode->getIsPersistent();

        // Keep the instance number, so "Globals Old 2" becomes "Globals New 2".
        const auto& existing = instance.owner->getDynamicNodes();
        auto sameType = existing.find(newTypeName);
        const bool identifierTaken = persistent || (sameType != existing.end() && sameType->second.count(instance.identifier) != 0);
        auto* newNode = instance.owner->createNodeFromName(newTypeName, identifierTaken ? -1 : int(instance.identifier), persistent);
        if(newNode == nullptr){
            // The old node keeps working; it still follows this group.
            ofLogWarning("Global Variables") << "Could not replace a node with " << newTypeName;
            success = false;
            continue;
        }
        newNode->getNodeGui().setPosition(position);
        newNode->getNodeGui().setExpanded(expanded);
        newNode->getNodeGui().setVisibility(visible);
        newNode->getNodeGui().setSelected(selected);
        newNode->setActive(active);

        oldNode->deleteSelf();
        for(const auto& link : instance.links){
            if(!newNode->getParameters().contains(link.parameter)){
                success = false;
                continue;
            }
            auto& source = static_cast<ofxOceanodeAbstractParameter&>(newNode->getParameters().get(link.parameter));
            auto* connection = instance.owner->createConnection(source, *link.sink);
            if(connection != nullptr) connection->setIsPersistent(link.persistent);
            else success = false;
        }
    }
    return success;
}

ofxOceanodeGlobalVariablesController::ofxOceanodeGlobalVariablesController(shared_ptr<ofxOceanodeContainer> _container) : container(_container), ofxOceanodeBaseController("Global Variables"){
    load();
}

std::shared_ptr<globalVariablesGroup> ofxOceanodeGlobalVariablesController::findGroup(const std::string& groupName) const{
    auto it = std::find_if(groups.begin(), groups.end(), [&](const auto& group){return group->name == groupName;});
    return it == groups.end() ? nullptr : *it;
}

bool ofxOceanodeGlobalVariablesController::isGroupNameAvailable(const std::string& groupName, const globalVariablesGroup* ignore) const{
    if(groupName.empty()) return false;
    // Node identities are escaped ("Globals_My_Group_1"), so compare those.
    const std::string escaped = escapedName(groupName);
    return std::none_of(groups.begin(), groups.end(), [&](const auto& group){
        return group.get() != ignore && escapedName(group->name) == escaped;
    });
}

void ofxOceanodeGlobalVariablesController::draw(){
    if(ImGui::MenuItem("Save Global Variables", unsavedChanges ? "unsaved" : nullptr)) save();
    if(ImGui::MenuItem("Load Global Variables")) load();
    ofxOceanodeMenu::section("Groups");
    ImGui::PushItemFlag(ImGuiItemFlags_AutoClosePopups, false);
    if(ImGui::MenuItem("New Group...")) newGroupRequested = true;
    ImGui::PopItemFlag();

    string groupToDelete;
    for(const auto& group : groups){
        ImGui::PushID(group.get());
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        const bool open = ImGui::TreeNode("##group", "%s", group->name.c_str());
        ImGui::SameLine();
        if(ImGui::SmallButton("Rename##group")){
            renameGroupName = group->name;
            renameGroupRequested = true;
        }
        ImGui::SameLine();
        if(ImGui::SmallButton("[-]##group")) groupToDelete = group->name;

        if(open){
            drawGroupTable(group);
            if(ImGui::Button("New Variable...")){
                newVariableGroupName = group->name;
                newVariableRequested = true;
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    for(const auto& edit : pendingEdits){
        auto group = findGroup(edit.group);
        if(group == nullptr) continue;
        switch(edit.kind){
            case PendingEdit::Rename:{
                const std::string newName = ofTrim(edit.newName);
                if(newName != edit.parameter && group->renameParameter(edit.parameter, newName)) unsavedChanges = true;
                break;
            }
            case PendingEdit::Retype:
                if(group->setParameterType(edit.parameter, edit.type)) unsavedChanges = true;
                renamingParameter = nullptr;
                break;
            case PendingEdit::Move:
                if(group->moveParameter(edit.parameter, edit.index)) unsavedChanges = true;
                break;
            case PendingEdit::Remove:
                group->removeParameter(edit.parameter);
                renamingParameter = nullptr;
                unsavedChanges = true;
                break;
        }
    }
    pendingEdits.clear();

    if(!groupToDelete.empty()){
        groups.erase(std::remove_if(groups.begin(), groups.end(), [&](const auto& group){
            return group->name == groupToDelete;
        }), groups.end());
        unsavedChanges = true;
    }

    drawDialogs();
}

void ofxOceanodeGlobalVariablesController::drawGroupTable(const std::shared_ptr<globalVariablesGroup>& group){
    if(group->parameters.empty()){
        ImGui::TextDisabled("No variables");
        return;
    }
    const ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings;
    if(!ImGui::BeginTable("##variables", 5, flags)) return;
    const float fontSize = ImGui::GetFontSize();
    ImGui::TableSetupColumn("##order", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, fontSize * 10.0f);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, fontSize * 7.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, fontSize * 12.0f);
    ImGui::TableSetupColumn("##remove", ImGuiTableColumnFlags_WidthFixed);
    // Headers take the theme's title colour.
    ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, ImGui::GetStyleColorVec4(ImGuiCol_TitleBg));
    ImGui::TableHeadersRow();
    ImGui::PopStyleColor();

    const int rowCount = int(group->parameters.size());
    for(int row = 0; row < rowCount; row++){
        const auto& parameter = group->parameters[row];
        ofAbstractParameter& absParam = *parameter;
        const std::string parameterName = absParam.getName();
        ImGui::PushID(parameter.get());
        ImGui::TableNextRow();

        // Order.
        ImGui::TableNextColumn();
        PendingEdit move{PendingEdit::Move, group->name, parameterName};
        ImGui::BeginDisabled(row == 0);
        if(ImGui::ArrowButton("##up", ImGuiDir_Up)){ move.index = row - 1; pendingEdits.push_back(move); }
        ImGui::EndDisabled();
        ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::BeginDisabled(row + 1 == rowCount);
        if(ImGui::ArrowButton("##down", ImGuiDir_Down)){ move.index = row + 1; pendingEdits.push_back(move); }
        ImGui::EndDisabled();

        // Name: edited in place, applied when the field is left.
        ImGui::TableNextColumn();
        const bool editing = renamingParameter == parameter.get();
        char rowName[sizeof(variableNameBuffer)];
        copyName(rowName, sizeof(rowName), parameterName);
        char* nameBuffer = editing ? variableNameBuffer : rowName;
        const std::string proposedName = ofTrim(std::string(nameBuffer));
        const bool invalidName = editing && proposedName != parameterName &&
            (!group->isParameterNameAvailable(proposedName, parameter.get()) || escapedName(proposedName) == escapedName(parameterName));
        if(invalidName) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.45f, 0.4f, 1.0f));
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##name", nameBuffer, sizeof(variableNameBuffer), ImGuiInputTextFlags_AutoSelectAll);
        if(invalidName) ImGui::PopStyleColor();
        if(ImGui::IsItemActivated()){
            renamingParameter = parameter.get();
            copyName(variableNameBuffer, sizeof(variableNameBuffer), parameterName);
        }
        if(invalidName && ImGui::IsItemHovered()){
            ImGui::SetTooltip("Another variable in this group already uses this name.\nSpaces and symbols count as underscores.");
        }
        if(ImGui::IsItemDeactivated()){
            if(ImGui::IsItemDeactivatedAfterEdit()){
                pendingEdits.push_back({PendingEdit::Rename, group->name, parameterName, std::string(variableNameBuffer)});
            }
            renamingParameter = nullptr;
        }else if(editing && !ImGui::IsItemActive() && !ImGui::IsItemActivated()){
            // The menu closed mid-edit without submitting the field.
            renamingParameter = nullptr;
        }

        // Type: changing it keeps the row, converts the value and keeps the
        // connections the new type accepts.
        ImGui::TableNextColumn();
        const int type = globalVariableTypeOf(absParam);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if(ImGui::BeginCombo("##type", globalVariableTypeName(type))){
            for(int t = 0; t < globalVariableType_Count; t++){
                if(ImGui::Selectable(globalVariableTypeName(t), t == type) && t != type){
                    PendingEdit edit{PendingEdit::Retype, group->name, parameterName};
                    edit.type = t;
                    pendingEdits.push_back(edit);
                }
            }
            ImGui::EndCombo();
        }

        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool edited = false;
        switch(type){
            case globalVariableType_Float:{
                auto value = absParam.cast<float>();
                float v = value.get();
                if(ImGui::DragFloat("##value", &v, 0.001f, value.getMin(), value.getMax())){ value = v; edited = true; }
                break;
            }
            case globalVariableType_Int:{
                auto value = absParam.cast<int>();
                int v = value.get();
                if(ImGui::DragInt("##value", &v, 1.0f, value.getMin(), value.getMax())){ value = v; edited = true; }
                break;
            }
            case globalVariableType_Bool:{
                auto value = absParam.cast<bool>();
                bool v = value.get();
                if(ImGui::Checkbox("##value", &v)){ value = v; edited = true; }
                break;
            }
            case globalVariableType_String:{
                auto value = absParam.cast<string>();
                const string current = value.get();
                vector<char> v(current.size() + 256, '\0');
                std::copy(current.begin(), current.end(), v.begin());
                if(ImGui::InputText("##value", v.data(), v.size())){ value = string(v.data()); edited = true; }
                break;
            }
            case globalVariableType_Color:{
                auto value = absParam.cast<ofColor>();
                ofFloatColor v(value.get());
                if(ImGui::ColorEdit4("##value", &v.r)){ value = ofColor(v); edited = true; }
                break;
            }
            case globalVariableType_FloatColor:{
                auto value = absParam.cast<ofFloatColor>();
                ofFloatColor v = value.get();
                if(ImGui::ColorEdit4("##value", &v.r, ImGuiColorEditFlags_Float)){ value = v; edited = true; }
                break;
            }
        }
        if(edited) unsavedChanges = true;

        ImGui::TableNextColumn();
        if(ImGui::Button("[-]")) pendingEdits.push_back({PendingEdit::Remove, group->name, parameterName});
        ImGui::PopID();
    }
    ImGui::EndTable();
}

void ofxOceanodeGlobalVariablesController::drawDialogs(){
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
    if(renameGroupRequested){
        copyName(groupNameBuffer, sizeof(groupNameBuffer), renameGroupName);
        ImGui::OpenPopup("Rename Global Variables Group");
        renameGroupRequested = false;
    }

    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if(ImGui::BeginPopupModal("New Global Variables Group", nullptr, ImGuiWindowFlags_AlwaysAutoResize)){
        if(ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 22.0f);
        const bool enter = ImGui::InputText("Name", newGroupName, sizeof(newGroupName), ImGuiInputTextFlags_EnterReturnsTrue);
        const string proposedName = ofTrim(string(newGroupName));
        const bool duplicate = !proposedName.empty() && !isGroupNameAvailable(proposedName);
        const bool canCreate = !proposedName.empty() && !duplicate;
        if(duplicate) ImGui::TextDisabled("A group with this name already exists.");
        ImGui::BeginDisabled(!canCreate);
        const bool create = ImGui::Button("Create");
        ImGui::EndDisabled();
        if(canCreate && (enter || create)){
            auto group = std::make_shared<globalVariablesGroup>(proposedName, container);
            groups.push_back(group);
            group->registerModule();
            unsavedChanges = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if(ImGui::BeginPopupModal("Rename Global Variables Group", nullptr, ImGuiWindowFlags_AlwaysAutoResize)){
        auto group = findGroup(renameGroupName);
        if(group == nullptr){
            ImGui::TextUnformatted("This group is no longer available.");
            if(ImGui::Button("Close")) ImGui::CloseCurrentPopup();
        }else{
            if(ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 22.0f);
            const bool enter = ImGui::InputText("Name", groupNameBuffer, sizeof(groupNameBuffer),
                                                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
            const string proposedName = ofTrim(string(groupNameBuffer));
            const bool duplicate = !proposedName.empty() && !isGroupNameAvailable(proposedName, group.get());
            const bool canRename = !proposedName.empty() && !duplicate && proposedName != group->name;
            if(duplicate) ImGui::TextDisabled("A group with this name already exists.");
            if(!group->nodes.empty()){
                const int count = int(group->nodes.size());
                ImGui::Text("%d \"Globals %s\" node%s in the patch will be replaced by \"Globals %s\".",
                            count, group->name.c_str(), count == 1 ? "" : "s",
                            proposedName.empty() ? "..." : proposedName.c_str());
                ImGui::TextDisabled("Connections are kept. Canvas undo history is cleared, and saved\n"
                                    "presets that use the old name won't find these nodes.");
            }
            ImGui::BeginDisabled(!canRename);
            const bool rename = ImGui::Button("Rename");
            ImGui::EndDisabled();
            if(canRename && (enter || rename)){
                if(!group->rename(proposedName)){
                    ofLogWarning("Global Variables") << "Renamed group " << proposedName << ", but some nodes or connections could not be restored";
                }
                unsavedChanges = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if(ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if(ImGui::BeginPopupModal("New Variable", nullptr, ImGuiWindowFlags_AlwaysAutoResize)){
        auto group = findGroup(newVariableGroupName);
        if(group == nullptr){
            ImGui::TextUnformatted("This group is no longer available.");
            if(ImGui::Button("Close")) ImGui::CloseCurrentPopup();
        }else{
            ImGui::Text("New variable for %s", group->name.c_str());
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 16.0f);
            ImGui::Combo("Type", &newVariableType, variableTypeNames, globalVariableType_Count);
            if(ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 22.0f);
            const bool enter = ImGui::InputText("Name", newVariableName, sizeof(newVariableName), ImGuiInputTextFlags_EnterReturnsTrue);
            const string proposedName = ofTrim(string(newVariableName));
            const bool duplicate = !proposedName.empty() && !group->isParameterNameAvailable(proposedName);
            const bool canCreate = !proposedName.empty() && !duplicate;
            if(duplicate) ImGui::TextDisabled("A variable with this name already exists.");
            ImGui::BeginDisabled(!canCreate);
            const bool create = ImGui::Button("Create");
            ImGui::EndDisabled();
            if(canCreate && (enter || create)){
                group->addParameter(createGlobalVariable(newVariableType, proposedName));
                unsavedChanges = true;
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
        json[group->name] = ofJson::array();
        for(int i = 0; i < parameterGroup.size(); i++){
            auto &parameter = *parameterGroup[i];
            json[group->name][i]["Name"] = parameter.getName();
            json[group->name][i]["Type"] = parameter.valueType();
            json[group->name][i]["Value"] = parameter.toString();
        }
    }
    ofSavePrettyJson("globalVars.json", json);
    unsavedChanges = false;
}

void ofxOceanodeGlobalVariablesController::load(){
    ofJson json = ofLoadJson("globalVars.json");
    vector<string> groupsUpdated;
    if(!json.empty()){
        for(auto &jsonGroup : json.items()){
            shared_ptr<globalVariablesGroup> group = findGroup(jsonGroup.key());
            if(group == nullptr){
                group = groups.emplace_back(std::make_shared<globalVariablesGroup>(jsonGroup.key(), container));
                group->registerModule();
            }
            for(auto &jsonParameter : jsonGroup.value()){
                const int parameterType = typeFromValueType(jsonParameter.value("Type", "err"));
                const string parameterName = jsonParameter.value("Name", "");
                if(parameterType == -1 || parameterName.empty()) continue;

                auto foundParameter = find_if(group->parameters.begin(), group->parameters.end(), [parameterName](const auto &parameter){return parameter->getName() == parameterName;});
                std::shared_ptr<ofAbstractParameter> parameter;
                if(foundParameter != group->parameters.end()){
                    // A different type is converted in place, keeping its row.
                    group->setParameterType(parameterName, parameterType);
                    parameter = *find_if(group->parameters.begin(), group->parameters.end(), [parameterName](const auto &parameter){return parameter->getName() == parameterName;});
                }else{
                    parameter = createGlobalVariable(parameterType, parameterName);
                    group->addParameter(parameter);
                }
                if(jsonParameter.contains("Value")) parameter->fromString(jsonParameter.value("Value", ""));
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
    unsavedChanges = false;
}
