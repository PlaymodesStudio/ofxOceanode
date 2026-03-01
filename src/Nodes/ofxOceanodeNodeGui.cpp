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
#include "imgui.h"
#include "ofxOceanodeParameter.h"
#include "ofxOceanodeScope.h"
#include "ofxOceanodeTime.h"

ofxOceanodeNodeGui::ofxOceanodeNodeGui(ofxOceanodeContainer& _container, ofxOceanodeNode& _node) : container(_container), node(_node){
    color = node.getColor();
    //color.setBrightness(255);
    guiRect = ofRectangle(10, 10, 0, 0);
    guiToBeDestroyed = false;
    lastExpandedState = true;
    isGuiCreated = false;
    visible = true;
    
    expanded = true;
#ifdef OFXOCEANODE_USE_MIDI
    isListeningMidi = false;
#endif
}

ofxOceanodeNodeGui::~ofxOceanodeNodeGui(){
    
}

bool ofxOceanodeNodeGui::constructGui(int nodeWidthText, int nodeWidthWidget){
    string moduleName = getParameters().getName();
	
	bool isTransparent = (node.getNodeModel().getFlags() & ofxOceanodeNodeModelFlags_TransparentNode);

    ImGui::BeginGroup(); // Lock horizontal position
    
    bool deleteModule = false;
    
	if (!isTransparent) {
		
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
	}
    
    if(inputPositions.size() != getParameters().size()){
        inputPositions.resize(getParameters().size());
        outputPositions.resize(getParameters().size());
    }
    
    if(expanded){
        
        ImGui::Spacing();
        
        auto startPos = ImGui::GetCursorScreenPos();
        
        for(int i=0 ; i<getParameters().size(); i++){
            ofxOceanodeAbstractParameter &absParam = static_cast<ofxOceanodeAbstractParameter&>(getParameters().get(i));
            string uniqueId = absParam.getName();
            if(absParam.getFlags() & ofxOceanodeParameterFlags_ReadOnly) ImGui::BeginDisabled();
            ImGui::PushID(uniqueId.c_str());
            if(absParam.getFlags() & ofxOceanodeParameterFlags_NoGuiWidget){
                ImGui::Dummy(ImVec2(0, 0));
                inputPositions[i] = glm::vec2(0, ImGui::GetItemRectMin().y);
                outputPositions[i] = glm::vec2(0, ImGui::GetItemRectMin().y);
                ImGui::PopID();
                if(absParam.getFlags() & ofxOceanodeParameterFlags_ReadOnly) ImGui::EndDisabled();
                continue;
            }
            if(absParam.valueType() == typeid(std::function<void()>).name()){
                // Check if this is a separator by looking at the parameter name
                if(uniqueId.find("SEPARATOR:|") == 0){
                    // Parse separator data: "SEPARATOR:|label|r,g,b,a"
                    vector<string> parts = ofSplitString(uniqueId, "|");
                    string label = parts.size() > 1 ? parts[1] : "";
                    ofColor color(200, 200, 200, 255); // default
                    
                    if(parts.size() > 2){
                        vector<string> colorParts = ofSplitString(parts[2], ",");
                        if(colorParts.size() >= 4){
                            color.r = ofToInt(colorParts[0]);
                            color.g = ofToInt(colorParts[1]);
                            color.b = ofToInt(colorParts[2]);
                            color.a = ofToInt(colorParts[3]);
                        }
                    }
                    
                    // Render separator with label and background highlight
                    if(!label.empty()){
                        ImVec2 p = ImGui::GetCursorScreenPos();
                        float w = guiRect.width;
                        
                        // Draw the text
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(color.r/255.0f, color.g/255.0f, color.b/255.0f, color.a/255.0f));
                        ImGui::TextUnformatted(label.c_str());
                        ImGui::PopStyleColor();
                        
                        // Get the size of the rendered text
                        ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
                        
                        // Draw a semi-transparent rectangle behind/below the text (25% opacity)
                        ImU32 bgCol = IM_COL32(color.r, color.g, color.b, color.a * 0.15f);
                        ImGui::GetWindowDrawList()->AddRectFilled(
                            ImVec2(p.x, p.y),
                            ImVec2(p.x + w - 16 , p.y + textSize.y),
                            bgCol
                        );
                        
                        ImGui::Dummy(ImVec2(0, 2));
                    }
                }else{
                    // Regular custom region function
                    absParam.cast<std::function<void()>>().getParameter().get()();
                }
            }else{
                
                ImGui::Text("%s", uniqueId.c_str());
                
                ImGui::SetItemAllowOverlap();
                ImGui::SameLine(-1);
				ImGui::InvisibleButton(("##InvBut_" + uniqueId).c_str(), ImVec2(nodeWidthText, ImGui::GetFrameHeight())); //Used to check later behaviours
                
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
                
				// [ node width ] : this was 90 pixels for the name and 150 for the "widget"
				ImGui::SameLine(nodeWidthText);
				ImGui::SetNextItemWidth(nodeWidthWidget);
                
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
                    
                    if(absParam.cast<float>().getDropdownOptions().size() == 0)
                    {
                        if(drag != 0){
                            if(ImGui::GetIO().KeyShift) absParam.cast<float>().applyPrecisionDrag(drag);
                            else if(ImGui::GetIO().KeyAlt) absParam.cast<float>().applySpeedDrag(drag);
                            else absParam.cast<float>().applyNormalDrag(drag);
                        }
                        
                        if(resetValue){
                            tempCast = absParam.cast<float>().getDefaultValue();
                        }
                        
                        auto temp = tempCast.get();
                        if(tempCast.getMin() == std::numeric_limits<float>::lowest() || tempCast.getMax() == std::numeric_limits<float>::max()){
                            ImGui::DragFloat(hiddenUniqueId.c_str(), &temp, 0.001, tempCast.getMin(), tempCast.getMax());
                        }else{
                            ImGui::SliderFloat(hiddenUniqueId.c_str(), &temp, tempCast.getMin(), tempCast.getMax(), "%.4f");
                        }
                        
                        //TODO: Implement better this hack
                        // Maybe discard and reset value when not presed enter??
                        if(ImGui::IsItemDeactivated() || (ImGui::IsMouseDown(0) && ImGui::IsItemEdited()) ){
                            tempCast = ofClamp(temp, tempCast.getMin(), tempCast.getMax());
                        }
                        if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGuiKey_Space)){
                            tempCast = tempCast;
                        }
                        isItemEditableByText = true;
                    }
                    else{
                        auto vector_getter = [](void* vec, int idx, const char** out_text)
                        {
                            auto& vector = *static_cast<std::vector<std::string>*>(vec);
                            if (idx < 0 || idx >= static_cast<int>(vector.size())) { return false; }
                            *out_text = vector.at(idx).c_str();
                            return true;
                        };
                        
                        int temp = tempCast.get();
                        
                        vector<string> options = absParam.cast<float>().getDropdownOptions();
                        if(tempCast.get() != temp){
                            for(int op_idx = 0; op_idx < options.size()-1; op_idx++){
                                options[op_idx] += " ---> " + options[op_idx+1];
                            }
                        }
                        if(ImGui::Combo(hiddenUniqueId.c_str(), &temp, vector_getter, static_cast<void*>(&options), options.size()))
                            tempCast = ofClamp(temp, tempCast.getMin(), tempCast.getMax());
                            
                        
                        if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGuiKey_Space))
                            tempCast = tempCast;
                    }
                }
                // PARAM VECTOR < FLOAT >
                /////////////////////////
                else if(absParam.valueType() == typeid(vector<float>).name())
                {
                    auto tempCast = absParam.cast<vector<float>>().getParameter();
                    if(tempCast->size() == 1)
                    {
                        if(absParam.cast<vector<float>>().getDropdownOptions().size() == 0)
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
                            if(tempCast.getMin()[0] == std::numeric_limits<float>::lowest() || tempCast.getMax()[0] == std::numeric_limits<float>::max()){
                                ImGui::DragFloat(hiddenUniqueId.c_str(), &temp, 0.001, tempCast.getMin()[0], tempCast.getMax()[0]);
                            }else{
                                ImGui::SliderFloat(hiddenUniqueId.c_str(), &temp, tempCast.getMin()[0], tempCast.getMax()[0], "%.4f");
                            }
                            
                            if(ImGui::IsItemDeactivated() || (ImGui::IsMouseDown(0) && ImGui::IsItemEdited())){
                                tempCast = vector<float>(1, ofClamp(temp, tempCast.getMin()[0], tempCast.getMax()[0]));
                            }
                            isItemEditableByText = true;
                        }
                        else{
                            auto vector_getter = [](void* vec, int idx, const char** out_text)
                            {
                                auto& vector = *static_cast<std::vector<std::string>*>(vec);
                                if (idx < 0 || idx >= static_cast<int>(vector.size())) { return false; }
                                *out_text = vector.at(idx).c_str();
                                return true;
                            };
                            
                            int temp = tempCast.get()[0];
                            
                            vector<string> options = absParam.cast<vector<float>>().getDropdownOptions();
                            if(tempCast.get()[0] != temp){
                                for(int op_idx = 0; op_idx < options.size()-1; op_idx++){
                                    options[op_idx] += " ---> " + options[op_idx+1];
                                }
                            }
                            if(ImGui::Combo(hiddenUniqueId.c_str(), &temp, vector_getter, static_cast<void*>(&options), options.size()))
                                tempCast = vector<float>(1, ofClamp(temp, tempCast.getMin()[0], tempCast.getMax()[0]));
                                
                            
                            if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGuiKey_Space))
                                tempCast = tempCast;
                        }
                    }else{
                        if(tempCast.getMin()[0] == std::numeric_limits<float>::lowest() || tempCast.getMax()[0] == std::numeric_limits<float>::max()){
                            ImGui::PlotHistogram(hiddenUniqueId.c_str(), tempCast->data(), tempCast->size(), 0, NULL, *std::min_element(tempCast->begin(), tempCast->end()), *std::max_element(tempCast->begin(), tempCast->end()));
                        }else{
                            ImGui::PlotHistogram(hiddenUniqueId.c_str(), tempCast->data(), tempCast->size(), 0, NULL, tempCast.getMin()[0], tempCast.getMax()[0]);
                        }
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
                        if(tempCast.getMin() == std::numeric_limits<int>::lowest() || tempCast.getMax() == std::numeric_limits<int>::max()){
                            ImGui::DragInt(hiddenUniqueId.c_str(), &temp, 1, tempCast.getMin(), tempCast.getMax());
                        }else{
                            ImGui::SliderInt(hiddenUniqueId.c_str(), &temp, tempCast.getMin(),tempCast.getMax());
                        }
                        
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
                        if(absParam.cast<vector<int>>().getDropdownOptions().size() == 0)
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
                            if(tempCast.getMin()[0] == std::numeric_limits<int>::lowest() || tempCast.getMax()[0] == std::numeric_limits<int>::max()){
                                ImGui::DragInt(hiddenUniqueId.c_str(), &temp, 1, tempCast.getMin()[0], tempCast.getMax()[0]);
                            }else{
                                ImGui::SliderInt(hiddenUniqueId.c_str(), &temp, tempCast.getMin()[0], tempCast.getMax()[0]);
                            }
                            
                            if(ImGui::IsItemDeactivated() || (ImGui::IsMouseDown(0) && ImGui::IsItemEdited())){
                                tempCast = vector<int>(1, ofClamp(temp, tempCast.getMin()[0], tempCast.getMax()[0]));
                            }
                            isItemEditableByText = true;
                        }
                        else{
                            auto vector_getter = [](void* vec, int idx, const char** out_text)
                            {
                                auto& vector = *static_cast<std::vector<std::string>*>(vec);
                                if (idx < 0 || idx >= static_cast<int>(vector.size())) { return false; }
                                *out_text = vector.at(idx).c_str();
                                return true;
                            };
                            
                            vector<string> options = absParam.cast<int>().getDropdownOptions();
                            if(ImGui::Combo(hiddenUniqueId.c_str(), (int*)&tempCast.get()[0], vector_getter, static_cast<void*>(&options), options.size()))
                                tempCast = vector<int>(1, ofClamp(tempCast.get()[0], tempCast.getMin()[0], tempCast.getMax()[0]));
                            
                            if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGuiKey_Space))
                                tempCast = tempCast;
                        }
                    }
                    else{
                        std::vector<float> floatVec(tempCast.get().begin(), tempCast.get().end());
                        if(tempCast.getMin()[0] == std::numeric_limits<int>::lowest() || tempCast.getMax()[0] == std::numeric_limits<int>::max()){
                            ImGui::PlotHistogram(hiddenUniqueId.c_str(), floatVec.data(), tempCast->size(), 0, NULL, *std::min_element(tempCast->begin(), tempCast->end()), *std::max_element(tempCast->begin(), tempCast->end()));
                        }else{
                            ImGui::PlotHistogram(hiddenUniqueId.c_str(), floatVec.data(), tempCast->size(), 0, NULL, tempCast.getMin()[0], tempCast.getMax()[0]);
                        }
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
					ImVec2 sizeCB = ImGui::GetItemRectSize();
                    if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                        tempCast = !tempCast;
                    }
					ImGui::SameLine();
					ImGui::Dummy(ImVec2(nodeWidthWidget-sizeCB.x-8,ImGui::GetFrameHeight()));
					
                    // PARAM VOID
                    /////////////
                }else if(absParam.valueType() == typeid(void).name()){
                    auto tempCast = absParam.cast<void>().getParameter();
                    if (ImGui::Button(hiddenUniqueId.c_str(), ImVec2(ImGui::GetFrameHeight(), 0)))
                    {
                        tempCast.trigger();
                    }
					ImVec2 sizeCB = ImGui::GetItemRectSize();
                    if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                        tempCast.trigger();
                    }
					ImGui::SameLine();
					ImGui::Dummy(ImVec2(nodeWidthWidget-sizeCB.x-8,ImGui::GetFrameHeight()));
				// PARAM STRING
				///////////////
                }else if(absParam.valueType() == typeid(string).name()){
					auto tempCast = absParam.cast<string>().getParameter();
					size_t bufferSize = max(static_cast<size_t>(1024), tempCast.get().length() + 256);
					char * cString = new char[bufferSize];
					strncpy(cString, tempCast.get().c_str(), bufferSize - 1);
					cString[bufferSize - 1] = '\0';
					auto result = false;
					if (ImGui::InputText(hiddenUniqueId.c_str(), cString, bufferSize, ImGuiInputTextFlags_EnterReturnsTrue))
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
                }else if(absParam.valueType() == typeid(time_t).name()){
					auto tm_ = *std::localtime(&absParam.cast<time_t>().getParameter().get());
					std::stringstream str;
					constexpr int bufsize = 256;
					char buf[bufsize];
				
					if (strftime(buf,bufsize, "%d/%m/%Y-%H:%M:%S", &tm_) != 0){
						str << buf;
					}
					ImGui::Text("%s", str.str().c_str());
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
                
				// Push custom style colors
				ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));  // Dark background
				ImGui::PushStyleColor(ImGuiCol_Text,     ImVec4(0.7f, 0.7f, 0.7f, 0.7f));  // White text
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f)); // Hovered button
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
				
                if(ImGui::BeginPopup("Param Popup")){
                    ImGui::Separator();
                    if(!absParam.isScoped()){ //Param is not scoped
                        if(ImGui::Selectable("Add to Scope")){
                            ofxOceanodeScope::getInstance()->addParameter(&absParam,node.getColor());
                        }
                    }else{
                        if(ImGui::Selectable("Remove from Scope")){
                            ofxOceanodeScope::getInstance()->removeParameter(&absParam);
                        }
                    }
                    ImGui::Separator();
                    if(!absParam.isTimelined()){ //Param is not timelined
                        if(ImGui::Selectable("Add to Timeline")){
                            ofxOceanodeTime::getInstance()->addParameter(&absParam,node.getColor());
                        }
                    }else{
                        if(ImGui::Selectable("Remove from Timeline")){
                            ofxOceanodeTime::getInstance()->removeParameter(&absParam);
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
					ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));  // Dark background
					ImGui::PushStyleColor(ImGuiCol_Text,     ImVec4(0.7f, 0.7f, 0.7f, 0.7f));  // White text

                    //ImGui::Text("OSC Address: %s/%s", getParameters().getEscapedName().c_str(), absParam.getEscapedName().c_str());
					ImGui::Text("OSC Address");
					if (ImGui::IsItemHovered())
					{
						string tt = getParameters().getEscapedName() + "/" +absParam.getEscapedName();
						ImGui::SetTooltip(tt.c_str());
					}
					ImGui::PopStyleColor(2);

#endif
                    ImGui::Separator();
                    ImGui::EndPopup();
                }
				// Always pop the same number you pushed
				ImGui::PopStyleColor(3);
				ImGui::PopStyleVar();

                ImGui::PopStyleColor(6);
            }
            inputPositions[i] = glm::vec2(0, ImGui::GetItemRectMin().y + ImGui::GetItemRectSize().y/2);
            outputPositions[i] = glm::vec2(0, ImGui::GetItemRectMin().y + ImGui::GetItemRectSize().y/2);
            
            ImGui::PopID();
            if(absParam.getFlags() & ofxOceanodeParameterFlags_ReadOnly) ImGui::EndDisabled();
        } //endFor
    }
	else
	{
		ImGui::Spacing();
		
		// First pass: calculate total height needed for all minimized parameters
		float totalHeight = 0;
		float spacing = 2; // Spacing between parameters
		std::vector<ImVec2> paramSizes;
		
		for(int i=0 ; i<getParameters().size(); i++)
		{
			ofxOceanodeAbstractParameter &absParam = static_cast<ofxOceanodeAbstractParameter&>(getParameters().get(i));
			if((absParam.getFlags() & ofxOceanodeParameterFlags_DisplayMinimized))
			{
				ImVec2 size;
				if(absParam.valueType() == typeid(ofTexture*).name())
				{
					bool keepAspectRatio = (absParam.getFlags() & ofxOceanodeParameterFlags_ScopeKeepAspectRatio);
					if(keepAspectRatio)
					{
						auto tempCast = absParam.cast<ofTexture*>().getParameter();
						float ar = tempCast.get()->getWidth() / tempCast.get()->getHeight();
						size = ImVec2(nodeWidthText+nodeWidthWidget,nodeWidthText+nodeWidthWidget/ar);
					}
					else
					{
						size = ImVec2(nodeWidthText+nodeWidthWidget, nodeWidthText+nodeWidthWidget);
					}
				}
				else size = ImVec2(nodeWidthText+nodeWidthWidget,30);
				
				paramSizes.push_back(size);
				totalHeight += size.y + spacing;
			}
		}
		
		// Remove last spacing if we added any parameters
		if(totalHeight > 0) totalHeight -= spacing;
		
		// Second pass: render parameters with proper positioning
		float currentY = 0;
		int minimizedParamIndex = 0;
		
		for(int i=0 ; i<getParameters().size(); i++)
		{
			ofxOceanodeAbstractParameter &absParam2 = static_cast<ofxOceanodeAbstractParameter&>(getParameters().get(i));
			if((absParam2.getFlags() & ofxOceanodeParameterFlags_DisplayMinimized))
			{
				// Style setup for minimized parameters
				ImGui::PushStyleColor(ImGuiCol_SliderGrab,ImVec4(node.getColor()*0.5f));
				ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,ImVec4(node.getColor()*0.5f));
				ImGui::PushStyleColor(ImGuiCol_PlotHistogram,ImVec4(node.getColor()*0.5f));
				ImGui::PushStyleColor(ImGuiCol_PlotHistogram,ImVec4(node.getColor()*0.5f));
				ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(node.getColor()*.25f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(node.getColor()*.50f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(node.getColor()*.75f));
				
				ImVec2 size = paramSizes[minimizedParamIndex];
				
				// Set cursor position for this parameter
				if(currentY > 0) {
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() + currentY);
				}
				
				// Get the position where we'll draw
				ImVec2 pos = ImGui::GetCursorScreenPos();
				ImDrawList* draw_list = ImGui::GetWindowDrawList();
				
				// Draw background manually
				ImU32 bg_color = ImGui::GetColorU32(ImGuiCol_ChildBg);
				ImU32 border_color = ImGui::GetColorU32(ImGuiCol_Border);
				draw_list->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), bg_color);
				draw_list->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), border_color);
				
				// Create a clipping rectangle to constrain drawing
				ImGui::PushClipRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), true);
				
				// Save current cursor position
				ImVec2 savedCursorPos = ImGui::GetCursorPos();
				
				// Position cursor inside the "fake child" area for rendering
				ImGui::SetCursorScreenPos(pos);
				
				// Render the content using scope functions
				// IMPORTANT: We must break after the first scope function that handles the parameter
				// to avoid multiple renderings. Each scope function returns true if it handled the parameter.
								
				for(auto f : ofxOceanodeScope::getInstance()->getScopedTypes())
				{
					if(f(&absParam2, size))
					{
						break; // Stop after first successful handler
					}
				}
								
				// Restore clipping
				ImGui::PopClipRect();
				
				// Restore cursor position and advance it by the size we used
				ImGui::SetCursorPos(savedCursorPos);
				ImGui::SetCursorPosY(savedCursorPos.y + size.y);
				
				ImGui::PopStyleColor(7);
				
				// Update Y position for next parameter
				currentY = spacing;
				minimizedParamIndex++;
			}
		}
		
		// Add an invisible item to ensure the group has the correct size
		if(totalHeight > 0) {
			ImGui::InvisibleButton("##nodesize", ImVec2(nodeWidthText+nodeWidthWidget, 0.1f));
		}
	}
				
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
