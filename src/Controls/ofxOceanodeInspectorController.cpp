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
#include "ofxOceanodeNodeRegistry.h"
#include "ofxOceanodeShared.h"
#include "imgui.h"
#include "ofxOceanodeColors.h"

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

bool ofxOceanodeInspectorController::hasAnySelectedNode(){
    vector<pair<string, ofxOceanodeNode*>> nodes(container->getParameterGroupNodesMap().begin(), container->getParameterGroupNodesMap().end());
    std::function<bool(vector<pair<string, ofxOceanodeNode*>>)> checkSelected = [&checkSelected](vector<pair<string, ofxOceanodeNode*>> nodes) -> bool {
        for(auto &nodePair : nodes){
            if(nodePair.second->getNodeGui().getSelected()){
                return true;
            }
            if(ofxOceanodeNodeMacro* m = dynamic_cast<ofxOceanodeNodeMacro*>(&nodePair.second->getNodeModel())){
                if(checkSelected(vector<pair<string, ofxOceanodeNode*>>(m->getContainer()->getParameterGroupNodesMap().begin(), m->getContainer()->getParameterGroupNodesMap().end()))){
                    return true;
                }
            }
        }
        return false;
    };
    return checkSelected(nodes);
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
    auto* owningContainer = container->getContainerForCanvasID(node->getNodeModel().getParents());
    if(owningContainer == nullptr) owningContainer = node->getNodeModel().getHostContainer();

    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 0.0f);

	// Node name with colored background
	{
		ImVec2 textSize = ImGui::CalcTextSize(selectedNodes[0].first.c_str());
		float padding = ImGui::GetStyle().FramePadding.y;
		ImVec2 cursorPos = ImGui::GetCursorScreenPos();
		float availWidth = ImGui::GetContentRegionAvail().x;
		
		// Draw colored background rectangle
		ImGui::GetWindowDrawList()->AddRectFilled(
			cursorPos,
			ImVec2(cursorPos.x + availWidth, cursorPos.y + textSize.y + padding * 2),
			ImGui::ColorConvertFloat4ToU32(node->getColor()*.5)
		);
		
		// Draw white text on top
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + padding);
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text));
		ImGui::Text("%s", selectedNodes[0].first.c_str());
		ImGui::PopStyleColor();
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + padding);
	}

    if(auto* portal = dynamic_cast<abstractPortal*>(&node->getNodeModel())){
        static abstractPortal* editedPortal = nullptr;
        static char nameBuffer[1024] = "";
        static string typeName;
        if(editedPortal != portal){
            editedPortal = portal;
            strncpy(nameBuffer, portal->getName().c_str(), sizeof(nameBuffer) - 1);
            nameBuffer[sizeof(nameBuffer) - 1] = '\0';
            typeName = portal->nodeName();
        }

        ImGui::SeparatorText("Portal instances");
        ImGui::InputText("Name##portal_instances", nameBuffer, sizeof(nameBuffer));
        ImGui::SameLine();
        if(nameBuffer[0] == '\0') ImGui::BeginDisabled();
        if(ImGui::Button("Apply##portal_name")){
            ofxOceanodeShared::replacePortalNameInAllInstances(portal, nameBuffer);
        }
        if(nameBuffer[0] == '\0') ImGui::EndDisabled();

        vector<string> portalTypes;
        for(const auto& model : owningContainer->getRegistry()->getRegisteredModels()){
            if(model.first.rfind("Portal ", 0) == 0) portalTypes.push_back(model.first);
        }
        std::sort(portalTypes.begin(), portalTypes.end());
        const string typeLabel = typeName.rfind("Portal ", 0) == 0 ? typeName.substr(7) : typeName;
        if(ImGui::BeginCombo("Type##portal_instances", typeLabel.c_str())){
            for(const auto& candidate : portalTypes){
                const bool selected = candidate == typeName;
                if(ImGui::Selectable(candidate.substr(7).c_str(), selected)) typeName = candidate;
                if(selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if(typeName == portal->nodeName()) ImGui::BeginDisabled();
        const bool replaceType = ImGui::Button("Apply##portal_type");
        if(typeName == portal->nodeName()) ImGui::EndDisabled();
        if(replaceType){
            editedPortal = nullptr;
            ImGui::PopStyleVar();
            owningContainer->replacePortalTypeInAllInstances(portal, typeName);
            return;
        }
    }

//    if(node->getNodeModel().getDescription() != ""){
//        ImGui::Separator();
//        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
//        ImGui::TextWrapped("%s", node->getNodeModel().getDescription().c_str());
//        ImGui::PopStyleColor();
//    }

	// Inspector Parameters
    ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
    if(ImGui::TreeNode("Inspector Parameters")){
        for(int i = 0; i < node->getInspectorParameters().size(); i++){
            ofAbstractParameter &absParam = node->getInspectorParameters().get(i);
            const bool trackUserValue = absParam.valueType() != typeid(std::function<void()>).name();
            ofJson valueBefore;
            if(trackUserValue) ofSerialize(valueBefore, absParam);
            string uniqueId = absParam.getName();
            ImGui::PushID(uniqueId.c_str());

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
                        float w = ImGui::GetContentRegionAvail().x;
                        
                        // Draw the text
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(color.r/255.0f, color.g/255.0f, color.b/255.0f, color.a/255.0f));
                        ImGui::TextUnformatted(label.c_str());
                        ImGui::PopStyleColor();
                        
                        // Get the size of the rendered text
                        ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
                        
                        // Draw a semi-transparent rectangle behind/below the text (15% opacity to match node gui)
                        ImU32 bgCol = IM_COL32(color.r, color.g, color.b, color.a * 0.15f);
                        ImGui::GetWindowDrawList()->AddRectFilled(
                            ImVec2(p.x, p.y),
                            ImVec2(p.x + w, p.y + textSize.y),
                            bgCol
                        );
                        
                        ImGui::Dummy(ImVec2(0, 2));
                    }
                } else {
                    // Regular custom region function
                    absParam.cast<std::function<void()>>().get()();
                }
            } else {
				string hiddenUniqueId = "##" + uniqueId;
				bool isItemEditableByText = false;

                // PARAM FLOAT
                if(absParam.valueType() == typeid(float).name()){
					ImGui::Text("%s", uniqueId.c_str());
					ImGui::SameLine();

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
					ImGui::Text("%s", uniqueId.c_str());
					ImGui::SameLine();

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
                    if(ImGui::IsItemHovered() && ImGui::IsKeyPressed((ImGuiKey_Space))){
                        tempCast = !tempCast;
                    }
					ImGui::SameLine();
					ImGui::Text("%s", uniqueId.c_str());

                }
                // PARAM VOID
                else if(absParam.valueType() == typeid(void).name()){
                    auto tempCast = absParam.cast<void>();
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Button));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                    if(ImGui::Button(hiddenUniqueId.c_str(), ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()))){
                        tempCast.trigger();
                    }
                    ImGui::PopStyleColor(3);
                    if(ImGui::IsItemHovered() && ImGui::IsKeyPressed((ImGuiKey_Space))){
                        tempCast.trigger();
                    }
					ImGui::SameLine();
					ImGui::Text("%s", uniqueId.c_str());

                }
                // PARAM STRING
                else if(absParam.valueType() == typeid(string).name()){
                    auto tempCast = absParam.cast<string>();
                    string currentText = tempCast.get();
                    const bool macroDescription = uniqueId == "Description" &&
                        dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel()) != nullptr;
                    size_t bufferSize = macroDescription
                        ? max(static_cast<size_t>(16384), currentText.length() + 4096)
                        : max(static_cast<size_t>(1024), currentText.length() + 256);
                    char* cString = new char[bufferSize];
                    strncpy(cString, currentText.c_str(), bufferSize - 1);
                    cString[bufferSize - 1] = '\0';
                    if(macroDescription){
                        ImGui::TextUnformatted(uniqueId.c_str());
                    }
                    const bool changed = macroDescription
                        ? ImGui::InputTextMultiline(hiddenUniqueId.c_str(), cString, bufferSize,
                                                    ImVec2(-1.0f, ImGui::GetTextLineHeight() * 5.0f))
                        : ImGui::InputText(hiddenUniqueId.c_str(), cString, bufferSize,
                                           ImGuiInputTextFlags_EnterReturnsTrue);
                    if(changed){
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
                    if((ImGui::IsItemHovered() && !ImGui::IsItemEdited() && ImGui::IsKeyPressed((ImGuiKey_Enter))) || ImGui::IsItemClicked(1)){
                        ImGui::SetKeyboardFocusHere(-1);
                    }
                }
            }
            if(trackUserValue && owningContainer != nullptr){
                ofJson valueAfter;
                ofSerialize(valueAfter, absParam);
                owningContainer->recordUserEditedValues(*node, valueBefore, valueAfter, true);
            }
            ImGui::PopID();
        }
        ImGui::TreePop();
    }

    ImGui::Separator();

    // Scope - full-width texture previews, square unless aspect ratio is kept
    {
        auto &nodeGui = node->getNodeGui();

        // Collect scope parameter indices
        vector<int> scopeIndices;
        for(int i = 0; i < nodeGui.getParameters().size(); i++){
            ofxOceanodeAbstractParameter &p = static_cast<ofxOceanodeAbstractParameter&>(nodeGui.getParameters().get(i));
            if(p.getFlags() & ofxOceanodeParameterFlags_DisplayMinimized){
                scopeIndices.push_back(i);
            }
        }

        int scopeCount = (int)scopeIndices.size();
        if(scopeCount > 0){
            float scopeHeight = 2.0f * ofxOceanodeShared::getBaseFrameHeight();

            for(int si = 0; si < scopeCount; si++){
                int i = scopeIndices[si];
                ofxOceanodeAbstractParameter &p = static_cast<ofxOceanodeAbstractParameter&>(nodeGui.getParameters().get(i));

                auto size = ImVec2(ImGui::GetContentRegionAvail().x, scopeHeight);
                const bool isTexture = p.valueType() == typeid(ofTexture).name() ||
                                       p.valueType() == typeid(ofTexture*).name();
                if(isTexture){
                    size.y = size.x;
                    if(p.getFlags() & ofxOceanodeParameterFlags_ScopeKeepAspectRatio){
                        const ofTexture* texture = p.valueType() == typeid(ofTexture*).name()
                            ? p.cast<ofTexture*>().getParameter().get()
                            : &p.cast<ofTexture>().getParameter().get();
                        if(texture != nullptr && texture->isAllocated() &&
                           texture->getWidth() > 0.0f && texture->getHeight() > 0.0f){
                            const float aspectRatio = texture->getWidth() / texture->getHeight();
                            size.y = size.x / aspectRatio;
                        }
                    }
                }

                ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(nodeGui.getColor()*0.75f));
                ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(nodeGui.getColor()*0.75f));
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(nodeGui.getColor()*0.75f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                ImGui::PushStyleColor(ImGuiCol_Border, OceanodeColors::TransparentButton);

                // Draw parameter label outside the scope child so the child
                // is filled entirely by the scope visualization.
                ImGui::Text("%s", (p.getGroupHierarchyNames().front() + "/" + p.getName()).c_str());

                // Remove window padding so the scope fills the preview area.
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                ImGui::BeginChild(("Child_" + p.getGroupHierarchyNames().front() + "/" + p.getName()).c_str(), size, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

                ofxOceanodeScope::getInstance()->drawParameter(&p, size);

                ImGui::EndChild();
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(5);
            }
        }
    }
	
	ImGui::Separator();

	if(node->getNodeModel().getDescription() != ""){
		ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 21.0f);
		ImGui::SetNextItemOpen(false, ImGuiCond_Appearing);
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text));
		bool descOpen = ImGui::TreeNode("Description");
		ImGui::PopStyleColor(); // always pop header colour right after TreeNode
		if(descOpen){
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
			ImGui::TextWrapped(node->getNodeModel().getDescription().c_str(), "%s");
			ImGui::PopStyleColor();
			ImGui::TreePop();
		}
		ImGui::PopStyleVar();
	}

    ImGui::PopStyleVar();
}
