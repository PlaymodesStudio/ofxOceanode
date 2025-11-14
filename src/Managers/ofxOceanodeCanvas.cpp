//
//  ofxOceanode.cpp
//  example-basic
//
//  Created by Eduard Frigola on 19/06/2017.
//
//

#ifndef OFXOCEANODE_HEADLESS

#include "ofxOceanodeCanvas.h"
#include "ofxOceanodeNodeRegistry.h"
#include "ofxOceanodeContainer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <unordered_set>

#include "imgui.h"
#include "imgui_internal.h"
#include "ofxOceanodeShared.h"
#include "ofxOceanodeParameter.h"

void ofxOceanodeCanvas::setup(string _uid, string _pid){
    transformationMatrix = &container->getTransformationMatrix();
    
    selectedRect = ofRectangle(0, 0, 0, 0);
    isSelecting = false;
    
    parentID = _pid;
    if(parentID != "Canvas" && parentID != ""){ //It is not canvas, and it is not the first branch of macros
        uniqueID = parentID + " / " + _uid; //Apend Parent to name;
    }else{
        uniqueID = _uid;
    }
    container->setCanvasID(uniqueID);
    
    scrolling = glm::vec2(0,0);
    moveSelectedModulesWithDrag = glm::vec2(0,0);
	
	NODE_WINDOW_PADDING = glm::vec2(8.0f,7.0f);
	gridDivisions=4;
	updateGridSize();

}

void ofxOceanodeCanvas::draw(bool *open, ofColor color, string title){
    //Draw Guis
    if(onTop){
        ImGui::SetNextWindowFocus();
        onTop = false;
    }
    // Draw a list of nodes on the left side
    bool open_context_menu = false;
    string node_hovered_in_list = "";
    string node_hovered_in_scene = "";
    
    bool isAnyNodeHovered = false;
    bool connectionIsDoable = false;
    
    ImGui::SetNextWindowDockID(ofxOceanodeShared::getDockspaceID(), ImGuiCond_FirstUseEver);
    string windowName = uniqueID;
    if(title != "") windowName = "(" + title + ") " + windowName;
    if(ImGui::Begin(windowName.c_str(), open)){
        ImGui::SameLine();
        ImGui::BeginGroup();
                
        // Create our child canvas
		offsetToCenter = glm::vec2(int(scrolling.x - (ImGui::GetContentRegionAvail().x/2.0f)), int( scrolling.y - (ImGui::GetContentRegionAvail().y/2.0f))+8);
		
		// CANVAS POSITION
		ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(1.0,1.0,1.0,0.5));
		ImGui::Text("[%d,%d]",int(scrolling.x - (ImGui::GetContentRegionAvail().x/2.0f)), int( scrolling.y - (ImGui::GetContentRegionAvail().y/2.0f))+8);
        ImGui::SameLine();
		ImGui::PopStyleColor();
		
		// FPS
        float fps = ofGetFrameRate();
        if(fps>=60.0) ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0.0,1.0,0.0,0.5));
        else if(fps>30.0) ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(1.0,0.5,0.0,0.5));
        else ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(1.0,0.0,0.0,0.5));
        ImGui::Text("%d fps",int(fps));
        ImGui::PopStyleColor();
        
		// [S]NAP
		// Push appropriate text color for [S] button based on snap_to_grid state
		
		ImGui::SameLine();
		if(snap_to_grid) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.86f, 0.38f, 0.0f, 1.0f)); // Orange when enabled
		}
		else ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0.4,0.4,0.4,1.0));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));

		if(ImGui::Button("[S]"))
		{
			snap_to_grid = !snap_to_grid;
		}
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();

		// [C]ENTER
		
		ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0.4,0.4,0.4,1.0));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));

		ImGui::SameLine();
		bool recenterCanvas = false;
		if(ImGui::Button("[C]") || isFirstDraw)
		{
			recenterCanvas = true;
		}
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();

		// COMMENTS
		
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55,0.55,0.55,1.0));
		ImGui::SetNextItemWidth(90);
		
        int numComments = container->getComments().size();
        
        // Initialize checkbox states if needed
        if(commentCheckboxStates.size() != numComments) {
            // Handle size changes - this should mainly happen during initialization
            // or if comments were added/removed outside the normal deletion flow
            if(commentCheckboxStates.size() > numComments) {
                // More states than comments - clean up excess entries
                for(int i = numComments; i < commentCheckboxStates.size(); i++) {
                    if(i < commentToSlot.size() && commentToSlot[i] != -1) {
                        keyboardSlots[commentToSlot[i]] = -1;
                    }
                }
            }
            
            commentCheckboxStates.resize(numComments, false);
            commentToSlot.resize(numComments, -1);
            
            // Validate existing assignments after resize
            for(int slot = 0; slot < MAX_KEYBOARD_SLOTS; slot++) {
                if(keyboardSlots[slot] >= numComments) {
                    keyboardSlots[slot] = -1;
                }
            }
        }
        
        // Initialize keyboard slots if needed
        if(keyboardSlots.size() != MAX_KEYBOARD_SLOTS) {
            keyboardSlots.resize(MAX_KEYBOARD_SLOTS, -1);
        }
        
        if(numComments > 0)
        {
            ImGui::SameLine();
            
            // Create dropdown for comment navigation
            // Set size constraints to show more items before scrolling (approximately double the default)
            ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(FLT_MAX, ImGui::GetTextLineHeightWithSpacing() * 24));
            if(ImGui::BeginCombo("##CommentDropdown", "Go To..."))
            {
                for(int i = 0; i < numComments; i++)
                {
                    auto &c = container->getComments()[i];
                    
                    // Checkbox for keyboard shortcut assignment
                    bool isChecked = commentCheckboxStates[i];
                    string checkboxId = "##checkbox" + ofToString(i);
                    
                    if(ImGui::Checkbox(checkboxId.c_str(), &isChecked))
                    {
                        if(isChecked && !commentCheckboxStates[i])
                        {
                            // Trying to activate checkbox - check if we have available slots
                            int availableSlot = -1;
                            for(int slot = 0; slot < MAX_KEYBOARD_SLOTS; slot++)
                            {
                                if(keyboardSlots[slot] == -1)
                                {
                                    availableSlot = slot;
                                    break;
                                }
                            }
                            
                            if(availableSlot != -1)
                            {
                                // Assign to available slot
                                commentCheckboxStates[i] = true;
                                keyboardSlots[availableSlot] = i;
                                commentToSlot[i] = availableSlot;
                            }
                            else
                            {
                                // No available slots - revert checkbox
                                isChecked = false;
                            }
                        }
                        else if(!isChecked && commentCheckboxStates[i])
                        {
                            // Deactivating checkbox - free up the slot
                            int slot = commentToSlot[i];
                            if(slot != -1)
                            {
                                keyboardSlots[slot] = -1;
                                commentToSlot[i] = -1;
                            }
                            commentCheckboxStates[i] = false;
                        }
                    }
                    
                    ImGui::SameLine();
                    
                    // Create display text with keyboard shortcut if assigned
                    string displayText;
                    if(commentCheckboxStates[i] && commentToSlot[i] != -1)
                    {
                        int keyNum = (commentToSlot[i] + 1) % 10; // 0 becomes 10, others stay 1-9
                        if(keyNum == 0) keyNum = 10;
                        displayText = "[" + ofToString(keyNum == 10 ? 0 : keyNum) + "] " + c.text;
                    }
                    else
                    {
                        displayText = c.text;
                    }
                    
                    // Apply comment color to the item
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(c.color.r, c.color.g, c.color.b, 1.0));
                    
                    if(ImGui::Selectable(displayText.c_str()))
                    {
                        // Navigate to comment position
                        scrolling.x = -c.position.x;
                        scrolling.y = -c.position.y;
                    }
                    
                    ImGui::PopStyleColor();
                }
                ImGui::EndCombo();
            }
        }
        
        // Keyboard shortcut functionality for numeric keys 1-0 using slot-based system
        for(int slot = 0; slot < MAX_KEYBOARD_SLOTS; slot++)
        {
            int commentIndex = keyboardSlots[slot];
            if(commentIndex != -1 && commentIndex < numComments)
            {
                // Map slot to key: slot 0-8 -> keys 1-9, slot 9 -> key 0
                int keyCode = (slot < 9) ? ('1' + slot) : '0';
                
                if(!ImGui::IsAnyItemActive() && ImGui::IsKeyDown(ImGuiKey(keyCode)))
                {
                    auto &c = container->getComments()[commentIndex];
                    scrolling.x = -c.position.x;
                    scrolling.y = -c.position.y;
                }
            }
        }
        
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();
		        
		// MAIN CANVAS
		
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(color.r/4, color.g/4, color.b/4, 200));
        
		ImGui::BeginChild("scrolling_region", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PushItemWidth(120.0f);
        
        ImVec2 offset = ImGui::GetCursorScreenPos() + scrolling;
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        
		// COMMENT CREATION WITH ALT MOUSE
		
        bool selectionHasBeenMade = false;
        bool newComment = false;
        if(ImGui::IsMouseReleased(0)){
            if(isSelecting){
                isSelecting = false;
                if(ImGui::GetIO().KeyAlt){
                    newComment = true;
                }
                    selectionHasBeenMade = true;
            }
        }
        
        // Display grid
        if (show_grid)
        {
            ImU32 GRID_COLOR = IM_COL32(90, 90, 90, 40);
            ImU32 GRID_COLOR_CENTER = IM_COL32(30, 30, 30, 80);
            ImVec2 win_pos = ImGui::GetCursorScreenPos();
            ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
            
            if (recenterCanvas )
            {
                if(!ImGui::GetIO().KeyShift)
                {
                    scrolling.x = ImGui::GetContentRegionAvail().x/2.0f;
                    scrolling.y = ImGui::GetContentRegionAvail().y/2.0f;
                }
                else
                {
                    vector<ofxOceanodeNode*> allNodes = container->getAllModules();
                    glm::vec2 centerOfMass = glm::vec2(0.0f,0.0f);
                    glm::vec2 currentNodeCenter;
                    glm::vec2 currentNodeRectangle;
                    for(int i=0;i<allNodes.size();i++)
                    {
                        currentNodeRectangle =  glm::vec2(allNodes[i]->getNodeGui().getRectangle().width,allNodes[i]->getNodeGui().getRectangle().height);
                        currentNodeCenter = allNodes[i]->getNodeGui().getPosition() + currentNodeRectangle/2.0f  ;
                        centerOfMass = centerOfMass + currentNodeCenter;
                    }
                    scrolling.x = (ImGui::GetContentRegionAvail().x/2.0f) - (centerOfMass.x/allNodes.size());
                    scrolling.y = (ImGui::GetContentRegionAvail().y/2.0f) - (centerOfMass.y/allNodes.size());
                }
            }

            for (float x = fmodf(scrolling.x, GRID_SIZE); x < canvas_sz.x; x += GRID_SIZE)
                draw_list->AddLine(ImVec2(x, 0.0f) + win_pos, ImVec2(x, canvas_sz.y) + win_pos, GRID_COLOR);
            for (float y = fmodf(scrolling.y, GRID_SIZE); y < canvas_sz.y; y += GRID_SIZE)
                draw_list->AddLine(ImVec2(0.0f, y) + win_pos, ImVec2(canvas_sz.x, y) + win_pos, GRID_COLOR);
            
            ImVec2 origin = ImVec2(win_pos.x + scrolling.x, win_pos.y + scrolling.y );
            draw_list->AddLine(ImVec2(origin.x,0),ImVec2(origin.x,origin.x +  canvas_sz.y/2),GRID_COLOR_CENTER,2);
            draw_list->AddLine(ImVec2(0,origin.y),ImVec2(origin.x +  canvas_sz.x,origin.y),GRID_COLOR_CENTER,2);
        }
		
		vector<pair<string, ofxOceanodeNode*>> nodesInThisFrame = vector<pair<string, ofxOceanodeNode*>>(container->getParameterGroupNodesMap().begin(), container->getParameterGroupNodesMap().end());
		
		//Look for deleted Nodes in drawing nodes order map
        // Create a set of valid node IDs for quick lookup
        std::unordered_set<std::string> validNodeIds;
        for (const auto& node : nodesInThisFrame) {
            validNodeIds.insert(node.first);
        }
        
        // Temporary vector to store valid nodes with their updated indices
        std::vector<std::pair<std::string, int>> updatedNodes;
        updatedNodes.reserve(nodesDrawingOrder.size());
        
        // Filter out invalid nodes and prepare for reordering
        for (const auto& [nodeId, index] : nodesDrawingOrder) {
            if (validNodeIds.count(nodeId)) {
                updatedNodes.emplace_back(nodeId, index);
            }
        }
        
        // Rebuild the map and update indices
        nodesDrawingOrder.clear();
        for (int newIndex = 0; newIndex < updatedNodes.size(); ++newIndex) {
            nodesDrawingOrder[updatedNodes[newIndex].first] = newIndex;
        }
        
		
		
		//Draw List layers
		draw_list->ChannelsSplit(max(nodesInThisFrame.size(), (size_t)2)*2 + 1 + 1); //We have foreground + background of each node + connections + comments on the background
		
		for(auto &n : nodesInThisFrame){
			if(nodesDrawingOrder.count(n.first) == 0){
				nodesDrawingOrder[n.first] = nodesDrawingOrder.size();
			}
		}
        
        vector<pair<string, ofxOceanodeNode*>> nodesVisibleInThisFrame;
        
        for(auto nodePair : nodesInThisFrame)
        {
            auto node = nodePair.second;
            auto &nodeGui = node->getNodeGui();
            string nodeId = nodePair.first;
            
            bool visibleNode = true;
            if(ofxOceanodeShared::getConfigurationFlags() & ofxOceanodeConfigurationFlags_DisableRenderAll){
                if(!ofRectangle(ImGui::GetWindowPos() - offset, ImGui::GetWindowWidth(), ImGui::GetWindowHeight()).intersects(nodeGui.getRectangle()) && nodeGui.getRectangle().getWidth() > 0){
                    if(!nodeGui.getSelected())
                        visibleNode = false;
                }
            }
            if(visibleNode){
                nodesVisibleInThisFrame.push_back(nodePair);
                nodeGui.setVisibility(true);
            }else{
                nodeGui.setVisibility(false);
            }
        }
        
        //reorder nodesInThisFrame, so they are in correct drawing order, for the interaction to work properly
        std::sort(nodesVisibleInThisFrame.begin(), nodesVisibleInThisFrame.end(), [this](std::pair<std::string, ofxOceanodeNode*> a, std::pair<std::string, ofxOceanodeNode*> b){
//            if (nodesDrawingOrder.count(a.first) == 0) nodesDrawingOrder[a.first] = nodesDrawingOrder.size();
//            if (nodesDrawingOrder.count(b.first) == 0) nodesDrawingOrder[b.first] = nodesDrawingOrder.size();
            return nodesDrawingOrder[a.first] > nodesDrawingOrder[b.first];
        });
		
        std::unordered_set<std::string> deletedIds;
        // Display nodes
        //Iterating over the map gives errors as we are removing elements from the map during the iteration.
        for(auto nodePair : nodesVisibleInThisFrame)
        {
            auto node = nodePair.second;
            auto &nodeGui = node->getNodeGui();
            string nodeId = nodePair.first;
            
            ImGui::PushID(nodeId.c_str());
			
			int nodeDrawChannel = 1;
			if(nodesDrawingOrder.count(nodeId) != 0){
				nodeDrawChannel = (nodesDrawingOrder[nodeId] * 2) + 2; //We have two channels per node, and we have to shift 2 position to draw on top of connections and comments
			}else{
				nodesDrawingOrder[nodeId] = nodesDrawingOrder.size();
			}
            
            glm::vec2 node_rect_min = offset + nodeGui.getPosition();
            // Display node contents first
            draw_list->ChannelsSetCurrent(nodeDrawChannel+1); // Foreground
            bool old_any_active = ImGui::IsAnyItemActive();
            ImGui::SetCursorScreenPos(node_rect_min + NODE_WINDOW_PADDING);
            
            
            //Draw Parameters
			if(nodeGui.constructGui(NODE_WIDTH_TEXT,NODE_WIDTH_WIDGET)){
                
                // Save the size of what we have emitted and whether any of the widgets are being used
                bool node_widgets_active = (!old_any_active && ImGui::IsAnyItemActive());
                glm::vec2 size = ImGui::GetItemRectSize() + NODE_WINDOW_PADDING + NODE_WINDOW_PADDING;
                ImVec2 node_rect_max = node_rect_min + size;
                ImVec2 node_rect_header = node_rect_min + ImVec2(size.x,29);
                
                nodeGui.setSize(size);
                
                // Display node box
                draw_list->ChannelsSetCurrent(nodeDrawChannel); // Background
                ImGui::SetCursorScreenPos(node_rect_min);
                bool interacting_node = ImGui::IsItemActive();
                bool connectionCanBeInteracted = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
                ImGui::InvisibleButton("node", size);
                
                if (ImGui::IsItemHovered())
                {
                    isAnyNodeHovered = true;
                    node_hovered_in_scene = nodeId;
                    open_context_menu |= ImGui::IsMouseClicked(1);
                }
                bool node_moving_active = ImGui::IsItemActive();
                
                auto getIsSelectedByRect = [this](ofRectangle rect) -> bool{
                    if(entireSelect){
                        return selectedRect.inside(rect);
                    }else{
                        return selectedRect.intersects(rect);
                    }
                };
                
                bool toCheckPress = nodeGui.getSelected() && lastSelectedNode != nodeId ? ImGui::IsMouseReleased(0) : ImGui::IsMouseClicked(0);
                
                if(toCheckPress && ImGui::IsItemHovered(ImGuiHoveredFlags_None)){
                    if(nodeGui.getSelected() && lastSelectedNode == nodeId) lastSelectedNode = "";
                    if(!someDragAppliedToSelection || !nodeGui.getSelected()){
                        if(!ImGui::GetIO().KeyShift){
                            deselectAllNodes();
                            nodeGui.setSelected(true);
							
							//We reorder the list do that this selected node goes to the top layer;
							int rearangeFrom = nodesDrawingOrder[nodeId];
							nodesDrawingOrder[nodeId] = nodesDrawingOrder.size();
							for_each(nodesDrawingOrder.begin(), nodesDrawingOrder.end(), [rearangeFrom](std::pair<const string, int> &orderPair){
								if(orderPair.second > rearangeFrom) orderPair.second--;
							});
                        }else{
                            nodeGui.setSelected(!nodeGui.getSelected());
                        }
                        if(nodeGui.getSelected()) lastSelectedNode = nodeId;
                    }
                }
                if(ImGui::IsMouseReleased(0) && ImGui::IsItemHovered(ImGuiHoveredFlags_None) && nodeGui.getSelected() && lastSelectedNode == nodeId){
                    lastSelectedNode = "";
                }
                
                //MultiSelect
                if(selectionHasBeenMade){
                    bool fitSelection = getIsSelectedByRect(nodeGui.getRectangle());
                    if(!ImGui::GetIO().KeyShift)
                        nodeGui.setSelected(fitSelection);
                    else if(fitSelection)
                        nodeGui.setSelected(true);
                }
                
                bool isSelectedOrSelecting = nodeGui.getSelected() || (isSelecting && getIsSelectedByRect(nodeGui.getRectangle()));
                
                //            ImU32 node_bg_color = /*(node_hovered_in_list == node->ID || node_hovered_in_scene == node->ID || (node_hovered_in_list == -1 && node_selected == node->ID)) ? IM_COL32(75, 75, 75, 255) :*/ IM_COL32(40, 40, 40, 255);
                ImU32 node_bg_color = IM_COL32(40, 40, 40, 255);
                
                ImU32 node_hd_color = (isSelectedOrSelecting) ? IM_COL32(node->getColor().r,node->getColor().g,node->getColor().b,160) : IM_COL32(node->getColor().r,node->getColor().g,node->getColor().b,64);
                
				// Check if this node should be rendered transparently
				bool isTransparent = (node->getNodeModel().getFlags() & ofxOceanodeNodeModelFlags_TransparentNode);

				if (!isTransparent) {
					//if(nodeGui.getExpanded()){
						draw_list->AddRectFilled(node_rect_min, node_rect_max, node_bg_color, 4.0f);
					//}
					draw_list->AddRectFilled(node_rect_min, node_rect_header, node_hd_color, 4.0f);
				}
                
                //draw_list->AddRect(node_rect_min, node_rect_max, IM_COL32(0, 0, 0, 255), 4.0f);
                
                if(nodeGui.getExpanded()){
                    int NODE_BULLET_MIN_SIZE = 3;
                    int NODE_BULLET_MAX_SIZE = 10;
                    int NODE_BULLET_GROW_DIST = 10;
                    
                    for (auto &absParam : node->getParameters()){
                        auto param = dynamic_pointer_cast<ofxOceanodeAbstractParameter>(absParam);
                        if(!(param->getFlags() & ofxOceanodeParameterFlags_DisableInConnection)){
                            auto bulletPosition = nodeGui.getSinkConnectionPositionFromParameter(*param) - glm::vec2(NODE_WINDOW_PADDING.x, 0);
                            auto mouseToBulletDistance = glm::distance(glm::vec2(ImGui::GetMousePos()), bulletPosition);
                            auto bulletSize = ofMap(mouseToBulletDistance, 0, NODE_BULLET_GROW_DIST, NODE_BULLET_MAX_SIZE, NODE_BULLET_MIN_SIZE, true);
                            draw_list->AddCircleFilled(bulletPosition, bulletSize, IM_COL32(0, 0, 0, 255));
                            if(mouseToBulletDistance < NODE_BULLET_MAX_SIZE && !ImGui::IsPopupOpen("New Node") && connectionCanBeInteracted){
                                connectionIsDoable = true;
                                if(ImGui::IsMouseClicked(0)){
                                    nodeGui.setSelected(false); //Deselect node if we are making connections
                                    isCreatingConnection = true;
                                    if(param->hasInConnection()){ //Parmaeter has sink connected
                                        auto inConnection = param->getInConnection();
                                        tempSourceParameter = &inConnection->getSourceParameter();
                                        if(!ImGui::GetIO().KeyAlt){
                                            inConnection->deleteSelf();
                                        }
                                    }else{
                                        tempSinkParameter = param.get();
                                    }
                                }else if(ImGui::IsMouseReleased(0) && isCreatingConnection){
                                    isCreatingConnection = false;
                                    if(tempSinkParameter != nullptr){
                                        tempSinkParameter = nullptr;
                                        ofLog() << "Cannot create a conection from Sink to Sink";
                                    }
                                    else if(tempSourceParameter != nullptr){
                                        if(tempSourceParameter == param.get()){ //Is the same parameter, no conection between them
                                             ofLog() << "Cannot create connection with same parameter";
                                        }
                                        else{
                                            bool hasInverseConnection = false;
                                            if(param->hasOutConnections()){
                                                for(auto c : param->getOutConnections()){
                                                    if(&c->getSinkParameter() == tempSourceParameter){
                                                        hasInverseConnection = true;
                                                        break;
                                                    }
                                                }
                                            }
                                            if(hasInverseConnection){
                                                ofLog() << "Cannot create connection Already connected the other way arround";
                                            }
                                            else{
                                                //Remove previous connection connected to that parameter.
                                                if(param->hasInConnection()){
                                                    param->getInConnection()->deleteSelf();
                                                }
                                                container->createConnection(*tempSourceParameter, *param);
                                            }
                                        }
                                        tempSourceParameter = nullptr;
                                    }
                                }
                            }
                        }
                    }
                    for (auto &absParam : node->getParameters()){
                        auto param = dynamic_pointer_cast<ofxOceanodeAbstractParameter>(absParam);
                        if(!(param->getFlags() & ofxOceanodeParameterFlags_DisableOutConnection)){
                            auto bulletPosition = nodeGui.getSourceConnectionPositionFromParameter(*param) + glm::vec2(NODE_WINDOW_PADDING.x, 0);
                            auto mouseToBulletDistance = glm::distance(glm::vec2(ImGui::GetMousePos()), bulletPosition);
                            auto bulletSize = ofMap(mouseToBulletDistance, 0, NODE_BULLET_GROW_DIST, NODE_BULLET_MAX_SIZE, NODE_BULLET_MIN_SIZE, true);
                            draw_list->AddCircleFilled(bulletPosition, bulletSize, IM_COL32(0, 0, 0, 255));
                            if(mouseToBulletDistance < NODE_BULLET_MAX_SIZE && !ImGui::IsPopupOpen("New Node") && connectionCanBeInteracted){
                                if(ImGui::IsMouseClicked(0)){
                                    nodeGui.setSelected(false); //Deselect node if we are making connections
                                    isCreatingConnection = true;
                                    tempSourceParameter = param.get();
                                }else if(ImGui::IsMouseReleased(0) && isCreatingConnection){
                                    isCreatingConnection = false;
                                    if(tempSourceParameter != nullptr){
                                        tempSourceParameter = nullptr;
                                        ofLog() << "Cannot create a conection from Source to Source";
                                    }
                                    if(tempSinkParameter != nullptr){
                                        if(tempSinkParameter == param.get()){ //Is the same parameter, no conection between them
                                             ofLog() << "Cannot create connection with same parameter";
                                        }
                                        else{
                                            bool hasInverseConnection = false;
                                            if(param->hasInConnection()){
                                                hasInverseConnection = &param->getInConnection()->getSourceParameter() == tempSinkParameter;
                                            }
                                            if(hasInverseConnection){
                                                ofLog() << "Cannot create connection Already connected the other way arround";
                                            }
                                            else{
                                                container->createConnection(*param, *tempSinkParameter);
                                            }
                                        }
                                        tempSinkParameter = nullptr;
                                    }
                                }
                            }
                        }
                    }
                }
                if(someSelectedModuleMove == nodeId) someSelectedModuleMove = "";
                if(!isCreatingConnection){
                    if (node_widgets_active || node_moving_active)
                        node_selected = nodeId;
                    if (node_moving_active && ImGui::IsMouseDragging(0, 0.0f) && !node_widgets_active)
                        someSelectedModuleMove = nodeId;
                    if(someSelectedModuleMove != "" && nodeGui.getSelected()) {
                        glm::vec2 newPosition = nodeGui.getPosition() + moveSelectedModulesWithDrag;
                        // During dragging, allow free movement without snapping
                        nodeGui.setPosition(newPosition);
                    }
                }
            }
            else{
                deletedIds.insert(nodeId);
            }
            ImGui::PopID();
		}
        
        if(newComment){
            auto &comment = container->getComments().emplace_back(selectedRect.position, glm::vec2(selectedRect.width, selectedRect.height));
            deselectAllNodes();
        }

        draw_list->ChannelsSetCurrent(0);
        int removeIndex = -1;
        for(int i = 0; i < container->getComments().size(); i++){
            auto &c = container->getComments()[i];
            ImGui::PushID(("Comment " + ofToString(i)).c_str());
            
            glm::vec2 currentPosition = c.position + offset;
            draw_list->AddRectFilled(currentPosition, currentPosition + glm::vec2(c.size.x, 15), IM_COL32(c.color.r*255, c.color.g*255, c.color.b*255, 255));
            draw_list->AddText(currentPosition, IM_COL32(c.textColor.r*255, c.textColor.g*255, c.textColor.b*255, 255), c.text.c_str());
            draw_list->AddRectFilled(currentPosition + glm::vec2(0, 15), currentPosition + c.size, IM_COL32(c.color.r*255, c.color.g*255, c.color.b*255, 100));
            ImGui::SetCursorScreenPos(currentPosition);
			// trying to avoid a crash on ImGui::InvisibleButton
			// IM_ASSERT(size_arg.x != 0.0f && size_arg.y != 0.0f);
			// if size of x or y is 0 -> crash !
			// This could happen if accidentally creating a "comment" with click+option without dragging = size = 0
			if(c.size.x==0 || c.size.y==0)
			{
				c.size.x = 256;
				c.size.y = 15;
			}
            ImGui::InvisibleButton("Inv Button", ImVec2(c.size.x, 15));
            
            if(ImGui::IsItemActive()){
                ofRectangle rect(c.position, c.size.x, c.size.y);
                c.position = c.position + ImGui::GetIO().MouseDelta;
                if(!ImGui::GetIO().KeyAlt){
                    if(c.nodes.size() == 0){
                        for(auto nodePair : nodesInThisFrame)
                        {
                            if(rect.inside(nodePair.second->getNodeGui().getRectangle())){
                                c.nodes.push_back(nodePair.second);
                            }
                        }
                    }
                    for(auto n : c.nodes){
                        n->getNodeGui().setPosition(n->getNodeGui().getPosition() + ImGui::GetIO().MouseDelta);
                    }
                }else{
                    c.nodes.clear();
                }
            }else{
                c.nodes.clear();
                c.nodes.clear();
            }
            
            if(ImGui::IsItemClicked(1)){
                c.openPopupInNext = true;
            }
            
            if(c.openPopupInNext){
                ImGui::OpenPopup("Comment");
                c.openPopupInNext = false;
            }
			ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));  // Dark background
			ImGui::PushStyleColor(ImGuiCol_Text,     ImVec4(0.7f, 0.7f, 0.7f, 0.7f));  // White text
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f)); // Hovered button
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

            if(ImGui::BeginPopup("Comment")){
                char * cString = new char[1024];
                strcpy(cString, c.text.c_str());
                if (ImGui::InputText("Text", cString, 1024, ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    c.text = cString;
                }
                delete[] cString;
                ImGui::DragFloat2("Position", &c.position.x);
                ImGui::DragFloat2("Size", &c.size.x);
                ImGui::ColorEdit3("Color", &c.color.r);
                ImGui::ColorEdit3("TextColor", &c.textColor.r);
                if(ImGui::Button("[Remove]")){
                    removeIndex = i;
                }
                ImGui::EndPopup();
            }
			ImGui::PopStyleColor(3);
			ImGui::PopStyleVar();

            ImGui::PopID();
        }
        if(removeIndex != -1){
            // Before deleting the comment, handle keyboard slot cleanup properly
            // to maintain stable assignments for remaining comments
            
            // 1. If the deleted comment had a keyboard slot, free it
            if(removeIndex < commentCheckboxStates.size() &&
               commentCheckboxStates[removeIndex] &&
               removeIndex < commentToSlot.size() &&
               commentToSlot[removeIndex] != -1) {
                int slotToFree = commentToSlot[removeIndex];
                keyboardSlots[slotToFree] = -1;
            }
            
            // 2. Update all keyboard slot assignments for comments that will shift indices
            // Comments with index > removeIndex will shift down by 1
            for(int slot = 0; slot < MAX_KEYBOARD_SLOTS; slot++) {
                if(keyboardSlots[slot] > removeIndex) {
                    keyboardSlots[slot]--; // Decrement the comment index in the slot
                }
            }
            
            // 3. Now delete the comment
            container->getComments().erase(container->getComments().begin() + removeIndex);
            
            // 4. Update the tracking vectors to match the new comment count
            // Remove the deleted comment's entries and shift remaining ones
            if(removeIndex < commentCheckboxStates.size()) {
                commentCheckboxStates.erase(commentCheckboxStates.begin() + removeIndex);
            }
            if(removeIndex < commentToSlot.size()) {
                commentToSlot.erase(commentToSlot.begin() + removeIndex);
            }
            
            // 5. Update commentToSlot mappings for comments that shifted indices
            for(int i = removeIndex; i < commentToSlot.size(); i++) {
                if(commentToSlot[i] != -1) {
                    // This comment's index shifted down by 1, but its slot assignment stays the same
                    // The keyboardSlots array was already updated in step 2
                    // commentToSlot[i] already contains the correct slot number
                }
            }
        }
        
        
        
        // Open context menu
        if (!ImGui::IsAnyItemHovered() && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(1))
        {
			newNodeClickPos = ImGui::GetMousePos();
			bool commentClicked = false;
			for(auto &c : container->getComments()){
				if(ofRectangle(c.position.x, c.position.y, c.size.x, 15).inside(newNodeClickPos-offset)){
					c.openPopupInNext = true;
					commentClicked = true;
				}
				if(commentClicked) break;
			}
			if(!commentClicked){
				ImGui::OpenPopup("New Node");
				searchField = "";
				lastSearchField = "";
				numTimesPopup = 0;
				selectedSearchResultIndex = -1;
				filteredSearchResults.clear();
				
				//Get node registry to update newly registered nodes
				auto const &models = container->getRegistry()->getRegisteredModels();
				auto const &categories = container->getRegistry()->getCategories();
				auto const &categoriesModelsAssociation = container->getRegistry()->getRegisteredModelsCategoryAssociation();
				
				categoriesVector = vector<string>(categories.begin(), categories.end());
				
				options = vector<vector<string>>(categories.size());
				for(int i = 0; i < categories.size(); i++){
					options.push_back(vector<string>());
					for(auto &model : models){
						if(categoriesModelsAssociation.at(model.first) == categoriesVector[i]){
							options[i].push_back(model.first);
						}
					}
					std::sort(options[i].begin(), options[i].end());
				}
			}
        }
        
        // Draw New Node menu
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
        bool popop_close_button = true;
        if (ImGui::BeginPopupModal("New Node", &popop_close_button, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize))
        {
            char * cString = new char[256];
            strcpy(cString, searchField.c_str());
            
            if(ImGui::InputText("Search", cString, 256)){
                searchField = cString;
                // Reset selection when search text changes
                if(searchField != lastSearchField) {
                    selectedSearchResultIndex = -1;
                    lastSearchField = searchField;
                }
            }
            
            if(numTimesPopup == 1){//!(!ImGui::IsItemClicked() && ImGui::IsMouseDown(0)) && searchField == ""){
                ImGui::SetKeyboardFocusHere(-1);
                numTimesPopup++;
            }else{
                numTimesPopup++;
            }
            
            bool isEnterPressed = ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Enter)); //Select first option if enter is pressed
            bool isEnterReleased = ImGui::IsKeyReleased(ImGui::GetKeyIndex(ImGuiKey_Enter)); //Select first option if enter is pressed
            
            // Handle arrow key navigation
            bool isUpPressed = ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow));
            bool isDownPressed = ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow));
   
   // TODO: Get all things, nodes, collections, macros, scripts;
    
            if(searchField != ""){
                // Build filtered search results list with scoring
                filteredSearchResults.clear();
                
                // Create a scoring function for search relevance
                auto calculateSearchScore = [](const string& itemName, const string& searchTerm) -> int {
                    if(itemName.empty() || searchTerm.empty()) return 0;
                    
                    string lowerItemName = itemName;
                    string lowerSearchTerm = searchTerm;
                    std::transform(lowerItemName.begin(), lowerItemName.end(), lowerItemName.begin(), ::tolower);
                    std::transform(lowerSearchTerm.begin(), lowerSearchTerm.end(), lowerSearchTerm.begin(), ::tolower);
                    
                    // Exact match (case-insensitive) gets highest score
                    if(lowerItemName == lowerSearchTerm) {
                        return 1000;
                    }
                    
                    // Prefix match gets high score
                    if(lowerItemName.find(lowerSearchTerm) == 0) {
                        // Shorter names with prefix matches get higher scores
                        return 800 - (int)lowerItemName.length();
                    }
                    
                    // Substring match gets lower score
                    size_t pos = lowerItemName.find(lowerSearchTerm);
                    if(pos != string::npos) {
                        // Earlier position in string gets higher score
                        // Shorter names get higher scores
                        return 400 - (int)pos - (int)lowerItemName.length();
                    }
                    
                    return 0; // No match
                };
                
                // Collect scored results
                vector<pair<SearchResultItem, int>> scoredResults;
                
                // Add matching nodes with scores
                for(int i = 0; i < categoriesVector.size(); i++){
                    for(auto &op : options[i])
                    {
                        int score = calculateSearchScore(op, searchField);
                        if(score > 0){
                            scoredResults.push_back(make_pair(SearchResultItem(op, "node"), score));
                        }
                    }
                }
                
                // Add matching macros with scores
                std::function<void(shared_ptr<macroCategory>)> collectMacros =
    [this, &collectMacros, &scoredResults, &calculateSearchScore](shared_ptr<macroCategory> category){
                    for(auto d : category->categories){
                        collectMacros(d);
                    }
                    for(auto m : category->macros){
                        int score = calculateSearchScore(m.first, searchField);
                        if(score > 0){
                            scoredResults.push_back(make_pair(SearchResultItem(m.first, "macro", m.second), score));
                        }
                    }
                };
                
                auto macroDirectoryStructure = ofxOceanodeShared::getMacroDirectoryStructure();
                collectMacros(macroDirectoryStructure);
                
                // Sort by score (highest first)
                std::sort(scoredResults.begin(), scoredResults.end(),
                    [](const pair<SearchResultItem, int>& a, const pair<SearchResultItem, int>& b) {
                        return a.second > b.second;
                    });
                
                // Extract sorted results
                for(const auto& scoredResult : scoredResults) {
                    filteredSearchResults.push_back(scoredResult.first);
                }
                
                // Handle arrow key navigation
                if(isUpPressed && selectedSearchResultIndex > 0) {
                    selectedSearchResultIndex--;
                } else if(isDownPressed && selectedSearchResultIndex < (int)filteredSearchResults.size() - 1) {
                    selectedSearchResultIndex++;
                } else if((isUpPressed || isDownPressed) && selectedSearchResultIndex == -1 && !filteredSearchResults.empty()) {
                    selectedSearchResultIndex = isDownPressed ? 0 : filteredSearchResults.size() - 1;
                }
                
                // Clamp selection index to valid range
                if(selectedSearchResultIndex >= (int)filteredSearchResults.size()) {
                    selectedSearchResultIndex = filteredSearchResults.size() - 1;
                }
                if(selectedSearchResultIndex < -1) {
                    selectedSearchResultIndex = -1;
                }
                
                // Display search results
                for(int i = 0; i < filteredSearchResults.size(); i++) {
                    const auto& result = filteredSearchResults[i];
                    bool isSelected = (i == selectedSearchResultIndex);
                    
                    // Highlight selected item
                    if(isSelected) {
                        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.43f, 0.19f, 0.0f, 0.8f));
                        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.86f,0.38f, 0.0f, 0.6f));
                        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.8f, 0.3f, 0.0f, 0.6f));
                    }
                    
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(0.6f, 0.6f, 0.6f,1.0f)));
                    
                    bool itemClicked = ImGui::Selectable(result.name.c_str(), isSelected);
                    bool itemActivated = itemClicked || (isSelected && isEnterPressed);
                    
                    if(itemClicked) {
                        selectedSearchResultIndex = i;
                    }
                    
                    if(itemActivated) {
                        if(result.type == "node") {
                            unique_ptr<ofxOceanodeNodeModel> type = container->getRegistry()->create(result.name);
                            if (type) {
                                auto &node = container->createNode(std::move(type));
                                node.getNodeGui().setPosition(newNodeClickPos - offset);
                            }
                        } else if(result.type == "macro") {
                            unique_ptr<ofxOceanodeNodeModel> type = container->getRegistry()->create("Macro");
                            if (type) {
                                auto &node = container->createNode(std::move(type), result.macroPath);
                                node.getNodeGui().setPosition(newNodeClickPos - offset);
                            }
                        }
                        ImGui::PopStyleColor();
                        if(isSelected) {
                            ImGui::PopStyleColor(3);
                        }
                        ImGui::CloseCurrentPopup();
                        isEnterPressed = false;
                        break;
                    }
                    
                    ImGui::PopStyleColor();
                    if(isSelected) {
                        ImGui::PopStyleColor(3);
                    }
                }
                
                // Handle Enter key when no specific item is selected (fallback to first result)
                if(isEnterPressed && selectedSearchResultIndex == -1 && !filteredSearchResults.empty()) {
                    const auto& firstResult = filteredSearchResults[0];
                    if(firstResult.type == "node") {
                        unique_ptr<ofxOceanodeNodeModel> type = container->getRegistry()->create(firstResult.name);
                        if (type) {
                            auto &node = container->createNode(std::move(type));
                            node.getNodeGui().setPosition(newNodeClickPos - offset);
                        }
                    } else if(firstResult.type == "macro") {
                        unique_ptr<ofxOceanodeNodeModel> type = container->getRegistry()->create("Macro");
                        if (type) {
                            auto &node = container->createNode(std::move(type), firstResult.macroPath);
                            node.getNodeGui().setPosition(newNodeClickPos - offset);
                        }
                    }
                    ImGui::CloseCurrentPopup();
                }
            }
                
            
            
            ImGui::Separator();
            
            if(ImGui::BeginMenu("Modules")){
                bool selectedModule = false;
                for(int i = 0; i < categoriesVector.size() && !selectedModule; i++){
                    vector<string> subcategoriesSplit = ofSplitString(categoriesVector[i], "/");
                    int openedMenus = 0;
                    for(auto subCategory : subcategoriesSplit){
                        if(ImGui::BeginMenu(subCategory.c_str())){
                            openedMenus++;
                        }else{
                            break;
                        }
                    }
                    if(openedMenus == subcategoriesSplit.size())
                    {
                        for(auto &op : options[i])
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(0.65f, 0.65f, 0.65f,1.0f)));
                            if( ImGui::MenuItem(op.c_str()) || (ImGui::IsItemFocused() && isEnterPressed))
                            {
                                unique_ptr<ofxOceanodeNodeModel> type = container->getRegistry()->create(op);
                                if (type)
                                {
                                    auto &node = container->createNode(std::move(type));
                                    node.getNodeGui().setPosition(newNodeClickPos - offset);
                                }
                                ImGui::PopStyleColor();
                                selectedModule = true;
                                break;
                            }
                            ImGui::PopStyleColor();
                        }
                    }
                    for(int i = 0; i < openedMenus; i++){
                        ImGui::EndMenu();
                    }
                }
                ImGui::EndMenu();
                if(selectedModule) ImGui::CloseCurrentPopup();
            }
            
            //TODO: Implement
            if(ImGui::BeginMenu("Module Groups")){
                ImGui::MenuItem("Example 1");
                ImGui::MenuItem("Example 2");
                ImGui::MenuItem("Example 3");
                ImGui::EndMenu();
            }
            
            if(ImGui::BeginMenu("Macros")){
				auto macroDirectoryStructure = ofxOceanodeShared::getMacroDirectoryStructure();
				
				std::function<void(shared_ptr<macroCategory>)> drawCategory =
				[this, offset, &drawCategory](shared_ptr<macroCategory> category){
					for(auto d : category->categories){
						if(ImGui::BeginMenu(d->name.c_str())){
							drawCategory(d);
							ImGui::EndMenu();
						}
					}
					for(auto m : category->macros){
						if(ImGui::MenuItem(m.first.c_str())){
							unique_ptr<ofxOceanodeNodeModel> type = container->getRegistry()->create("Macro");
							if (type)
							{
								auto &node = container->createNode(std::move(type), m.second);
								node.getNodeGui().setPosition(newNodeClickPos - offset);
							}
						}
					}
				};
				
				drawCategory(macroDirectoryStructure);
				
                ImGui::EndMenu();
            }
            
            //TODO: How to add scripts?
            if(ImGui::BeginMenu("Scripts")){
                ImGui::MenuItem("Example 1");
                ImGui::MenuItem("Example 2");
                ImGui::MenuItem("Example 3");
                ImGui::EndMenu();
            }
			
			if(ImGui::Selectable("Comment")){
				container->getComments().emplace_back(newNodeClickPos - offset);
			}
            
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();
        //ImGui::PopStyleColor();
        
        auto getSourceConnectionPositionFromParameter = [this, offset](ofxOceanodeAbstractParameter& param) -> glm::vec2{
            auto gui = container->getGuiFromModel(param.getNodeModel());
            if(gui != nullptr){
                if(gui->getVisibility()){
                    return gui->getSourceConnectionPositionFromParameter(param);
                }else{
                    return gui->getPosition() + offset + glm::vec2(gui->getRectangle().getWidth(), gui->getRectangle().getHeight()/2);
                }
            }
            return glm::vec2();
            //TODO: Throw exception
        };
        auto getSinkConnectionPositionFromParameter = [this, offset](ofxOceanodeAbstractParameter& param) -> glm::vec2{
            auto gui = container->getGuiFromModel(param.getNodeModel());
            if(gui != nullptr){
                if(gui->getVisibility()){
                    return gui->getSinkConnectionPositionFromParameter(param);
                }else{
                    return gui->getPosition() + offset + glm::vec2(0, gui->getRectangle().getHeight()/2);
                }
            }
            return glm::vec2();
            //TODO: Throw exception
        };
        
        // Display links
        draw_list->ChannelsSetCurrent(1); // Background
        
        std::vector<ofxOceanodeAbstractConnection*> drawnConnections;
        
        for(auto nodePair : nodesVisibleInThisFrame)
        {
            if(deletedIds.count(nodePair.first) != 0) continue;
            for (auto &absParam : nodePair.second->getParameters()){
                auto param = dynamic_pointer_cast<ofxOceanodeAbstractParameter>(absParam);
                auto connections = param->getOutConnections();
                if(param->getInConnection() != nullptr) connections.push_back(param->getInConnection());
                for(auto connection : connections){
                    if(std::find(drawnConnections.begin(), drawnConnections.end(), connection) == drawnConnections.end()){
                        glm::vec2 p1 = getSourceConnectionPositionFromParameter(connection->getSourceParameter()) + glm::vec2(NODE_WINDOW_PADDING.x, 0);
                        glm::vec2 p2 = getSinkConnectionPositionFromParameter(connection->getSinkParameter()) - glm::vec2(NODE_WINDOW_PADDING.x, 0);
                        glm::vec2  controlPoint(0,0);
                        controlPoint.x = ofMap(glm::distance(p1,p2),0,1500,25,400);
                        draw_list->AddBezierCubic(p1, p1 + controlPoint, p2 - controlPoint, p2, IM_COL32(200, 200, 200, 128), 2.0f);
                        drawnConnections.push_back(connection);
                    }
                }

            }
        }
        if(tempSourceParameter != nullptr || tempSinkParameter != nullptr){
            glm::vec2 p1, p2;
            if(tempSourceParameter != nullptr){
                p1 = getSourceConnectionPositionFromParameter(*tempSourceParameter) + glm::vec2(NODE_WINDOW_PADDING.x, 0);
                p2 = ImGui::GetMousePos();
            }else{
                p1 = ImGui::GetMousePos();
                p2 = getSinkConnectionPositionFromParameter(*tempSinkParameter) - glm::vec2(NODE_WINDOW_PADDING.x, 0);
            }
            glm::vec2  controlPoint(0,0);
            controlPoint.x = ofMap(glm::distance(p1,p2),0,1500,25,400);
            float linkWidth = 2.0f;
            ImColor c;
            if(connectionIsDoable){
                linkWidth = 3.0f;
                c = IM_COL32(255, 255, 255, 128);
            }else{
                c = IM_COL32(255, 255, 255, 64);
            }
            draw_list->AddBezierCubic(p1, p1 + controlPoint, p2 - controlPoint, p2, c, linkWidth);
        }
        
        draw_list->ChannelsMerge();
        
		moveSelectedModulesWithDrag = glm::vec2(0,0);
        // Scrolling
        if(ImGui::IsWindowFocused()){
            if(ImGui::IsMouseDragging(0, 0.0f)){
                if (ImGui::IsWindowHovered()){
#ifdef TARGET_OSX
                    if(ImGui::GetIO().KeySuper && !isCreatingConnection){//MultiSelect not allowed when connecting connectio
#else
                    if(ImGui::GetIO().KeyCtrl && !isCreatingConnection){
#endif
                        if(!isSelecting){
                            selectInitialPoint = ImGui::GetMousePos() - ImGui::GetIO().MouseDelta - offset;
                            isSelecting  = true;
                        }
                        selectEndPoint = ImGui::GetMousePos() - offset;
                        selectedRect = ofRectangle(selectInitialPoint, selectEndPoint);
                        entireSelect =  selectInitialPoint.y < selectEndPoint.y;
                        canvasHasScolled = true; //HACK to not remove selection on mouse release
                    }
                    if((!isSelecting && !isCreatingConnection && someSelectedModuleMove == "") || (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Space)))){
                        scrolling = scrolling + ImGui::GetIO().MouseDelta;
                        if(glm::vec2(ImGui::GetIO().MouseDelta) != glm::vec2(0,0)) canvasHasScolled = true;
#ifdef TARGET_OSX
                        if(isSelecting && !ImGui::GetIO().KeySuper){
#else
                        if(isSelecting && !ImGui::GetIO().KeyCtrl){
#endif
                            selectInitialPoint = selectInitialPoint +  ImGui::GetIO().MouseDelta;
                            selectEndPoint = selectEndPoint + ImGui::GetIO().MouseDelta;
                            selectedRect = ofRectangle(selectInitialPoint, selectEndPoint);
                            entireSelect = glm::vec2(selectedRect.getTopLeft()) == selectInitialPoint;
                        }
                    }
				}else if(ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)){
					if(ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Space))){
						scrolling = scrolling + ImGui::GetIO().MouseDelta;
                        if(glm::vec2(ImGui::GetIO().MouseDelta) != glm::vec2(0,0)) canvasHasScolled = true;
					}
					else if(someSelectedModuleMove != ""){
						moveSelectedModulesWithDrag = ImGui::GetIO().MouseDelta;
						if(moveSelectedModulesWithDrag != glm::vec2(0,0))
							someDragAppliedToSelection = true;
					}
				}
            }
            
            //TODO: Scroll amount in config
            if(ImGui::IsWindowHovered()){
                scrolling = scrolling + glm::vec2(ImGui::GetIO().MouseWheelH * 10, ImGui::GetIO().MouseWheel * 10);
            }
            
            if(isSelecting){
                //TODO: Change colors
                if(selectInitialPoint.y < selectEndPoint.y){ //From top to bottom;
                    draw_list->AddRectFilled(selectInitialPoint + offset, selectEndPoint + offset, IM_COL32(255,127,0,30));
                }else{
                    draw_list->AddRectFilled(selectInitialPoint + offset, selectEndPoint + offset, IM_COL32(0,125,255,30));
                }
            }
            
            
            if(ImGui::IsMouseReleased(0)){
                if(canvasHasScolled){
                    canvasHasScolled = false;
                }else if(!isAnyNodeHovered && !someDragAppliedToSelection){
                    deselectAllNodes();
                }
                
                // Apply snap-to-grid when drag operation ends
                if(snap_to_grid && someDragAppliedToSelection){
                    // Find and snap all selected nodes to grid
                    for(auto &n : container->getParameterGroupNodesMap()){
                        if(n.second->getNodeGui().getSelected()){
                            n.second->getNodeGui().setPosition(snapToGrid(n.second->getNodeGui().getPosition()));
                        }
                    }
                }
                
                someDragAppliedToSelection = false;
                moveSelectedModulesWithDrag = glm::vec2(0,0);
            }
            
#ifdef TARGET_OSX
            if(ImGui::GetIO().KeySuper){
#else
            if(ImGui::GetIO().KeyCtrl){
#endif
                if(ImGui::IsKeyPressed((ImGuiKey)'C')){
                    container->copySelectedModulesWithConnections();
                    deselectAllNodes();
                }else if(ImGui::IsKeyPressed((ImGuiKey)'V') && !ImGui::IsAnyItemActive()){
                    deselectAllNodes();
                    container->pasteModulesAndConnectionsInPosition(ImGui::GetMousePos() - offset, ImGui::GetIO().KeyShift);
                }else if(ImGui::IsKeyPressed((ImGuiKey)'X')){
                    container->cutSelectedModulesWithConnections();
                }else if(ImGui::IsKeyPressed((ImGuiKey)'D')){
                    container->copySelectedModulesWithConnections();
                    deselectAllNodes();
                    container->pasteModulesAndConnectionsInPosition(ImGui::GetMousePos() - offset, ImGui::GetIO().KeyShift);
                }else if(ImGui::IsKeyPressed((ImGuiKey)'A')){
                    selectAllNodes();
                }
            }
            else if(ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Backspace)) && !ImGui::IsAnyItemActive()){
                container->deleteSelectedModules();
            }
            
            if (isCreatingConnection && !ImGui::IsMouseDown(0)){
                //Destroy temporal connection
                tempSourceParameter = nullptr;
                tempSinkParameter = nullptr;
                isCreatingConnection = false;
            }
        }
        
        ImGui::PopItemWidth();
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        ImGui::EndGroup();
        
                }else{
                    deselectAllNodes();
                }
    ImGui::End();
    
    //TODO: Find better way to to this, when macro created, recoverr focus on canvas, should be its parent. something like. container->getParentCanvas? Or set a id in canvas as "Parent Canvas".
    if(isFirstDraw && parentID != ""){
        ImGui::FocusWindow(ImGui::FindWindowByName(parentID.c_str()));
        
    }
    isFirstDraw = false;
    ofPushMatrix();
    ofMultMatrix(glm::inverse(transformationMatrix->get()));
    ofPopMatrix();
}

//void ofxOceanodeCanvas::mouseScrolled(ofMouseEventArgs &e){
//#ifdef TARGET_OSX
//    if(ofGetKeyPressed(OF_KEY_COMMAND)){
//#else
//    if(ofGetKeyPressed(OF_KEY_CONTROL)){
//#endif
//        float scrollValue = -e.scrollY/100.0;
//        transformationMatrix->set(translateMatrixWithoutScale(transformationMatrix->get(), glm::vec3(e, 0) * getMatrixScale(transformationMatrix->get()) * scrollValue));
//        transformationMatrix->set(glm::scale(transformationMatrix->get(), glm::vec3(1-(scrollValue), 1-(scrollValue), 1)));
//    }else if(ofGetKeyPressed(OF_KEY_ALT)){
//        transformationMatrix->set(translateMatrixWithoutScale(transformationMatrix->get(), glm::vec3(e.scrollY*2, 0, 0)));
//    }else{
//        transformationMatrix->set(translateMatrixWithoutScale(transformationMatrix->get(), glm::vec3(-e.scrollX*2, -e.scrollY*2, 0)));
//    }
//}

glm::vec2 ofxOceanodeCanvas::screenToCanvas(glm::vec2 p){
    glm::vec4 result = transformationMatrix->get() * glm::vec4(p, 0, 1);
    return result;
}

glm::vec2 ofxOceanodeCanvas::canvasToScreen(glm::vec2 p){
    glm::vec4 result = glm::inverse(transformationMatrix->get()) * glm::vec4(p, 0, 1);
    return result;
}

glm::vec2 ofxOceanodeCanvas::snapToGrid(glm::vec2 position){
    if(!snap_to_grid) return position;
	
	// Snap to nearest grid point
	float snappedX = round(position.x / GRID_SIZE) * GRID_SIZE;
	float snappedY = round(position.y / GRID_SIZE) * GRID_SIZE;
    
    return glm::vec2(snappedX, snappedY);
}

glm::vec3 ofxOceanodeCanvas::getMatrixScale(const glm::mat4 &m){
    glm::vec3 scale;
    glm::quat rotation;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(m, scale, rotation, translation, skew, perspective);
    return scale;
}

glm::mat4 ofxOceanodeCanvas::translateMatrixWithoutScale(const glm::mat4 &m, glm::vec3 translationVector){
    return glm::translate(glm::mat4(1.0), translationVector) * m;
}

void ofxOceanodeCanvas::deselectAllNodes(){
    for(auto &n: container->getParameterGroupNodesMap()){
        n.second->getNodeGui().setSelected(false);
    }
}

void ofxOceanodeCanvas::selectAllNodes(){
    for(auto &n: container->getParameterGroupNodesMap()){
        n.second->getNodeGui().setSelected(true);
    }
}

#endif
