//
//  ofxOceanodeInspectorController.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 08/01/2021.
//

#include "ofxOceanodeInspectorController.h"
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeScope.h"
#include "ofxOceanodeNode.h"
#include "imgui.h"

void ofxOceanodeInspectorController::draw(){
    vector<pair<string, ofxOceanodeNode*>> nodesInThisFrame = vector<pair<string, ofxOceanodeNode*>>(container->getParameterGroupNodesMap().begin(), container->getParameterGroupNodesMap().end());
    
    vector<pair<string, ofxOceanodeNode*>> selectedNodes;
    
    for(auto nodePair : nodesInThisFrame)
    {
        auto &nodeGui = nodePair.second->getNodeGui();
        if(nodeGui.getSelected()){
            selectedNodes.push_back(nodePair);
        }
    }
    
    if(selectedNodes.size() > 0){
        //Order nodes by name
        std::sort(selectedNodes.begin(), selectedNodes.end(), [this](std::pair<std::string, ofxOceanodeNode*> a, std::pair<std::string, ofxOceanodeNode*> b){
            string aName = a.first;
            string bName = b.first;
            string aNum = a.first;
            string bNum = b.first;
            aNum = aNum.substr(aNum.find_last_of("_") + 1);
            bNum = bNum.substr(bNum.find_last_of("_") + 1);
            aName.erase(aName.rfind(aNum)-1);
            bName.erase(bName.rfind(bNum)-1);
            if(aName == bName) return ofToInt(aNum) < ofToInt(bNum);
            else{
                return aName < bName;
            }
        });
        
        vector<pair<string, ofxOceanodeNode*>> selectedNodesWithoutFirst = vector<pair<string, ofxOceanodeNode*>>(selectedNodes.begin() + 1, selectedNodes.end());
        
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 0.0f);
        
        
        //Draw Selected Names
        ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
        if(ImGui::TreeNode("Selected Nodes")){
            for(auto nodePair : selectedNodes)
            {
                auto node = nodePair.second;
                auto &nodeGui = node->getNodeGui();
                ImGui::PushStyleColor(ImGuiCol_Text, node->getColor());
                string nodeId = nodePair.first;
                ImGui::Text(nodeId.c_str(), "%s");
                ImGui::PopStyleColor();
            }
            ImGui::TreePop();
        }
        
        ImGui::Separator();
        ImGui::Separator();
    
    //Draw common Inspector Parameters
        ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
        if(ImGui::TreeNode("Inspector Parameters")){
            auto &node = selectedNodes[0].second;
            for(int i=0 ; i < node->getInspectorParameters().size(); i++){
                ofAbstractParameter &absParam = node->getInspectorParameters().get(i);
                bool isShared = true;
                bool sameValue = true;
                for(auto nodePair : selectedNodesWithoutFirst){
                    if(!nodePair.second->getInspectorParameters().contains(absParam.getName())) isShared = false;
                    else{
                        ofAbstractParameter &_absParam = (nodePair.second->getInspectorParameters().get(absParam.getName()));
                        if(_absParam.valueType() != absParam.valueType()) isShared = false;
                        else if(absParam.valueType() == typeid(float).name() &&
                                absParam.valueType() == typeid(int).name()){
                            bool sameRange = false;
                            if(checkSameRange<float>(absParam, _absParam)) sameRange = true;
                            if(checkSameRange<int>(absParam, _absParam)) sameRange = true;
                            
                            if(!sameRange) isShared = false;
                        }
                        
                        bool sameValueWithThis = false;
                        if(checkSameValue<float>(absParam, _absParam)) sameValueWithThis = true;
                        if(checkSameValue<int>(absParam, _absParam)) sameValueWithThis = true;
                        if(checkSameValue<bool>(absParam, _absParam)) sameValueWithThis = true;
                        if(checkSameValue<string>(absParam, _absParam)) sameValueWithThis = true;
                        if(checkSameValue<ofColor>(absParam, _absParam)) sameValueWithThis = true;
                        if(checkSameValue<ofFloatColor>(absParam, _absParam)) sameValueWithThis = true;
                        
                        if(!sameValueWithThis) sameValue = false;
                    }
                }
                if (isShared) {
                    string uniqueId = absParam.getName();
                    ImGui::PushID(uniqueId.c_str());
                    if(absParam.valueType() == typeid(std::function<void()>).name()){
                        absParam.cast<std::function<void()>>().get()();
                    }else{
                        ImGui::Text("%s", uniqueId.c_str());
                        
//                        ImGui::SetItemAllowOverlap();
//                        ImGui::SameLine(-1);
//                        ImGui::InvisibleButton(("##InvBut_" + uniqueId).c_str(), ImVec2(51, ImGui::GetFrameHeight())); //Used to check later behaviours

                        ImGui::SameLine();
                        //ImGui::SetNextItemWidth(150);
                        
                        string hiddenUniqueId = "##" + uniqueId;
                        //TODO: CAMBIAR ESTIL
                        if(!sameValue) ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0.2, 0.2, 0.2, 1));
                        //            ImGui::PushStyleColor(ImGuiCol_SliderGrab,ImVec4(node.getColor()*0.5f));
                        //            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,ImVec4(node.getColor()*0.5f));
                        //            ImGui::PushStyleColor(ImGuiCol_PlotHistogram,ImVec4(node.getColor()*0.5f));
                        //            ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(node.getColor()*.25f));
                        //            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(node.getColor()*.50f));
                        //            ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(node.getColor()*.75f));
                        
                        bool isItemEditableByText = false; //Used for set focus via mouse or key
                        
                        // PARAM FLOAT
                        ///////////////
                        if(absParam.valueType() == typeid(float).name())
                        {
                            auto tempCast = absParam.cast<float>();
                            
                            auto temp = tempCast.get();
                            ImGui::SliderFloat(hiddenUniqueId.c_str(), &temp, tempCast.getMin(), tempCast.getMax(), "%.4f");
                            
                            //TODO: Implement better this hack
                            // Maybe discard and reset value when not presed enter??
                            if(ImGui::IsItemDeactivated() || (ImGui::IsMouseDown(0) && ImGui::IsItemEdited()) ){
                                tempCast = ofClamp(temp, tempCast.getMin(), tempCast.getMax());
                                for(auto nodePair : selectedNodesWithoutFirst){
                                    nodePair.second->getInspectorParameters().getFloat(absParam.getName()) = tempCast;
                                }
                            }
                            if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                                tempCast = tempCast;
                                for(auto nodePair : selectedNodesWithoutFirst){
                                    nodePair.second->getInspectorParameters().getFloat(absParam.getName()) = tempCast;
                                }
                            }
                            isItemEditableByText = true;
                        }
                        // PARAM INT
                        /////////////
                        else if(absParam.valueType() == typeid(int).name())
                        {
                            auto tempCast = absParam.cast<int>();
                            if(true)//absParam.cast<int>().getDropdownOptions().size() == 0)
                            {
                                auto temp = tempCast.get();
                                ImGui::SliderInt(hiddenUniqueId.c_str(), &temp, tempCast.getMin(),tempCast.getMax());
                                
                                if(ImGui::IsItemDeactivated() || (ImGui::IsMouseDown(0) && ImGui::IsItemEdited())){
                                    tempCast = ofClamp(temp, tempCast.getMin(), tempCast.getMax());
                                    for(auto nodePair : selectedNodesWithoutFirst){
                                        nodePair.second->getInspectorParameters().getInt(absParam.getName()) = tempCast;
                                    }
                                }
                                
                                if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                                    tempCast = tempCast;
                                    for(auto nodePair : selectedNodesWithoutFirst){
                                        nodePair.second->getInspectorParameters().getInt(absParam.getName()) = tempCast;
                                    }
                                }
                                
                                isItemEditableByText = true;
                            }
                        }
                        // PARAM BOOL
                        /////////////
                        else if(absParam.valueType() == typeid(bool).name()){
                            auto tempCast = absParam.cast<bool>();
                            
                            if (ImGui::Checkbox(hiddenUniqueId.c_str(), (bool *)&tempCast.get()))
                            {
                                tempCast = tempCast;
                                for(auto nodePair : selectedNodesWithoutFirst){
                                    nodePair.second->getInspectorParameters().getBool(absParam.getName()) = tempCast;
                                }
                            }
                            if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                                tempCast = !tempCast;
                                for(auto nodePair : selectedNodesWithoutFirst){
                                    nodePair.second->getInspectorParameters().getBool(absParam.getName()) = tempCast;
                                }
                            }
                        // PARAM VOID
                        /////////////
                        }else if(absParam.valueType() == typeid(void).name()){
                            auto tempCast = absParam.cast<void>();
                            if (ImGui::Button(hiddenUniqueId.c_str(), ImVec2(ImGui::GetFrameHeight(), 0)))
                            {
                                tempCast.trigger();
                                for(auto nodePair : selectedNodesWithoutFirst){
                                    nodePair.second->getInspectorParameters().getVoid(absParam.getName()) = tempCast;
                                }
                            }
                            if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                                tempCast.trigger();
                                for(auto nodePair : selectedNodesWithoutFirst){
                                    nodePair.second->getInspectorParameters().getVoid(absParam.getName()) = tempCast;
                                }
                            }
                        // PARAM STRING
                        //////////////
                        }else if(absParam.valueType() == typeid(string).name()){
                            auto tempCast = absParam.cast<string>();
                            char * cString = new char[256];
                            strcpy(cString, tempCast.get().c_str());
                            auto result = false;
                            if (ImGui::InputText(hiddenUniqueId.c_str(), cString, 256, ImGuiInputTextFlags_EnterReturnsTrue))
                            {
                                tempCast = cString;
                                for(auto nodePair : selectedNodesWithoutFirst){
                                    nodePair.second->getInspectorParameters().getString(absParam.getName()) = tempCast;
                                }
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
                            auto tempCast = absParam.cast<ofColor>();
                            
                            ofFloatColor floatColor(tempCast.get());
                            
                            if (ImGui::ColorEdit3(hiddenUniqueId.c_str(), &floatColor.r))
                            {
                                tempCast = ofColor(floatColor);
                                for(auto nodePair : selectedNodesWithoutFirst){
                                    nodePair.second->getInspectorParameters().getColor(absParam.getName()) = tempCast;
                                }
                            }
                            if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                                tempCast = tempCast;
                                for(auto nodePair : selectedNodesWithoutFirst){
                                    nodePair.second->getInspectorParameters().getColor(absParam.getName()) = tempCast;
                                }
                            }
                            //PARAM FLOAT COLOR
                            ///////////////////
                        }else if(absParam.valueType() == typeid(ofFloatColor).name()){
                            auto tempCast = absParam.cast<ofFloatColor>();
                            
                            if (ImGui::ColorEdit3(hiddenUniqueId.c_str(), (float*)&tempCast.get().r))
                            {
                                tempCast = tempCast;
                                for(auto nodePair : selectedNodesWithoutFirst){
                                    nodePair.second->getInspectorParameters().getFloatColor(absParam.getName()) = tempCast;
                                }
                            }
                            if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                                tempCast = tempCast;
                                for(auto nodePair : selectedNodesWithoutFirst){
                                    nodePair.second->getInspectorParameters().getFloatColor(absParam.getName()) = tempCast;
                                }
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
                        
                        if(!sameValue)
                            ImGui::PopStyleColor(1);
                    }
                    ImGui::PopID();
                }
            } //endFor
            ImGui::TreePop();
        }
        
        ImGui::Separator();
        ImGui::Separator();
    
    //Draw common Parameters
        ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
        if(ImGui::TreeNode("Shared Parameters")){
            auto &node = selectedNodes[0].second;
            for(int i=0 ; i < node->getParameters().size(); i++){
                ofxOceanodeAbstractParameter &absParam = static_cast<ofxOceanodeAbstractParameter&>(node->getParameters().get(i));
                bool isOutput = (absParam.getFlags() & ofxOceanodeParameterFlags_DisableInConnection);
                bool hasInConnection = absParam.hasInConnection();
                bool isShared = true;
                bool sameValue = true;
                for(auto nodePair : selectedNodesWithoutFirst){
                    if(!nodePair.second->getParameters().contains(absParam.getName())) isShared = false;
                    else{
                        ofxOceanodeAbstractParameter &_absParam = static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName()));
                        if(_absParam.valueType() != absParam.valueType()) isShared = false;
                        else if(_absParam.hasInConnection()) isShared = false;
                        else if(_absParam.getFlags() & ofxOceanodeParameterFlags_DisableInConnection) isShared = false;
                        else if(absParam.valueType() == typeid(float).name() &&
                                absParam.valueType() == typeid(int).name() &&
                                absParam.valueType() == typeid(vector<float>).name() &&
                                absParam.valueType() == typeid(vector<int>).name()){
                            bool sameRange = false;
                            if(checkSameRange<float>(absParam, _absParam)) sameRange = true;
                            if(checkSameRange<int>(absParam, _absParam)) sameRange = true;
                            if(checkSameRange<vector<float>>(absParam, _absParam)) sameRange = true;
                            if(checkSameRange<vector<int>>(absParam, _absParam)) sameRange = true;
                            
                            if(!sameRange) isShared = false;
                        }
                        
                        bool sameValueWithThis = false;
                        if(checkSameValue<float>(absParam, _absParam)) sameValueWithThis = true;
                        if(checkSameValue<int>(absParam, _absParam)) sameValueWithThis = true;
                        if(checkSameValue<vector<float>>(absParam, _absParam)) sameValueWithThis = true;
                        if(checkSameValue<vector<int>>(absParam, _absParam)) sameValueWithThis = true;
                        if(checkSameValue<bool>(absParam, _absParam)) sameValueWithThis = true;
                        if(checkSameValue<string>(absParam, _absParam)) sameValueWithThis = true;
                        if(checkSameValue<ofColor>(absParam, _absParam)) sameValueWithThis = true;
                        if(checkSameValue<ofFloatColor>(absParam, _absParam)) sameValueWithThis = true;
                        
                        if(!sameValueWithThis) sameValue = false;
                    }
                }
                if (isShared && !isOutput && !hasInConnection) {
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
                        //ImGui::SetNextItemWidth(150);
                        
                        string hiddenUniqueId = "##" + uniqueId;
                        //TODO: CAMBIAR ESTIL
                        if(!sameValue) ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0.2, 0.2, 0.2, 1));
                        //            ImGui::PushStyleColor(ImGuiCol_SliderGrab,ImVec4(node.getColor()*0.5f));
                        //            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,ImVec4(node.getColor()*0.5f));
                        //            ImGui::PushStyleColor(ImGuiCol_PlotHistogram,ImVec4(node.getColor()*0.5f));
                        //            ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(node.getColor()*.25f));
                        //            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(node.getColor()*.50f));
                        //            ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(node.getColor()*.75f));
                        
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
                                for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<float>().getParameter() = tempCast;
                                }
                            }
                            
                            if(resetValue){
                                tempCast = absParam.cast<float>().getDefaultValue();
                                for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<float>().getParameter() = tempCast;
                                }
                            }
                            
                            auto temp = tempCast.get();
                            ImGui::SliderFloat(hiddenUniqueId.c_str(), &temp, tempCast.getMin(), tempCast.getMax(), "%.4f");
                            
                            //TODO: Implement better this hack
                            // Maybe discard and reset value when not presed enter??
                            if(ImGui::IsItemDeactivated() || (ImGui::IsMouseDown(0) && ImGui::IsItemEdited()) ){
                                tempCast = ofClamp(temp, tempCast.getMin(), tempCast.getMax());
                                for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<float>().getParameter() = tempCast;
                                }
                            }
                            if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                                tempCast = tempCast;
                                for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<float>().getParameter() = tempCast;
                                }
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
                                    for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<vector<float>>().getParameter() = tempCast;
                                    }
                                }
                                
                                if(resetValue){
                                    tempCast = absParam.cast<vector<float>>().getDefaultValue();
                                    for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<vector<float>>().getParameter() = tempCast;
                                    }
                                }
                                
                                auto temp = tempCast.get()[0];
                                ImGui::SliderFloat(hiddenUniqueId.c_str(),
                                                   &temp,
                                                   tempCast.getMin()[0],
                                                   tempCast.getMax()[0], "%.4f");
                                
                                if(ImGui::IsItemDeactivated() || (ImGui::IsMouseDown(0) && ImGui::IsItemEdited())){
                                    tempCast = vector<float>(1, ofClamp(temp, tempCast.getMin()[0], tempCast.getMax()[0]));
                                    for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<vector<float>>().getParameter() = tempCast;
                                    }
                                }
                                isItemEditableByText = true;
                            }else{
                                //ImGui::PlotHistogram(hiddenUniqueId.c_str(), tempCast->data(), tempCast->size(), 0, NULL, tempCast.getMin()[0], tempCast.getMax()[0]);
                            }
                            if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                                tempCast = tempCast;
                                for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<vector<float>>().getParameter() = tempCast;
                                }
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
                                    for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<int>().getParameter() = tempCast;
                                    }
                                }
                                
                                if(resetValue){
                                    tempCast = absParam.cast<int>().getDefaultValue();
                                    for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<int>().getParameter() = tempCast;
                                    }
                                }
                                
                                auto temp = tempCast.get();
                                ImGui::SliderInt(hiddenUniqueId.c_str(), &temp, tempCast.getMin(),tempCast.getMax());
                                
                                if(ImGui::IsItemDeactivated() || (ImGui::IsMouseDown(0) && ImGui::IsItemEdited())){
                                    tempCast = ofClamp(temp, tempCast.getMin(), tempCast.getMax());
                                    for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<int>().getParameter() = tempCast;
                                    }
                                }
                                
                                if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                                    tempCast = tempCast;
                                    for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<int>().getParameter() = tempCast;
                                    }
                                }
                                
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
                                if(ImGui::Combo(hiddenUniqueId.c_str(), (int*)&tempCast.get(), vector_getter, static_cast<void*>(&options), options.size())){
                                    tempCast = ofClamp(tempCast, tempCast.getMin(), tempCast.getMax());
                                    for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<int>().getParameter() = tempCast;
                                    }
                                }
                                
                                if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                                    tempCast = tempCast;
                                    for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<int>().getParameter() = tempCast;
                                    }
                                }
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
                                    for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<vector<int>>().getParameter() = tempCast;
                                    }
                                }
                                
                                if(resetValue){
                                    tempCast = absParam.cast<vector<int>>().getDefaultValue();
                                    for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<vector<int>>().getParameter() = tempCast;
                                    }
                                }
                                
                                auto temp = tempCast.get()[0];
                                ImGui::SliderInt(hiddenUniqueId.c_str(),&temp,tempCast.getMin()[0],tempCast.getMax()[0]);
                                
                                if(ImGui::IsItemDeactivated() || (ImGui::IsMouseDown(0) && ImGui::IsItemEdited())){
                                    tempCast = vector<int>(1, ofClamp(temp, tempCast.getMin()[0], tempCast.getMax()[0]));
                                    for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<vector<int>>().getParameter() = tempCast;
                                    }
                                }
                                isItemEditableByText = true;
                            }
                            else{
//                                std::vector<float> floatVec(tempCast.get().begin(), tempCast.get().end());
//                                ImGui::PlotHistogram(hiddenUniqueId.c_str(), floatVec.data(), tempCast->size(), 0, NULL, tempCast.getMin()[0], tempCast.getMax()[0]);
                            }
                            if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                                tempCast = tempCast;
                                for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<vector<int>>().getParameter() = tempCast;
                                }
                            }
                        }
                        // PARAM BOOL
                        /////////////
                        else if(absParam.valueType() == typeid(bool).name()){
                            auto tempCast = absParam.cast<bool>().getParameter();
                            
                            if(drag != 0){
                                absParam.cast<bool>().applyNormalDrag(drag);
                                for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<bool>().getParameter() = tempCast;
                                }
                            }
                            
                            if (ImGui::Checkbox(hiddenUniqueId.c_str(), (bool *)&tempCast.get()))
                            {
                                tempCast = tempCast;
                                for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<bool>().getParameter() = tempCast;
                                }
                            }
                            if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                                tempCast = !tempCast;
                                for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<bool>().getParameter() = tempCast;
                                }
                            }
                            // PARAM VOID
                            /////////////
                        }else if(absParam.valueType() == typeid(void).name()){
                            auto tempCast = absParam.cast<void>().getParameter();
                            if (ImGui::Button(hiddenUniqueId.c_str(), ImVec2(ImGui::GetFrameHeight(), 0)))
                            {
                                tempCast.trigger();
                                for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<void>().getParameter().trigger();
                                }
                            }
                            if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                                tempCast.trigger();
                                for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<void>().getParameter().trigger();
                                }
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
                                for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<string>().getParameter() = tempCast;
                                }
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
                                for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<ofColor>().getParameter() = tempCast;
                                }
                            }
                            
                            ofFloatColor floatColor(tempCast.get());
                            
                            if (ImGui::ColorEdit3(hiddenUniqueId.c_str(), &floatColor.r))
                            {
                                tempCast = ofColor(floatColor);
                                for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<ofColor>().getParameter() = tempCast;
                                }
                            }
                            if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                                tempCast = tempCast;
                                for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<ofColor>().getParameter() = tempCast;
                                }
                            }
                            //PARAM FLOAT COLOR
                            ///////////////////
                        }else if(absParam.valueType() == typeid(ofFloatColor).name()){
                            auto tempCast = absParam.cast<ofFloatColor>().getParameter();
                            
                            if(drag != 0){
                                if(ImGui::GetIO().KeyShift) absParam.cast<ofFloatColor>().applyPrecisionDrag(drag);
                                else if(ImGui::GetIO().KeyAlt) absParam.cast<ofFloatColor>().applySpeedDrag(drag);
                                else absParam.cast<ofFloatColor>().applyNormalDrag(drag);
                                for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<ofFloatColor>().getParameter() = tempCast;
                                }
                            }
                            
                            if (ImGui::ColorEdit3(hiddenUniqueId.c_str(), (float*)&tempCast.get().r))
                            {
                                tempCast = tempCast;
                                for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<ofFloatColor>().getParameter() = tempCast;
                                }
                            }
                            if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                                tempCast = tempCast;
                                for(auto nodePair : selectedNodesWithoutFirst){ static_cast<ofxOceanodeAbstractParameter&>(nodePair.second->getParameters().get(absParam.getName())).cast<ofFloatColor>().getParameter() = tempCast;
                                }
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
                        
                        if(!sameValue)
                            ImGui::PopStyleColor(1);
                    }
                    ImGui::PopID();
                }
            } //endFor
            ImGui::TreePop();
        }
        
        ImGui::Separator();
        ImGui::Separator();
    
        ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
        if(ImGui::TreeNode("Scope")){
            for(auto nodePair : selectedNodes){
                auto &nodeGui = nodePair.second->getNodeGui();
                for(int i=0 ; i<nodeGui.getParameters().size(); i++){
                    ofxOceanodeAbstractParameter &p = static_cast<ofxOceanodeAbstractParameter&>(nodeGui.getParameters().get(i));
                    if((p.getFlags() & ofxOceanodeParameterFlags_DisableInConnection)){
                        auto size = ImVec2(ImGui::GetContentRegionAvailWidth(), 100);
                        
                        ImGui::PushStyleColor(ImGuiCol_SliderGrab,ImVec4(nodeGui.getColor()*0.75f));
                        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,ImVec4(nodeGui.getColor()*0.75f));
                        ImGui::PushStyleColor(ImGuiCol_PlotHistogram,ImVec4(nodeGui.getColor()*0.75f));
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55,0.55,0.55,1.0));
                        ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(0.0,0.0,0.0,0.0));
                        
                        ImGui::BeginChild(("Child_" + p.getGroupHierarchyNames().front() + "/" + p.getName()).c_str(), size, true);
                        //ImGui::SameLine();
                        ImGui::Text((p.getGroupHierarchyNames().front() + "/" + p.getName()).c_str());
                        
                        // f() function to properly draw each scope item
                        for(auto f : ofxOceanodeScope::getInstance()->getScopedTypes())
                        {
                            if(f(&p, size)) break;
                        }
                        
                        ImGui::PopStyleColor(5);
                        ImGui::EndChild();
                    }
                }
            }
            ImGui::TreePop();
        }
        ImGui::PopStyleVar();

    }
    else{
        ImGui::Text("No Nodes Selected", "%s");
    }
}
