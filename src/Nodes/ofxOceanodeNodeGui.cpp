//
//  ofxOceanodeNodeGui.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 22/02/2018.
//

#ifndef OFXOCEANODE_HEADLESS

#include "ofxOceanodeNodeGui.h"
#include "ofxOceanodeNode.h"
#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeContainer.h"
#include "ofxImGuiSimple.h"
#include "ofxOceanodeParameter.h"
#include "ofxOceanodeScope.h"

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
    string moduleName = getParameters().getName();
    ImGui::BeginGroup(); // Lock horizontal position
    
    bool deleteModule = false;
    
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(0, 0, 0,0)));
    
    if(ImGui::ArrowButton("expand", expanded ? ImGuiDir_Down : ImGuiDir_Right)){
        expanded = !expanded;
    }
    ImGui::PopStyleColor();
    
    ImGui::SameLine();
    ImGui::Text("%s", moduleName.c_str());
    
    ImGui::SameLine(guiRect.width - 30);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(220,220,220,255)));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(0, 0, 0,0)));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(0, 0, 0,0)));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor(0, 0, 0,0)));

    if (ImGui::Button("x"))
    {
        ImGui::OpenPopup("Delete?");
    }
    ImGui::PopStyleColor(4);
    
    if (ImGui::BeginPopupModal("Delete?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("%s", (moduleName + "\n").c_str());
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
        
        if(inputPositions.size() != getParameters().size()){
            inputPositions.resize(getParameters().size());
            outputPositions.resize(getParameters().size());
        }
        
        for(int i=0 ; i<getParameters().size(); i++){
            ofxOceanodeAbstractParameter &absParam = static_cast<ofxOceanodeAbstractParameter&>(getParameters().get(i));
            string uniqueId = absParam.getName();
            ImGui::PushID(uniqueId.c_str());
            if(absParam.valueType() == typeid(std::function<void()>).name()){
                absParam.cast<std::function<void()>>().getParameter().get()();
            }else{
                
                ImGui::Text("%s", uniqueId.c_str());
                
                ImGui::SetItemAllowOverlap();
                ImGui::SameLine(-1);
                ImGui::InvisibleButton(("##InvBut_" + uniqueId).c_str(), ImVec2(51, ImGui::GetFrameHeight())); //Used to check later behaviours
                
                int drag = 0;
                bool resetValue = false;
                if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)){
                    resetValue = true;
                    valueHasBeenReseted = true;
                }
                else if(ImGui::IsItemActive() && ImGui::IsMouseDragging(0, 0.1f) && !valueHasBeenReseted){
                    drag = ImGui::GetIO().MouseDelta.x;
                }
                else if(ImGui::IsItemClicked(1)){
                    ImGui::OpenPopup("Param Popup");
                }
                else if(valueHasBeenReseted && ImGui::IsMouseReleased(0)){
                    valueHasBeenReseted = false;
                }
                
                
                ImGui::SameLine(60);
                ImGui::SetNextItemWidth(150);
                
                string hiddenUniqueId = "##" + uniqueId;
                ImGui::PushStyleColor(ImGuiCol_SliderGrab,ImVec4(node.getColor()*0.5f));
                ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,ImVec4(node.getColor()*0.5f));
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram,ImVec4(node.getColor()*0.5f));
                ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(node.getColor()*.25f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(node.getColor()*.50f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(node.getColor()*.75f));
                
                bool isItemEditableByText = false; //Used for set focus via mouse or key
                
                // PARAM FLOAT
                ///////////////
                if(absParam.valueType() == typeid(float).name())
                {
                    auto tempCast = absParam.cast<float>().getParameter();
                    
                    if(drag != 0){
                        if(ImGui::GetIO().KeyShift) absParam.cast<float>().applyPrecisionDrag(drag);
                        else if(ImGui::GetIO().KeyAlt) absParam.cast<float>().applySpeedDrag(drag);
                        else absParam.cast<float>().applyNormalDrag(drag);
                    }
                    
                    if(resetValue){
                        tempCast = absParam.cast<float>().getDefaultValue();
                    }
                    
                    auto temp = tempCast.get();
                    ImGui::SliderFloat(hiddenUniqueId.c_str(), &temp, tempCast.getMin(), tempCast.getMax(), "%.4f");
                    
                    //TODO: Implement better this hack
                    // Maybe discard and reset value when not presed enter??
                    if(ImGui::IsItemDeactivated() || (ImGui::IsMouseDown(0) && ImGui::IsItemEdited()) ){
                        tempCast = ofClamp(temp, tempCast.getMin(), tempCast.getMax());
                    }
                    if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                        tempCast = tempCast;
                    }
                    isItemEditableByText = true;
                }
                // PARAM VECTOR < FLOAT >
                /////////////////////////
                else if(absParam.valueType() == typeid(vector<float>).name())
                {
                    auto tempCast = absParam.cast<vector<float>>().getParameter();
                    if(tempCast->size() == 1)
                    {
                        if(drag != 0){
                            if(ImGui::GetIO().KeyShift) absParam.cast<vector<float>>().applyPrecisionDrag(drag);
                            else if(ImGui::GetIO().KeyAlt) absParam.cast<vector<float>>().applySpeedDrag(drag);
                            else absParam.cast<vector<float>>().applyNormalDrag(drag);
                        }
                        
                        if(resetValue){
                            tempCast = absParam.cast<vector<float>>().getDefaultValue();
                        }
                        
                        auto temp = tempCast.get()[0];
                        ImGui::SliderFloat(hiddenUniqueId.c_str(),
                                           &temp,
                                           tempCast.getMin()[0],
                                           tempCast.getMax()[0], "%.4f");
                        
                        if(ImGui::IsItemDeactivated() || (ImGui::IsMouseDown(0) && ImGui::IsItemEdited())){
                            tempCast = vector<float>(1, ofClamp(temp, tempCast.getMin()[0], tempCast.getMax()[0]));
                        }
                        isItemEditableByText = true;
                    }else{
                        ImGui::PlotHistogram(hiddenUniqueId.c_str(), tempCast->data(), tempCast->size(), 0, NULL, tempCast.getMin()[0], tempCast.getMax()[0]);
                    }
                    if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                        tempCast = tempCast;
                    }
                }
                // PARAM INT
                /////////////
                else if(absParam.valueType() == typeid(int).name())
                {
                    auto tempCast = absParam.cast<int>().getParameter();
                    if(absParam.cast<int>().getDropdownOptions().size() == 0)
                    {
                        if(drag != 0){
                            if(ImGui::GetIO().KeyShift) absParam.cast<int>().applyPrecisionDrag(drag);
                            else if(ImGui::GetIO().KeyAlt) absParam.cast<int>().applySpeedDrag(drag);
                            else absParam.cast<int>().applyNormalDrag(drag);
                        }
                        
                        if(resetValue){
                            tempCast = absParam.cast<int>().getDefaultValue();
                        }
                        
                        auto temp = tempCast.get();
                        ImGui::SliderInt(hiddenUniqueId.c_str(), &temp, tempCast.getMin(),tempCast.getMax());
                        
                        if(ImGui::IsItemDeactivated() || (ImGui::IsMouseDown(0) && ImGui::IsItemEdited()))
                            tempCast = ofClamp(temp, tempCast.getMin(), tempCast.getMax());
                        if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space)))
                            tempCast = tempCast;
                        
                        isItemEditableByText = true;
                    }else{
                        auto vector_getter = [](void* vec, int idx, const char** out_text)
                        {
                            auto& vector = *static_cast<std::vector<std::string>*>(vec);
                            if (idx < 0 || idx >= static_cast<int>(vector.size())) { return false; }
                            *out_text = vector.at(idx).c_str();
                            return true;
                        };
                        
                        vector<string> options = absParam.cast<int>().getDropdownOptions();
                        if(ImGui::Combo(hiddenUniqueId.c_str(), (int*)&tempCast.get(), vector_getter, static_cast<void*>(&options), options.size()))
                            tempCast = ofClamp(tempCast, tempCast.getMin(), tempCast.getMax());
                        
                        if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space)))
                            tempCast = tempCast;
                        
                    }
                }
                // PARAM VECTOR < INT >
                /////////////////////////
                else if(absParam.valueType() == typeid(vector<int>).name())
                {
                    auto tempCast = absParam.cast<vector<int>>().getParameter();
                    if(tempCast->size() == 1)
                    {
                        if(drag != 0){
                            if(ImGui::GetIO().KeyShift) absParam.cast<vector<int>>().applyPrecisionDrag(drag);
                            else if(ImGui::GetIO().KeyAlt) absParam.cast<vector<int>>().applySpeedDrag(drag);
                            else absParam.cast<vector<int>>().applyNormalDrag(drag);
                        }
                        
                        if(resetValue){
                            tempCast = absParam.cast<vector<int>>().getDefaultValue();
                        }
                        
                        auto temp = tempCast.get()[0];
                        ImGui::SliderInt(hiddenUniqueId.c_str(),&temp,tempCast.getMin()[0],tempCast.getMax()[0]);
                        
                        if(ImGui::IsItemDeactivated() || (ImGui::IsMouseDown(0) && ImGui::IsItemEdited())){
                            tempCast = vector<int>(1, ofClamp(temp, tempCast.getMin()[0], tempCast.getMax()[0]));
                        }
                        isItemEditableByText = true;
                    }
                    else{
                        std::vector<float> floatVec(tempCast.get().begin(), tempCast.get().end());
                        ImGui::PlotHistogram(hiddenUniqueId.c_str(), floatVec.data(), tempCast->size(), 0, NULL, tempCast.getMin()[0], tempCast.getMax()[0]);
                    }
                    if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space)))
                        tempCast = tempCast;
                }
                // PARAM BOOL
                /////////////
                else if(absParam.valueType() == typeid(bool).name()){
                    auto tempCast = absParam.cast<bool>().getParameter();
                    
                    if(drag != 0){
                        absParam.cast<bool>().applyNormalDrag(drag);
                    }
                    
                    if (ImGui::Checkbox(hiddenUniqueId.c_str(), (bool *)&tempCast.get()))
                    {
                        tempCast = tempCast;
                    }
                    if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                        tempCast = !tempCast;
                    }
                    // PARAM VOID
                    /////////////
                }else if(absParam.valueType() == typeid(void).name()){
                    auto tempCast = absParam.cast<void>().getParameter();
                    if (ImGui::Button(hiddenUniqueId.c_str(), ImVec2(ImGui::GetFrameHeight(), 0)))
                    {
                        tempCast.trigger();
                    }
                    if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                        tempCast.trigger();
                    }
                    // PARAM STRING
                    ///////////////
                }else if(absParam.valueType() == typeid(string).name()){
                    auto tempCast = absParam.cast<string>().getParameter();
                    char * cString = new char[256];
                    strcpy(cString, tempCast.get().c_str());
                    auto result = false;
                    if (ImGui::InputText(hiddenUniqueId.c_str(), cString, 256, ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        tempCast = cString;
                    }
                    delete[] cString;
                    isItemEditableByText = true;
                    // PARAM CHAR
                    /////////////
                }else if(absParam.valueType() == typeid(char).name()){
                    ImGui::Dummy(ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
                    // PARAM COLOR
                    //////////////
                }else if(absParam.valueType() == typeid(ofColor).name()){
                    auto tempCast = absParam.cast<ofColor>().getParameter();
                    if(drag != 0){
                        if(ImGui::GetIO().KeyShift) absParam.cast<ofColor>().applyPrecisionDrag(drag);
                        else if(ImGui::GetIO().KeyAlt) absParam.cast<ofColor>().applySpeedDrag(drag);
                        else absParam.cast<ofColor>().applyNormalDrag(drag);
                    }
                    
                    ofFloatColor floatColor(tempCast.get());
                    
                    if (ImGui::ColorEdit3(hiddenUniqueId.c_str(), &floatColor.r))
                    {
                        tempCast = ofColor(floatColor);
                    }
                    if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                        tempCast = tempCast;
                    }
                    //PARAM FLOAT COLOR
                    ///////////////////
                }else if(absParam.valueType() == typeid(ofFloatColor).name()){
                    auto tempCast = absParam.cast<ofFloatColor>().getParameter();
                    
                    if(drag != 0){
                        if(ImGui::GetIO().KeyShift) absParam.cast<ofFloatColor>().applyPrecisionDrag(drag);
                        else if(ImGui::GetIO().KeyAlt) absParam.cast<ofFloatColor>().applySpeedDrag(drag);
                        else absParam.cast<ofFloatColor>().applyNormalDrag(drag);
                    }
                    
                    if (ImGui::ColorEdit4(hiddenUniqueId.c_str(), (float*)&tempCast.get().r, ImGuiColorEditFlags_Float))
                    {
                        tempCast = tempCast;
                    }
                    if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                        tempCast = tempCast;
                    }
                }
                // UNKNOWN PARAM
                ////////////////
                else
                {
                    ImGui::Dummy(ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
                }
                
                if (isItemEditableByText){
                    if ((ImGui::IsItemHovered() && !ImGui::IsItemEdited() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter))) || ImGui::IsItemClicked(1)){
                        ImGui::SetKeyboardFocusHere(-1);
                    }
                }
                
                if(ImGui::BeginPopup("Param Popup")){
                    ImGui::Separator();
                    if(!absParam.isScoped()){ //Param is not scoped
                        if(ImGui::Selectable("Add to Scope")){
                            ofxOceanodeScope::getInstance()->addParameter(&absParam,getColor());
                        }
                    }else{
                        if(ImGui::Selectable("Remove from Scope")){
                            ofxOceanodeScope::getInstance()->removeParameter(&absParam);
                        }
                    }
                    ImGui::Separator();
                    if(true){ //Param is not timelined
                        if(ImGui::Selectable("Add to Timeline")){
                            
                        }
                    }else{
                        if(ImGui::Selectable("Remove from Timeline")){
                            
                        }
                    }
#ifdef OFXOCEANODE_USE_MIDI
                    ImGui::Separator();
                    if(ImGui::Selectable("Bind MIDI")){
                        container.createMidiBinding(absParam);
                    }
                    if(ImGui::Selectable("Unbind last MIDI")){
                        container.removeLastMidiBinding(absParam);
                    }
#endif
#ifdef OFXOCEANODE_USE_OSC
                    ImGui::Separator();
                    ImGui::Text("OSC Address: %s/%s", getParameters().getEscapedName().c_str(), absParam.getEscapedName().c_str());
#endif
                    ImGui::Separator();
                    ImGui::EndPopup();
                }
                ImGui::PopStyleColor(6);
            }
            inputPositions[i] = glm::vec2(0, ImGui::GetItemRectMin().y + ImGui::GetItemRectSize().y/2);
            outputPositions[i] = glm::vec2(0, ImGui::GetItemRectMin().y + ImGui::GetItemRectSize().y/2);
            
            ImGui::PopID();
        } //endFor
    }else{}
    
    ImGui::EndGroup();
    if(expanded){
        for(auto &inPos : inputPositions){
            inPos.x = ImGui::GetItemRectMin().x;
        }
        for(auto &outPos : outputPositions){
            outPos.x = ImGui::GetItemRectMax().x;
        }
    }else{
        auto numParams = getParameters().size();
        for(int i=0 ; i < numParams; i++){
            ofAbstractParameter &absParam = getParameters().get(i);
            string uniqueId = absParam.getName();
            float yPos = numParams == 1 ? ImGui::GetItemRectSize().y / 2 : ImGui::GetItemRectSize().y * ((float)i/(numParams-1));
            inputPositions[i] = glm::vec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMin().y + yPos);
            outputPositions[i] = glm::vec2(ImGui::GetItemRectMax().x, ImGui::GetItemRectMin().y + yPos);
        }
    }
    if(deleteModule){
        node.deleteSelf();
        return false;
    }
    return true;
}

ofParameterGroup &ofxOceanodeNodeGui::getParameters(){
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

glm::vec2 ofxOceanodeNodeGui::getSourceConnectionPositionFromParameter(ofxOceanodeAbstractParameter& parameter){
    return outputPositions[getParameters().getPosition(parameter.getName())];
}

glm::vec2 ofxOceanodeNodeGui::getSinkConnectionPositionFromParameter(ofxOceanodeAbstractParameter& parameter){
    return inputPositions[getParameters().getPosition(parameter.getName())];
}

#endif
