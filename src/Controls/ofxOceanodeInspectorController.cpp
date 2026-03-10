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
#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeNodeMacro.h"
#include "imgui.h"

std::map<std::string, std::vector<std::string>> ofxOceanodeInspectorController::inspectorDropdownOptions;

void ofxOceanodeInspectorController::registerInspectorDropdown(const std::string& nodeTypeName, const std::string& paramName, const std::vector<std::string>& options) {
	std::string key = nodeTypeName + "::" + paramName;
	inspectorDropdownOptions[key] = options;
}

std::vector<std::string> ofxOceanodeInspectorController::getInspectorDropdownOptions(const std::string& nodeTypeName, const std::string& paramName) {
	std::string key = nodeTypeName + "::" + paramName;
	auto it = inspectorDropdownOptions.find(key);
	return (it != inspectorDropdownOptions.end()) ? it->second : std::vector<std::string>();
}

void ofxOceanodeInspectorController::draw(){
    vector<pair<string, ofxOceanodeNode*>> nodesInThisFrame = vector<pair<string, ofxOceanodeNode*>>(container->getParameterGroupNodesMap().begin(), container->getParameterGroupNodesMap().end());
    
    vector<pair<string, ofxOceanodeNode*>> selectedNodes;
	
	std::function<void(vector<pair<string, ofxOceanodeNode*>>)> getSelectedModules = [&selectedNodes, &getSelectedModules](vector<pair<string, ofxOceanodeNode*>> nodes){
		for(auto nodePair : nodes)
		{
			auto &nodeGui = nodePair.second->getNodeGui();
			if(nodeGui.getSelected()){
				selectedNodes.push_back(nodePair);
			}
			if (ofxOceanodeNodeMacro* m = dynamic_cast<ofxOceanodeNodeMacro*>(&nodePair.second->getNodeModel())) {
				getSelectedModules(vector<pair<string, ofxOceanodeNode*>>(m->getContainer()->getParameterGroupNodesMap().begin(), m->getContainer()->getParameterGroupNodesMap().end()));
			}
		}
	};
	
	getSelectedModules(nodesInThisFrame);

    // Only show inspector when exactly one node is selected
    if(selectedNodes.size() != 1){
        if(selectedNodes.size() == 0)
            ImGui::Text("No Nodes Selected", "%s");
        else
            ImGui::Text("Multiple Nodes Selected", "%s");
        return;
    }

    auto &node = selectedNodes[0].second;

    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 0.0f);

    // Node name and description
    ImGui::PushStyleColor(ImGuiCol_Text, node->getColor());
    ImGui::Text(selectedNodes[0].first.c_str(), "%s");
    ImGui::PopStyleColor();

    // Inspector Parameters
    ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
    if(ImGui::TreeNode("Inspector Parameters")){
        for(int i = 0; i < node->getInspectorParameters().size(); i++){
            ofAbstractParameter &absParam = node->getInspectorParameters().get(i);
            string uniqueId = absParam.getName();
            ImGui::PushID(uniqueId.c_str());

            if(absParam.valueType() == typeid(std::function<void()>).name()){
                absParam.cast<std::function<void()>>().get()();
            } else {
                ImGui::Text("%s", uniqueId.c_str());
                ImGui::SameLine();
                string hiddenUniqueId = "##" + uniqueId;
                bool isItemEditableByText = false;

                // PARAM FLOAT
                if(absParam.valueType() == typeid(float).name()){
                    auto tempCast = absParam.cast<float>();
                    auto temp = tempCast.get();
                    if(tempCast.getMin() == std::numeric_limits<float>::lowest() || tempCast.getMax() == std::numeric_limits<float>::max()){
                        ImGui::DragFloat(hiddenUniqueId.c_str(), &temp, 0.001, tempCast.getMin(), tempCast.getMax());
                    } else {
                        ImGui::SliderFloat(hiddenUniqueId.c_str(), &temp, tempCast.getMin(), tempCast.getMax(), "%.4f");
                    }
                    if(ImGui::IsItemDeactivated() || (ImGui::IsMouseDown(0) && ImGui::IsItemEdited())){
                        tempCast = ofClamp(temp, tempCast.getMin(), tempCast.getMax());
                    }
                    isItemEditableByText = true;
                }
                // PARAM INT
                else if(absParam.valueType() == typeid(int).name()){
                    auto tempCast = absParam.cast<int>();
                    std::string nodeTypeName = node->getNodeModel().nodeName();
                    std::vector<std::string> dropdownOptions = getInspectorDropdownOptions(nodeTypeName, absParam.getName());

                    if(dropdownOptions.empty()){
                        auto temp = tempCast.get();
                        if(tempCast.getMin() == std::numeric_limits<int>::lowest() || tempCast.getMax() == std::numeric_limits<int>::max()){
                            ImGui::DragInt(hiddenUniqueId.c_str(), &temp, 1, tempCast.getMin(), tempCast.getMax());
                        } else {
                            ImGui::SliderInt(hiddenUniqueId.c_str(), &temp, tempCast.getMin(), tempCast.getMax());
                        }
                        if(ImGui::IsItemDeactivated() || (ImGui::IsMouseDown(0) && ImGui::IsItemEdited())){
                            tempCast = ofClamp(temp, tempCast.getMin(), tempCast.getMax());
                        }
                        isItemEditableByText = true;
                    } else {
                        auto vector_getter = [](void* vec, int idx, const char** out_text){
                            auto& vector = *static_cast<std::vector<std::string>*>(vec);
                            if (idx < 0 || idx >= static_cast<int>(vector.size())) return false;
                            *out_text = vector.at(idx).c_str();
                            return true;
                        };
                        if(ImGui::Combo(hiddenUniqueId.c_str(), (int*)&tempCast.get(), vector_getter, static_cast<void*>(&dropdownOptions), dropdownOptions.size())){
                            tempCast = ofClamp(tempCast, tempCast.getMin(), tempCast.getMax());
                        }
                    }
                }
                // PARAM BOOL
                else if(absParam.valueType() == typeid(bool).name()){
                    auto tempCast = absParam.cast<bool>();
                    if(ImGui::Checkbox(hiddenUniqueId.c_str(), (bool *)&tempCast.get())){
                        tempCast = tempCast;
                    }
                    if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                        tempCast = !tempCast;
                    }
                }
                // PARAM VOID
                else if(absParam.valueType() == typeid(void).name()){
                    auto tempCast = absParam.cast<void>();
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
                    if(ImGui::Button(hiddenUniqueId.c_str(), ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()))){
                        tempCast.trigger();
                    }
                    ImGui::PopStyleColor(3);
                    if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))){
                        tempCast.trigger();
                    }
                }
                // PARAM STRING
                else if(absParam.valueType() == typeid(string).name()){
                    auto tempCast = absParam.cast<string>();
                    string currentText = tempCast.get();
                    size_t bufferSize = max(static_cast<size_t>(1024), currentText.length() + 256);
                    char* cString = new char[bufferSize];
                    strncpy(cString, currentText.c_str(), bufferSize - 1);
                    cString[bufferSize - 1] = '\0';
                    if(ImGui::InputText(hiddenUniqueId.c_str(), cString, bufferSize, ImGuiInputTextFlags_EnterReturnsTrue)){
                        tempCast = cString;
                    }
                    delete[] cString;
                    isItemEditableByText = true;
                }
                // PARAM CHAR
                else if(absParam.valueType() == typeid(char).name()){
                    ImGui::Dummy(ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
                }
                // PARAM COLOR
                else if(absParam.valueType() == typeid(ofColor).name()){
                    auto tempCast = absParam.cast<ofColor>();
                    ofFloatColor floatColor(tempCast.get());
                    if(ImGui::ColorEdit3(hiddenUniqueId.c_str(), &floatColor.r)){
                        tempCast = ofColor(floatColor);
                    }
                }
                // PARAM FLOAT COLOR
                else if(absParam.valueType() == typeid(ofFloatColor).name()){
                    auto tempCast = absParam.cast<ofFloatColor>();
                    if(ImGui::ColorEdit3(hiddenUniqueId.c_str(), (float*)&tempCast.get().r)){
                        tempCast = tempCast;
                    }
                }
                // UNKNOWN PARAM
                else {
                    ImGui::Dummy(ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
                }

                if(isItemEditableByText){
                    if((ImGui::IsItemHovered() && !ImGui::IsItemEdited() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter))) || ImGui::IsItemClicked(1)){
                        ImGui::SetKeyboardFocusHere(-1);
                    }
                }
            }
            ImGui::PopID();
        }
        ImGui::TreePop();
    }

    ImGui::Separator();

    // Scope - full available space with draggable separators
    {
        auto &nodeGui = node->getNodeGui();

        // Collect scope parameter indices
        vector<int> scopeIndices;
        for(int i = 0; i < nodeGui.getParameters().size(); i++){
            ofxOceanodeAbstractParameter &p = static_cast<ofxOceanodeAbstractParameter&>(nodeGui.getParameters().get(i));
            if(p.getFlags() & ofxOceanodeParameterFlags_DisableInConnection){
                scopeIndices.push_back(i);
            }
        }

        int scopeCount = (int)scopeIndices.size();
        if(scopeCount > 0){
            const float splitterThickness = 6.0f;
            const float minPaneHeight = 30.0f;
            float itemSpacing = ImGui::GetStyle().ItemSpacing.y;
            float availableHeight = ImGui::GetContentRegionAvail().y;
            // Each panel-splitter-panel sequence adds 2 ItemSpacing gaps per splitter
            float totalSplitterHeight = (scopeCount - 1) * (splitterThickness + 2.0f * itemSpacing);
            float totalContentHeight = availableHeight - totalSplitterHeight;

            // Persistent heights keyed by node name, with resize tracking
            static std::map<string, vector<float>> scopeHeights;
            static std::map<string, float> scopePrevAvailHeight;
            string nodeKey = selectedNodes[0].first;
            auto &heights = scopeHeights[nodeKey];
            float &prevAvailHeight = scopePrevAvailHeight[nodeKey];

            if((int)heights.size() != scopeCount){
                // First time or scope count changed: distribute evenly
                heights.assign(scopeCount, totalContentHeight / scopeCount);
                prevAvailHeight = availableHeight;
            } else if(prevAvailHeight > 0.0f && prevAvailHeight != availableHeight){
                // Window resized: scale all heights proportionally
                float prevTotalContent = prevAvailHeight - totalSplitterHeight;
                if(prevTotalContent > 0.0f){
                    float scale = totalContentHeight / prevTotalContent;
                    for(auto &h : heights){
                        h *= scale;
                        h = max(h, minPaneHeight);
                    }
                }
                prevAvailHeight = availableHeight;
            }

            for(int si = 0; si < scopeCount; si++){
                int i = scopeIndices[si];
                ofxOceanodeAbstractParameter &p = static_cast<ofxOceanodeAbstractParameter&>(nodeGui.getParameters().get(i));

                float childHeight = max(heights[si], minPaneHeight);
                auto size = ImVec2(ImGui::GetContentRegionAvail().x, childHeight);

                ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(nodeGui.getColor()*0.75f));
                ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(nodeGui.getColor()*0.75f));
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(nodeGui.getColor()*0.75f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55, 0.55, 0.55, 1.0));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0, 0.0, 0.0, 0.0));

                ImGui::BeginChild(("Child_" + p.getGroupHierarchyNames().front() + "/" + p.getName()).c_str(), size, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                ImGui::Text("%s", (p.getGroupHierarchyNames().front() + "/" + p.getName()).c_str());

                for(auto f : ofxOceanodeScope::getInstance()->getScopedTypes()){
                    if(f(&p, size)) break;
                }

                ImGui::PopStyleColor(5);
                ImGui::EndChild();

                // Draggable splitter between panes
                if(si < scopeCount - 1){
                    string splitterId = "##scopesplitter_" + to_string(si);

                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.28f, 0.28f, 0.6f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.50f, 0.50f, 0.50f, 0.8f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.65f, 0.65f, 0.65f, 1.0f));

                    ImGui::Button(splitterId.c_str(), ImVec2(-1.0f, splitterThickness));

                    if(ImGui::IsItemActive()){
                        float delta = ImGui::GetIO().MouseDelta.y;
                        heights[si]   += delta;
                        heights[si+1] -= delta;
                        heights[si]   = max(heights[si],   minPaneHeight);
                        heights[si+1] = max(heights[si+1], minPaneHeight);
                    }
                    if(ImGui::IsItemHovered() || ImGui::IsItemActive()){
                        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                    }

                    ImGui::PopStyleColor(3);
                }
            }
        }
    }
	ImGui::Separator();

	if(node->getNodeModel().getDescription() != ""){
		ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 21.0f);
		ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.75,0.75,.75,1.0));
		bool descOpen = ImGui::TreeNode("Description");
		ImGui::PopStyleColor(); // always pop header colour right after TreeNode
		if(descOpen){
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.6,0.6,.6,1.0));
			ImGui::TextWrapped(node->getNodeModel().getDescription().c_str(), "%s");
			ImGui::PopStyleColor();
			ImGui::TreePop();
		}
		ImGui::PopStyleVar();
	}



    ImGui::PopStyleVar();
}
