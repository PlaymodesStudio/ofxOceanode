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

#include "imgui.h"
#include "imgui_internal.h"
#include "ofxOceanodeShared.h"

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
}

void ofxOceanodeCanvas::draw(bool *open){
    //Draw Guis
    
    // Draw a list of nodes on the left side
    bool open_context_menu = false;
    string node_hovered_in_list = "";
    string node_hovered_in_scene = "";
    
    bool isAnyNodeHovered = false;
    
    
    ImGui::SetNextWindowDockID(ofxOceanodeShared::getDockspaceID(), ImGuiCond_FirstUseEver);
    if(ImGui::Begin(uniqueID.c_str(), open)){
        ImGui::SameLine();
        ImGui::BeginGroup();
        
        const float NODE_SLOT_RADIUS = 4.0f;
        const ImVec2 NODE_WINDOW_PADDING(8.0f, 7.0f);
        
        // Create our child canvas
        
        ImGui::Text("[%d,%d]",int(scrolling.x - (ImGui::GetContentRegionAvail().x/2.0f)), int( scrolling.y - (ImGui::GetContentRegionAvail().y/2.0f))+8);

        ImGui::SameLine(ImGui::GetContentRegionAvail().x-20.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55,0.55,0.55,1.0));

        bool recenterCanvas = false;
        if(ImGui::Button("[C]") || isFirstDraw)
        {
            recenterCanvas = true;
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(60, 60, 60, 200));
        ImGui::BeginChild("scrolling_region", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PushItemWidth(120.0f);
        
        ImVec2 offset = ImGui::GetCursorScreenPos() + scrolling;
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        
        bool selectionHasBeenMade = false;
        if(ImGui::IsMouseReleased(0)){
            if(isSelecting){
                isSelecting = false;
                selectionHasBeenMade = true;
            }else{
                //selectedRect = ofRectangle(0,0,0,0);
            }
        }
        
        // Display grid
        if (show_grid)
        {
            ImU32 GRID_COLOR = IM_COL32(90, 90, 90, 40);
            ImU32 GRID_COLOR_CENTER = IM_COL32(30, 30, 30, 80);
            float GRID_SZ = 64.0f;
            ImVec2 win_pos = ImGui::GetCursorScreenPos();
            ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
            
            if (recenterCanvas )
            {
                scrolling.x = ImGui::GetContentRegionAvail().x/2.0f;
                scrolling.y = ImGui::GetContentRegionAvail().y/2.0f;
            }

            for (float x = fmodf(scrolling.x, GRID_SZ); x < canvas_sz.x; x += GRID_SZ)
                draw_list->AddLine(ImVec2(x, 0.0f) + win_pos, ImVec2(x, canvas_sz.y) + win_pos, GRID_COLOR);
            for (float y = fmodf(scrolling.y, GRID_SZ); y < canvas_sz.y; y += GRID_SZ)
                draw_list->AddLine(ImVec2(0.0f, y) + win_pos, ImVec2(canvas_sz.x, y) + win_pos, GRID_COLOR);
            
            ImVec2 origin = ImVec2(win_pos.x + scrolling.x, win_pos.y + scrolling.y );
            draw_list->AddLine(ImVec2(origin.x,0),ImVec2(origin.x,origin.x +  canvas_sz.y/2),GRID_COLOR_CENTER,2);
            draw_list->AddLine(ImVec2(0,origin.y),ImVec2(origin.x +  canvas_sz.x,origin.y),GRID_COLOR_CENTER,2);
        }
		
		vector<pair<string, ofxOceanodeNode*>> nodesInThisFrame = vector<pair<string, ofxOceanodeNode*>>(container->getParameterGroupNodesMap().begin(), container->getParameterGroupNodesMap().end());
		
		//Look for deleted Nodes in drawing nodes order map
		vector<int> erasedPositions;
		for(auto it = nodesDrawingOrder.begin() ; it != nodesDrawingOrder.end(); ){
			string nodeId = it->first;
			if(find_if(nodesInThisFrame.begin(), nodesInThisFrame.end(), [nodeId](pair<string, ofxOceanodeNode*> &pair){return pair.first == nodeId;}) != nodesInThisFrame.end()){
				++it;
			}else{
				erasedPositions.push_back(it->second);
				it = nodesDrawingOrder.erase(it);
			}
		}
		for(auto &i : erasedPositions){
			//We reorder the list do that this selected node goes to the top layer;
			for_each(nodesDrawingOrder.begin(), nodesDrawingOrder.end(), [i](std::pair<const string, int> &orderPair){
				if(orderPair.second > i) orderPair.second--;
			});
		}
		
		
		//Draw List layers
		draw_list->ChannelsSplit(max(nodesDrawingOrder.size(), (size_t)2)*2 + 1); //We have foreground + background of each node + connections on the background
		
        // Display nodes
        //Iterating over the map gives errors as we are removing elements from the map during the iteration.
        for(auto nodePair : nodesInThisFrame)
        {
            auto node = nodePair.second;
            auto &nodeGui = node->getNodeGui();
            string nodeId = nodePair.first;
            ImGui::PushID(nodeId.c_str());
			
			int nodeDrawChannel = 1;
			if(nodesDrawingOrder.count(nodeId) != 0){
				nodeDrawChannel = (nodesDrawingOrder[nodeId] * 2) + 1; //We have two channels per node, and we have to shift 1 position to get the connecitons channel
			}else{
				nodesDrawingOrder[nodeId] = nodesDrawingOrder.size();
			}
            
            glm::vec2 node_rect_min = offset + nodeGui.getPosition();
            // Display node contents first
            draw_list->ChannelsSetCurrent(nodeDrawChannel+1); // Foreground
            bool old_any_active = ImGui::IsAnyItemActive();
            ImGui::SetCursorScreenPos(node_rect_min + NODE_WINDOW_PADDING);
            
            
            //Draw Parameters
            if(nodeGui.constructGui()){
                
                // Save the size of what we have emitted and whether any of the widgets are being used
                bool node_widgets_active = (!old_any_active && ImGui::IsAnyItemActive());
                glm::vec2 size = ImGui::GetItemRectSize() + NODE_WINDOW_PADDING + NODE_WINDOW_PADDING;
                ImVec2 node_rect_max = node_rect_min + size;
                ImVec2 node_rect_header = node_rect_min + ImVec2(size.x,29);
                
                if(nodeGui.getExpanded()){
                    nodeGui.setSize(size);
                }
                
                // Display node box
                draw_list->ChannelsSetCurrent(nodeDrawChannel); // Background
                ImGui::SetCursorScreenPos(node_rect_min);
                bool interacting_node = ImGui::IsItemActive();
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
                
                if((deselectAllNodesExcept != nodeId && deselectAllNodesExcept != "")){
                    nodeGui.setSelected(false);
                }else{
                    deselectAllNodesExcept = "";
                }
                
                bool toCheckPress = nodeGui.getSelected() && lastSelectedNode != nodeId ? ImGui::IsMouseReleased(0) : ImGui::IsMouseClicked(0);
                
                if(toCheckPress && ImGui::IsItemHovered(ImGuiHoveredFlags_None)){
                    if(nodeGui.getSelected() && lastSelectedNode == nodeId) lastSelectedNode = "";
                    if(!someDragAppliedToSelection || !nodeGui.getSelected()){
                        if(!ImGui::GetIO().KeyShift){
                            deselectAllNodesExcept = nodeId;
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
                
                
                if(nodeGui.getExpanded()){
                    draw_list->AddRectFilled(node_rect_min, node_rect_max, node_bg_color, 4.0f);
                }
                draw_list->AddRectFilled(node_rect_min, node_rect_header, node_hd_color, 4.0f);
                
                //draw_list->AddRect(node_rect_min, node_rect_max, IM_COL32(0, 0, 0, 255), 4.0f);
                
                
                int NODE_BULLET_MIN_SIZE = 3;
                int NODE_BULLET_MAX_SIZE = 10;
                int NODE_BULLET_GROW_DIST = 10;
                
                //TODO: Review press/release detection, better use ImGui::InvisibeButton()?
                for (auto &param : *node->getParameters().get()){
                    //TODO: Check if parameter is plugable
                    auto bulletPosition = nodeGui.getSinkConnectionPositionFromParameter(*param) - glm::vec2(NODE_WINDOW_PADDING.x, 0);
                    //TODO: only grow bulllets that the temporal connection is plugable to.
                    auto mouseToBulletDistance = glm::distance(glm::vec2(ImGui::GetMousePos()), bulletPosition);
                    auto bulletSize = ofMap(mouseToBulletDistance, 0, NODE_BULLET_GROW_DIST, NODE_BULLET_MAX_SIZE, NODE_BULLET_MIN_SIZE, true);
                    draw_list->AddCircleFilled(bulletPosition, bulletSize, IM_COL32(0, 0, 0, 255));
                    if(mouseToBulletDistance < bulletSize && !ImGui::IsPopupOpen("New Node")){
                        if(ImGui::IsMouseClicked(0)){
                            isCreatingConnection = true;
                            auto inConnection = node->getInputConnectionForParameter(*param);
                            if(inConnection != nullptr){ //Parmaeter has sink connected
                                tempSourceParameter = &inConnection->getSourceParameter();
                                if(!ImGui::GetIO().KeyAlt){
                                    container->destroyConnection(inConnection);
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
                                if(tempSourceParameter != param.get()){ //Is the same parameter, no conection between them
                                    //Remove previous connection connected to that parameter.
                                    auto inConnection = node->getInputConnectionForParameter(*param);
                                    if(inConnection != nullptr) container->destroyConnection(inConnection);
                                    container->createConnection(*tempSourceParameter, *param);
                                }
                                else{
                                    ofLog() << "Cannot create connection with same parameter";
                                }
                                tempSourceParameter = nullptr;
                            }
                        }
                    }
                }
                for (auto &param : *node->getParameters().get()){
                    //TODO: Check if parameter is plugable
                    auto bulletPosition = nodeGui.getSourceConnectionPositionFromParameter(*param) + glm::vec2(NODE_WINDOW_PADDING.x, 0);
                    //TODO: only grow bulllets that the temporal connection is plugable to.
                    auto mouseToBulletDistance = glm::distance(glm::vec2(ImGui::GetMousePos()), bulletPosition);
                    auto bulletSize = ofMap(mouseToBulletDistance, 0, NODE_BULLET_GROW_DIST, NODE_BULLET_MAX_SIZE, NODE_BULLET_MIN_SIZE, true);
                    draw_list->AddCircleFilled(bulletPosition, bulletSize, IM_COL32(0, 0, 0, 255));
                    if(mouseToBulletDistance < bulletSize && !ImGui::IsPopupOpen("New Node")){
                        if(ImGui::IsMouseClicked(0)){
                            isCreatingConnection = true;
                            tempSourceParameter = param.get();
                        }else if(ImGui::IsMouseReleased(0) && isCreatingConnection){
                            isCreatingConnection = false;
                            if(tempSourceParameter != nullptr){
                                tempSourceParameter = nullptr;
                                ofLog() << "Cannot create a conection from Source to Source";
                            }
                            if(tempSinkParameter != nullptr){
                                if(tempSinkParameter != param.get()){ //Is the same parameter, no conection between them
                                    container->createConnection(*param, *tempSinkParameter);
                                }
                                else{
                                    ofLog() << "Cannot create connection with same parameter";
                                }
                                tempSinkParameter = nullptr;
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
                    if(someSelectedModuleMove != "" && nodeGui.getSelected())
                        nodeGui.setPosition(nodeGui.getPosition() + moveSelectedModulesWithDrag);
                }
            }
            ImGui::PopID();
		}
        
        
        
        
        // Open context menu
        if (!ImGui::IsAnyItemHovered() && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(1))
        {
            newNodeClickPos = ImGui::GetMousePos();
            ImGui::OpenPopup("New Node");
            searchField = "";
            numTimesPopup = 0;
            
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
        
        // Draw New Node menu
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
        if (ImGui::BeginPopup("New Node"))
        {
            char * cString = new char[256];
            strcpy(cString, searchField.c_str());
            
            if(ImGui::InputText("?", cString, 256)){
                searchField = cString;
            }
            
            if(numTimesPopup == 1){//!(!ImGui::IsItemClicked() && ImGui::IsMouseDown(0)) && searchField == ""){
                ImGui::SetKeyboardFocusHere(-1);
            }
            
            numTimesPopup++;
            
            bool isEnterPressed = ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Enter)); //Select first option if enter is pressed
            for(int i = 0; i < categoriesVector.size(); i++){
                if(numTimesPopup == 1){
                    ImGui::SetNextTreeNodeOpen(false);
                }
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f,0.45f,0.0f,0.5f));
                ImGui::Button("##colorTree",ImVec2(5,0));
                ImGui::PopStyleColor();
                ImGui::SameLine();
                
                if(searchField != "") ImGui::SetNextTreeNodeOpen(true);
                
                if(ImGui::TreeNode(categoriesVector[i].c_str()))
                {
                    for(auto &op : options[i])
                    {
                        bool showThis = false;
                        if(searchField != ""){
                            string lowercaseName = op;
                            std::transform(lowercaseName.begin(), lowercaseName.end(), lowercaseName.begin(), ::tolower);
                            if(ofStringTimesInString(op, searchField) || ofStringTimesInString(lowercaseName, searchField)){
                                showThis = true;
                            }
                        }else{
                            showThis = true;
                        }
                        if(showThis)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f,0.45f,0.0f,0.5f));
                            ImGui::Button("##colorBand",ImVec2(5,0));
                            ImGui::SameLine();
                            ImGui::PopStyleColor();
                            
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(0.65f, 0.65f, 0.65f,1.0f)));
                            
                            if(ImGui::Selectable(op.c_str()) || isEnterPressed)
                            {
                                unique_ptr<ofxOceanodeNodeModel> type = container->getRegistry()->create(op);
                                if (type)
                                {
                                    auto &node = container->createNode(std::move(type));
                                    node.getNodeGui().setPosition(newNodeClickPos - offset);
                                }
                                ImGui::PopStyleColor();
                                ImGui::CloseCurrentPopup();
                                isEnterPressed = false; //Next options we dont want to create them;
                                break;
                            }
                            ImGui::PopStyleColor();
                            
                            
                        }
                    }
                    //ImGui::CloseCurrentPopup();
                    ImGui::TreePop();
                }
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();
        //ImGui::PopStyleColor();
        
        // Scrolling
        if(ImGui::IsMouseDragging(0, 0.0f)){
            if (ImGui::IsWindowHovered()){
                if(ImGui::GetIO().KeySuper && !isCreatingConnection){//MultiSelect not allowed when connecting connectio
                    if(!isSelecting){
                        selectInitialPoint = ImGui::GetMousePos() - ImGui::GetIO().MouseDelta - offset;
                        isSelecting  = true;
                    }
                    selectEndPoint = ImGui::GetMousePos() - offset;
                    selectedRect = ofRectangle(selectInitialPoint, selectEndPoint);
                    entireSelect =  selectInitialPoint.y < selectEndPoint.y;
                    canvasHasScolled = true; //HACK to not remove selection on mouse release
                }
                if((!isSelecting && !isCreatingConnection) || (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Space)))){
                    scrolling = scrolling + ImGui::GetIO().MouseDelta;
                    if(glm::vec2(ImGui::GetIO().MouseDelta) != glm::vec2(0,0)) canvasHasScolled = true;
                    if(isSelecting && !ImGui::GetIO().KeySuper){
                        selectInitialPoint = selectInitialPoint +  ImGui::GetIO().MouseDelta;
                        selectEndPoint = selectEndPoint + ImGui::GetIO().MouseDelta;
                        selectedRect = ofRectangle(selectInitialPoint, selectEndPoint);
                        entireSelect = glm::vec2(selectedRect.getTopLeft()) == selectInitialPoint;
                    }
                }
            }else if(someSelectedModuleMove != "" && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)){
                moveSelectedModulesWithDrag = ImGui::GetIO().MouseDelta;
                if(moveSelectedModulesWithDrag != glm::vec2(0,0))
                    someDragAppliedToSelection = true;
            }
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
            someDragAppliedToSelection = false;
            moveSelectedModulesWithDrag = glm::vec2(0,0);
        }
        
        if(ImGui::GetIO().KeySuper){
            if(ImGui::IsKeyPressed('C')){
                container->copySelectedModulesWithConnections();
                deselectAllNodes();
            }else if(ImGui::IsKeyPressed('V')){
                deselectAllNodes();
                container->pasteModulesAndConnectionsInPosition(ImGui::GetMousePos() - offset, ImGui::GetIO().KeyShift);
            }else if(ImGui::IsKeyPressed('X')){
                container->cutSelectedModulesWithConnections();
            }else if(ImGui::IsKeyPressed('D')){
                container->copySelectedModulesWithConnections();
                deselectAllNodes();
                container->pasteModulesAndConnectionsInPosition(ImGui::GetMousePos() - offset, ImGui::GetIO().KeyShift);
            }else if(ImGui::IsKeyPressed('A')){
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
        
        auto getSourceConnectionPositionFromParameter = [this](ofAbstractParameter& param) -> glm::vec2{
            if(container->getParameterGroupNodesMap().count(param.getGroupHierarchyNames().front())){
                return container->getParameterGroupNodesMap().at(param.getGroupHierarchyNames().front())->getNodeGui().getSourceConnectionPositionFromParameter(param);
            }
            //TODO: Throw exception
        };
        auto getSinkConnectionPositionFromParameter = [this](ofAbstractParameter& param) -> glm::vec2{
            if(container->getParameterGroupNodesMap().count(param.getGroupHierarchyNames().front())){
                return container->getParameterGroupNodesMap().at(param.getGroupHierarchyNames().front())->getNodeGui().getSinkConnectionPositionFromParameter(param);
            }
            //TODO: Throw exception
        };
        
        // Display links
        draw_list->ChannelsSetCurrent(0); // Background
        for(auto &connection : container->getAllConnections()){
            glm::vec2 p1 = getSourceConnectionPositionFromParameter(connection->getSourceParameter()) + glm::vec2(NODE_WINDOW_PADDING.x, 0);
            glm::vec2 p2 = getSinkConnectionPositionFromParameter(connection->getSinkParameter()) - glm::vec2(NODE_WINDOW_PADDING.x, 0);
            glm::vec2  controlPoint(0,0);
            controlPoint.x = ofMap(glm::distance(p1,p2),0,1500,25,400);
            draw_list->AddBezierCurve(p1, p1 + controlPoint, p2 - controlPoint, p2, IM_COL32(200, 200, 200, 128), 2.0f);
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
            draw_list->AddBezierCurve(p1, p1 + controlPoint, p2 - controlPoint, p2, IM_COL32(255, 255, 255, 128), 1.0f);
        }
        
        draw_list->ChannelsMerge();
        
        ImGui::PopItemWidth();
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        ImGui::EndGroup();
        
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
