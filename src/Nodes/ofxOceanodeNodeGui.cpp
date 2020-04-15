//
//  ofxOceanodeNodeGui.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 22/02/2018.
//

#ifndef OFXOCEANODE_HEADLESS

#include "ofxOceanodeNodeGui.h"
#include "ofxOceanodeNode.h"
#include "ofxOceanodeContainer.h"
#include "ofxImGuiSimple.h"

ofxOceanodeNodeGui::ofxOceanodeNodeGui(ofxOceanodeContainer& _container, ofxOceanodeNode& _node) : container(_container), node(_node){
    color = node.getColor();
    //color.setBrightness(255);
    guiRect = ofRectangle(10, 10, 10, 10);
    guiToBeDestroyed = false;
    lastExpandedState = true;
    isGuiCreated = false;
    
    expanded = true;
#ifdef OFXOCEANODE_USE_MIDI
    isListeningMidi = false;
#endif
}

ofxOceanodeNodeGui::~ofxOceanodeNodeGui(){
    
}

bool ofxOceanodeNodeGui::constructGui(){
    string moduleName = getParameters()->getName();
    ImGui::BeginGroup(); // Lock horizontal position
    
    bool deleteModule = false;
    
    if(ImGui::ArrowButton("expand", expanded ? ImGuiDir_Down : ImGuiDir_Right)){
        expanded = !expanded;
    }
    ImGui::SameLine();
//    ImGui::SameLine(0, 10);
    ImGui::Text("%s", moduleName.c_str());
    
    ImGui::SameLine(guiRect.width - 30);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(220,220,220,255)));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(0, 0, 0,0)));

    if (ImGui::Button("x"))
    {
        ImGui::OpenPopup("Delete?");
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
    
    if (ImGui::BeginPopupModal("Delete?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("%s", ("Are you sure you want to delete.\n " + moduleName + "\n").c_str());
        ImGui::Separator();
        
        if (ImGui::Button("OK", ImVec2(120,0)) || ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Enter))) {
            ImGui::CloseCurrentPopup();
            deleteModule = true;
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120,0))) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    
    if(expanded){
    
        ImGui::Spacing();
        
        auto startPos = ImGui::GetCursorScreenPos();
        
        for(int i=0 ; i<getParameters()->size(); i++){
            ofAbstractParameter &absParam = getParameters()->get(i);
            string uniqueId = absParam.getName();
            ImGui::Text("%s", uniqueId.c_str());
            ImGui::SameLine(ImGui::GetItemRectMin().x - startPos.x + 50);
            
            string hiddenUniqueId = "##" + uniqueId;
            ImGui::PushStyleColor(ImGuiCol_SliderGrab,ImVec4(node.getColor()));
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,ImVec4(node.getColor()));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram,ImVec4(node.getColor()));
            
            ImGui::SetNextItemWidth(150);
            
            if(absParam.type() == typeid(ofParameter<float>).name()){
                auto tempCast = absParam.cast<float>();
                if(tempCast.getMin() == FLT_MIN || tempCast.getMax() == FLT_MAX){
                    ImGui::DragFloat(hiddenUniqueId.c_str(), (float *)&tempCast.get(), 1, tempCast.getMin(), tempCast.getMax());
                }else{
                    
                    ImGui::SliderFloat(hiddenUniqueId.c_str(), (float *)&tempCast.get(), tempCast.getMin(), tempCast.getMax());
                }
                //TODO: Implement better this hack
                // Maybe discard and reset value when not presed enter??
                if(ImGui::IsItemDeactivated() || (ImGui::IsMouseDown(0) && ImGui::IsItemEdited()) ){
                    tempCast = tempCast;
                }
                if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                    tempCast = tempCast;
                }
            }else if(absParam.type() == typeid(ofParameter<int>).name()){
                auto tempCast = absParam.cast<int>();
                if(tempCast.getMin() == INT_MIN || tempCast.getMax() == INT_MAX){
                    ImGui::DragInt(hiddenUniqueId.c_str(), (int *)&tempCast.get(), 1, tempCast.getMin(), tempCast.getMax());
                }else{
                    ImGui::SliderInt(hiddenUniqueId.c_str(), (int *)&tempCast.get(), tempCast.getMin(), tempCast.getMax());
                }
                if(ImGui::IsItemDeactivated() || (ImGui::IsMouseDown(0) && ImGui::IsItemEdited())){
                    tempCast = tempCast;
                }
                if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                    tempCast = tempCast;
                }
            }else if(absParam.type() == typeid(ofParameter<bool>).name()){
                auto tempCast = absParam.cast<bool>();
                if (ImGui::Checkbox(hiddenUniqueId.c_str(), (bool *)&tempCast.get()))
                {
                    tempCast = tempCast;
                }
                if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                    tempCast = !tempCast;
                }
            }else if(absParam.type() == typeid(ofParameter<void>).name()){
                if (ImGui::Button(hiddenUniqueId.c_str(), ImVec2(ImGui::GetFrameHeight(), 0)))
                {
                    absParam.cast<void>().trigger();
                }
                if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                    absParam.cast<void>().trigger();
                }
            }else if(absParam.type() == typeid(ofParameter<string>).name()){
                auto tempCast = absParam.cast<string>();
                char * cString = new char[256];
                strcpy(cString, tempCast.get().c_str());
                auto result = false;
                if (ImGui::InputText(hiddenUniqueId.c_str(), cString, 256, ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    tempCast = tempCast;
                }
                delete[] cString;
            }else if(absParam.type() == typeid(ofParameter<char>).name()){
                ImGui::Text("%s", absParam.getName().c_str());
            }else if(absParam.type() == typeid(ofParameter<ofColor>).name()){
                auto tempCast = absParam.cast<ofFloatColor>();
                if (ImGui::ColorEdit4(hiddenUniqueId.c_str(), (float*)&tempCast.get().r))
                {
                    tempCast = tempCast;
                }
                if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                    tempCast = tempCast;
                }
            }else if(absParam.type() == typeid(ofParameterGroup).name()){
                auto vector_getter = [](void* vec, int idx, const char** out_text)
                {
                    auto& vector = *static_cast<std::vector<std::string>*>(vec);
                    if (idx < 0 || idx >= static_cast<int>(vector.size())) { return false; }
                    *out_text = vector.at(idx).c_str();
                    return true;
                };
                
                auto tempCast = absParam.castGroup();
                vector<string> options = ofSplitString(tempCast.getString(0), "-|-");
                if(ImGui::Combo(hiddenUniqueId.c_str(), (int*)&tempCast.getInt(1).get(), vector_getter, static_cast<void*>(&options), options.size())){
                    tempCast.getInt(1) = tempCast.getInt(1);
                }
                if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                    tempCast = tempCast;
                }
            }else if(absParam.type() == typeid(ofParameter<vector<float>>).name()){
                auto tempCast = absParam.cast<vector<float>>();
                if(tempCast->size() == 1){
                    if(tempCast.getMin()[0] == FLT_MIN || tempCast.getMax()[0] == FLT_MAX){
                        ImGui::DragFloat(hiddenUniqueId.c_str(), (float *)&tempCast->at(0), 1, tempCast.getMin()[0], tempCast.getMax()[0]);
                    }else{
                        ImGui::SliderFloat(hiddenUniqueId.c_str(), (float *)&tempCast->at(0), tempCast.getMin()[0], tempCast.getMax()[0]);
                    }
                    if(ImGui::IsItemDeactivated() || (ImGui::IsMouseDown(0) && ImGui::IsItemEdited())){
                        tempCast = vector<float>(1, tempCast->at(0));
                    }
                }else{
                    ImGui::PlotHistogram(hiddenUniqueId.c_str(), tempCast->data(), tempCast->size(), 0, NULL, tempCast.getMin()[0], tempCast.getMax()[0]);
                }
                if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                    tempCast = tempCast;
                }
            }else if(absParam.type() == typeid(ofParameter<vector<int>>).name()){
                auto tempCast = absParam.cast<vector<int>>();
                if(tempCast->size() == 1){
                    if(tempCast.getMin()[0] == INT_MIN || tempCast.getMax()[0] == INT_MAX){
                        ImGui::DragInt(hiddenUniqueId.c_str(), (int *)&tempCast->at(0), 1, tempCast.getMin()[0], tempCast.getMax()[0]);
                    }else{
                        ImGui::SliderInt(hiddenUniqueId.c_str(), (int *)&tempCast->at(0), tempCast.getMin()[0], tempCast.getMax()[0]);
                    }
                    if(ImGui::IsItemDeactivated() || (ImGui::IsMouseDown(0) && ImGui::IsItemEdited())){
                        tempCast = vector<int>(1, tempCast->at(0));
                    }
                }else{
                    std::vector<float> floatVec(tempCast.get().begin(), tempCast.get().end());
                    ImGui::PlotHistogram(hiddenUniqueId.c_str(), floatVec.data(), tempCast->size(), 0, NULL, tempCast.getMin()[0], tempCast.getMax()[0]);
                }
                if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                    tempCast = tempCast;
                }
            }else {
                ImGui::Dummy(ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
            }
            inputPositions[uniqueId] = glm::vec2(0, ImGui::GetItemRectMin().y + ImGui::GetItemRectSize().y/2);
            outputPositions[uniqueId] = glm::vec2(0, ImGui::GetItemRectMin().y + ImGui::GetItemRectSize().y/2);
            
            if(ImGui::IsItemClicked(1)){
#ifdef OFXOCEANODE_USE_MIDI
                if(isListeningMidi){
                    if(ImGui::GetIO().KeyShift){
                        container.removeLastMidiBinding(absParam);
                    }else{
                        container.createMidiBinding(absParam);
                    }
                }
                else
#endif
                {
                    auto connection = node.parameterConnectionPress(container, absParam);
                }
            }else if(container.isOpenConnection() && ImGui::IsItemHovered() && !ImGui::IsMouseDown(1)){
                auto connection = node.parameterConnectionRelease(container, absParam);
            }
            
            ImGui::PopStyleColor(3);
        }
    }else{
        
    }
    ImGui::EndGroup();
    if(expanded){
        for(auto &inPos : inputPositions){
            inPos.second.x = ImGui::GetItemRectMin().x;
        }
        for(auto &outPos : outputPositions){
            outPos.second.x = ImGui::GetItemRectMax().x;
        }
    }else{
        auto numParams = getParameters()->size();
        for(int i=0 ; i < numParams; i++){
            ofAbstractParameter &absParam = getParameters()->get(i);
            string uniqueId = absParam.getName();
            float yPos = numParams == 1 ? ImGui::GetItemRectSize().y / 2 : ImGui::GetItemRectSize().y * ((float)i/(numParams-1));
            inputPositions[uniqueId] = glm::vec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMin().y + yPos);
            outputPositions[uniqueId] = glm::vec2(ImGui::GetItemRectMax().x, ImGui::GetItemRectMin().y + yPos);
        }
    }
    if(deleteModule){
        node.deleteSelf();
        return false;
    }
    return true;
}

shared_ptr<ofParameterGroup> ofxOceanodeNodeGui::getParameters(){
    return node.getParameters();
}

void ofxOceanodeNodeGui::setPosition(glm::vec2 position){
    guiRect.setPosition(glm::vec3(position, 1));
}

void ofxOceanodeNodeGui::setSize(glm::vec2 size){
    guiRect.setSize(size.x, size.y);
}

glm::vec2 ofxOceanodeNodeGui::getPosition(){
    return guiRect.getPosition();
}

ofRectangle ofxOceanodeNodeGui::getRectangle(){
    return guiRect;
}

void ofxOceanodeNodeGui::enable(){
//    gui->setVisible(true);
}

void ofxOceanodeNodeGui::disable(){
//    gui->setVisible(false);
}

void ofxOceanodeNodeGui::duplicate(){
    node.duplicateSelf(getPosition());
}

glm::vec2 ofxOceanodeNodeGui::getSourceConnectionPositionFromParameter(ofAbstractParameter& parameter){
    return outputPositions[parameter.getName()];
}

glm::vec2 ofxOceanodeNodeGui::getSinkConnectionPositionFromParameter(ofAbstractParameter& parameter){
    return inputPositions[parameter.getName()];
}

#endif
