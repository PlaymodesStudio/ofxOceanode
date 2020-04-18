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
    dragModulesInitialPoint = glm::vec2(NAN, NAN);
    selecting = false;
    
    parentID = _pid;
    if(parentID != "Canvas" && parentID != ""){ //It is not canvas, and it is not the first branch of macros
        uniqueID = parentID + " / " + _uid; //Apend Parent to name;
    }else{
        uniqueID = _uid;
    }
    container->setCanvasID(uniqueID);
    
    scrolling = glm::vec2(0,0);
}

void ofxOceanodeCanvas::draw(){
//    if(selecting){
//        ofPushStyle();
//        ofSetColor(255, 255, 255, 40);
//        ofDrawRectangle(ofRectangle(canvasToScreen(selectInitialPoint), canvasToScreen(selectEndPoint)));
//        ofPopStyle();
//    }
//    else if(selectedRect != ofRectangle(0,0,0,0)){
//        ofPushStyle();
//        if(entireSelect)
//            ofSetColor(255, 80, 0, 40);
//        else
//            ofSetColor(0, 80, 255, 40);
//        ofDrawRectangle(ofRectangle(canvasToScreen(selectedRect.getTopLeft()), canvasToScreen(selectedRect.getBottomRight())));
//        ofPopStyle();
//    }
    
    //Draw Guis
//    gui.begin();
    
    // Draw a list of nodes on the left side
    bool open_context_menu = false;
    string node_hovered_in_list = "";
    string node_hovered_in_scene = "";
//    ImGui::BeginChild("node_list", ImVec2(100, 0));
//    ImGui::Text("Nodes");
//    ImGui::Separator();
//    for (int node_idx = 0; node_idx < nodes.Size; node_idx++)
//    {
//        Node* node = &nodes[node_idx];
//        ImGui::PushID(node->ID);
//        if (ImGui::Selectable(node->Name, node->ID == node_selected))
//            node_selected = node->ID;
//        if (ImGui::IsItemHovered())
//        {
//            node_hovered_in_list = node->ID;
//            open_context_menu |= ImGui::IsMouseClicked(1);
//        }
//        ImGui::PopID();
//    }
//    ImGui::EndChild();
    
    ImGui::SetNextWindowDockID(ofxOceanodeShared::getDockspaceID(), ImGuiCond_FirstUseEver);

    ImGui::Begin(uniqueID.c_str());
    ImGui::SameLine();
    ImGui::BeginGroup();
    
    const float NODE_SLOT_RADIUS = 4.0f;
    const ImVec2 NODE_WINDOW_PADDING(8.0f, 7.0f);
    
    // Create our child canvas
    ImGui::Text("[%.2f,%.2f]", scrolling.x, scrolling.y);
    
    ImGui::SameLine();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
    if(ImGui::Button("[C]"))
    {
        scrolling.x = 0;
        scrolling.y = 0;
    }
    ImGui::PopStyleVar();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(60, 60, 60, 200));
    ImGui::BeginChild("scrolling_region", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PushItemWidth(120.0f);
    
    ImVec2 offset = ImGui::GetCursorScreenPos() + scrolling;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    // Display grid
    if (show_grid)
    {
        ImU32 GRID_COLOR = IM_COL32(90, 90, 90, 40);
        float GRID_SZ = 64.0f;
        ImVec2 win_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_sz = ImGui::GetWindowSize();
        for (float x = fmodf(scrolling.x, GRID_SZ); x < canvas_sz.x; x += GRID_SZ)
            draw_list->AddLine(ImVec2(x, 0.0f) + win_pos, ImVec2(x, canvas_sz.y) + win_pos, GRID_COLOR);
        for (float y = fmodf(scrolling.y, GRID_SZ); y < canvas_sz.y; y += GRID_SZ)
            draw_list->AddLine(ImVec2(0.0f, y) + win_pos, ImVec2(canvas_sz.x, y) + win_pos, GRID_COLOR);
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
    draw_list->ChannelsSplit(2);
    draw_list->ChannelsSetCurrent(0); // Background
    for(auto &connection : container->getAllConnections()){
        glm::vec2 p1 = getSourceConnectionPositionFromParameter(connection->getSourceParameter()) + glm::vec2(NODE_WINDOW_PADDING.x, 0);
        glm::vec2 p2 = getSinkConnectionPositionFromParameter(connection->getSinkParameter()) - glm::vec2(NODE_WINDOW_PADDING.x, 0);
        glm::vec2  controlPoint(0,0);
        controlPoint.x = ofMap(glm::distance(p1,p2),0,1500,25,400);
        draw_list->AddBezierCurve(p1, p1 + controlPoint, p2 - controlPoint, p2, IM_COL32(200, 200, 200, 128), 2.0f);
    }
//    if(container->isOpenConnection()){
//        glm::vec2 p1 = getSourceConnectionPositionFromParameter(container->getTemporalConnectionParameter()) + glm::vec2(NODE_WINDOW_PADDING.x, 0);
//        glm::vec2 p2 = ImGui::GetMousePos();
//        glm::vec2  controlPoint(0,0);
//        controlPoint.x = ofMap(glm::distance(p1,p2),0,1500,25,400);
//        draw_list->AddBezierCurve(p1, p1 + controlPoint, p2 - controlPoint, p2, IM_COL32(255, 255, 255, 128), 1.0f);
//    }
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
    
    // Display nodes
    //Iterating over the map gives errors as we are removing elements from the map during the iteration.
    vector<pair<string, ofxOceanodeNode*>> nodesInThisFrame = vector<pair<string, ofxOceanodeNode*>>(container->getParameterGroupNodesMap().begin(), container->getParameterGroupNodesMap().end());
    for(auto nodePair : nodesInThisFrame)
    {
        auto node = nodePair.second;
        auto &nodeGui = node->getNodeGui();
        string nodeId = node->getParameters()->getName();
        ImGui::PushID(nodeId.c_str());
        
        glm::vec2 node_rect_min = offset + nodeGui.getPosition();
        // Display node contents first
        draw_list->ChannelsSetCurrent(1); // Foreground
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
            draw_list->ChannelsSetCurrent(0); // Background
            ImGui::SetCursorScreenPos(node_rect_min);
            ImGui::InvisibleButton("node", size);
            
            if (ImGui::IsItemHovered())
            {
                node_hovered_in_scene = nodeId;
                open_context_menu |= ImGui::IsMouseClicked(1);
            }
            bool node_moving_active = ImGui::IsItemActive();
            
            ImU32 node_bg_color = /*(node_hovered_in_list == node->ID || node_hovered_in_scene == node->ID || (node_hovered_in_list == -1 && node_selected == node->ID)) ? IM_COL32(75, 75, 75, 255) :*/ IM_COL32(40, 40, 40, 255);
            ImU32 node_hd_color = IM_COL32(node->getColor().r,node->getColor().g,node->getColor().b,64);
            
            
            if(nodeGui.getExpanded()){
                draw_list->AddRectFilled(node_rect_min, node_rect_max, node_bg_color, 4.0f);
            }
            draw_list->AddRectFilled(node_rect_min, node_rect_header, node_hd_color, 4.0f);
            
            //draw_list->AddRect(node_rect_min, node_rect_max, IM_COL32(0, 0, 0, 255), 4.0f);
            
            if (ImGui::IsItemHovered() && ImGui::IsMouseDown(0) && ImGui::GetIO().KeySuper && !isNodeDuplicated){
                nodeGui.duplicate();
                //TODO: Change focus to new duplicated node
                isNodeDuplicated = true;
            }
            
            int NODE_BULLET_MIN_SIZE = 3;
            int NODE_BULLET_MAX_SIZE = 10;
            int NODE_BULLET_GROW_DIST = 10;
            
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
            if(!isCreatingConnection){
                if (node_widgets_active || node_moving_active)
                    node_selected = nodeId;
                if (node_moving_active && ImGui::IsMouseDragging(0))
                    nodeGui.setPosition(nodeGui.getPosition() + ImGui::GetIO().MouseDelta);
            }
        }
        
        //Delete duplicate module?
//        if(ImGui::IsItemHovered() && ImGui::IsMouseClicked(1)){
//            ImGui::OpenPopup("Context Menu");
//        }
//        // Draw menu
//        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
//        if (ImGui::BeginPopup("Context Menu"))
//        {
//            Node* node = node_selected != -1 ? &nodes[node_selected] : NULL;
//            ImVec2 scene_pos = ImGui::GetMousePosOnOpeningCurrentPopup() - offset;
//            if (node)
//            {
//                ImGui::Text("Node '%s'", node->Name);
//                ImGui::Separator();
//                if (ImGui::MenuItem("Rename..", NULL, false, false)) {}
//                if (ImGui::MenuItem("Delete", NULL, false, false)) {}
//                if (ImGui::MenuItem("Copy", NULL, false, false)) {}
//            }
//            else
//            {
//                if (ImGui::MenuItem("Add")) { nodes.push_back(Node(nodes.Size, "New node", scene_pos, 0.5f, ImColor(100, 100, 200), 2, 2)); }
//                if (ImGui::MenuItem("Paste", NULL, false, false)) {}
//            }
//            ImGui::EndPopup();
//        }
//        ImGui::PopStyleVar();
        
        ImGui::PopID();
    }
    
    if(!ofGetKeyPressed(OF_KEY_COMMAND)){
        isNodeDuplicated = false;
    }
    
    draw_list->ChannelsMerge();
    
    
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
    if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(0, 0.0f) && !isCreatingConnection){
        scrolling = scrolling + ImGui::GetIO().MouseDelta;
    }
    
    if (isCreatingConnection && !ImGui::IsMouseDown(0)){
        //Destroy temporal connection
        tempSourceParameter = nullptr;
        tempSinkParameter = nullptr;
        isCreatingConnection = false;
    }
    
    ImGui::PopItemWidth();
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    ImGui::EndGroup();
    
    ImGui::End();
    
    //TODO: Find better way to to this, when macro created, recoverr focus on canvas, should be its parent. something like. container->getParentCanvas? Or set a id in canvas as "Parent Canvas".
    if(isFirstDraw && parentID != ""){
        ImGui::FocusWindow(ImGui::FindWindowByName(parentID.c_str()));
        isFirstDraw = false;
    }
    
    ofPushMatrix();
    ofMultMatrix(glm::inverse(transformationMatrix->get()));
//    gui.draw();
    ofPopMatrix();
}

//void ofxOceanodeCanvas::searchListener(ofxDatGuiTextInputEvent e){
//    searchedOptions.clear();
//    if(e.text != ""){
//        for(auto drop : modulesSelectors){
//            int numMathes = 0;
//            for(int i = 0; i < drop->getNumOptions(); i++){
//                auto item = drop->getChildAt(i);
//                string lowercaseName = item->getName();
//                std::transform(lowercaseName.begin(), lowercaseName.end(), lowercaseName.begin(), ::tolower);
//                if(ofStringTimesInString(item->getName(), e.text) || ofStringTimesInString(lowercaseName, e.text)){
//                    searchedOptions.push_back(make_pair(drop, i));
//                    numMathes++;
//                }else{
//                    item->setVisible(false);
//                }
//            }
//            if(numMathes == 0){
//                for(int i = 0; i < drop->getNumOptions(); i++){
//                    drop->getChildAt(i)->setVisible(true);
//                }
//                drop->collapse();
//            }else{
//                drop->expand();
//            }
//        }
//        if(searchedOptions.size() == 0){
//            for(auto drop : modulesSelectors){
//                drop->collapse();
//                for(int i = 0; i < drop->getNumOptions(); i++){
//                    drop->getChildAt(i)->setVisible(true);
//                }
//            }
//        }
//    }else{
//        for(auto drop : modulesSelectors){
//            drop->setLabel(drop->getName());
//            drop->collapse();
//            for(int i = 0; i < drop->getNumOptions(); i++){
//                drop->getChildAt(i)->setVisible(true);
//            }
//        }
//    }
//}

void ofxOceanodeCanvas::keyPressed(ofKeyEventArgs &e){
//    if(searchField->ofxDatGuiComponent::getFocused()){
//        if(e.key == OF_KEY_RETURN){
//            if(searchedOptions.size() != 0){
//                ofxDatGuiDropdownEvent e(searchedOptions[0].first, 0, searchedOptions[0].second);
//                newModuleListener(e);
//            }
//        }
//    }else{
        if(e.key == ' '){
            //glfwSetCursor((GLFWwindow*)ofGetWindowPtr()->getWindowContext(), openedHandCursor);
            if(ofGetMousePressed()) dragCanvasInitialPoint = glm::vec2(ofGetMouseX(), ofGetMouseY());
//        }else if(e.key == OF_KEY_BACKSPACE){
//            popUpMenu->setVisible(false);
//            searchField->setFocused(false);
//            toMoveNodes.clear();
//            selectedRect = ofRectangle(0,0,0,0);
        }
#ifdef TARGET_OSX
        else if(ofGetKeyPressed(OF_KEY_COMMAND)){
#else
        else if(ofGetKeyPressed(OF_KEY_CONTROL)){
#endif
            if(e.key == 'c' || e.key == 'C'){
                container->copyModulesAndConnectionsInsideRect(selectedRect, entireSelect);
                toMoveNodes.clear();
                selectedRect = ofRectangle(0,0,0,0);
            }else if(e.key == 'v' || e.key == 'V'){
                container->pasteModulesAndConnectionsInPosition(screenToCanvas(glm::vec2(ofGetMouseX(), ofGetMouseY())));
            }else if(e.key == 'x' || e.key == 'X'){
                container->cutModulesAndConnectionsInsideRect(selectedRect, entireSelect);
                toMoveNodes.clear();
                selectedRect = ofRectangle(0,0,0,0);
            }
        }
//    }
}

void ofxOceanodeCanvas::mouseDragged(ofMouseEventArgs &e){
    glm::vec2 transformedPos = screenToCanvas(e);
    if(ofGetKeyPressed(' ')){
        transformationMatrix->set(glm::translate(transformationMatrix->get(), glm::vec3(dragCanvasInitialPoint-e, 0)));
        dragCanvasInitialPoint = e;
    }else if(selecting){
        selectEndPoint = transformedPos;
    }else if(toMoveNodes.size() != 0 && dragModulesInitialPoint == dragModulesInitialPoint){
        for(auto node : toMoveNodes){
            node.first->setPosition(node.second + (transformedPos - dragModulesInitialPoint));
        }
        selectedRect.setPosition(glm::vec3(selectedRectIntialPosition + (transformedPos - dragModulesInitialPoint), 1));
    }
}

void ofxOceanodeCanvas::mousePressed(ofMouseEventArgs &e){
    glm::vec2 transformedPos = screenToCanvas(e);
#ifdef TARGET_OSX
    if(ofGetKeyPressed(OF_KEY_COMMAND)){
#else
    if(ofGetKeyPressed(OF_KEY_CONTROL)){
#endif
        if(e.button == 0){
            //searchField->setText("");
            //searchField->setFocused(true);
            //popUpMenu->setPosition(e.x, e.y);
            //popUpMenu->setVisible(true);
        }
        else if(e.button == 2){
            transformationMatrix->set(glm::mat4(1.0));
        }
    }
    if(ofGetKeyPressed(' ')){
        dragCanvasInitialPoint = e;
    }
    if(ofGetKeyPressed(OF_KEY_ALT)){
        selectInitialPoint = transformedPos;
        selectEndPoint = transformedPos;
        selectedRect = ofRectangle(0,0,0,0);
        selecting = true;
    }
    if(toMoveNodes.size() != 0 && selectedRect.inside(transformedPos)){
        selectedRectIntialPosition = selectedRect.getPosition();
        for(auto &node : toMoveNodes){
            node.second = node.first->getPosition();
        }
        dragModulesInitialPoint = transformedPos;
    }
}

void ofxOceanodeCanvas::mouseReleased(ofMouseEventArgs &e){
    if(selecting){
        selectedRect = ofRectangle(selectInitialPoint, selectEndPoint);
        if(glm::all(glm::greaterThan(selectInitialPoint, selectEndPoint)))
            entireSelect = false;
        else
            entireSelect = true;
        
        toMoveNodes.clear();
        for(auto nodeGui : container->getModulesGuiInRectangle(selectedRect, entireSelect)){
            toMoveNodes.push_back(make_pair(nodeGui, nodeGui->getPosition()));
        }
        if(toMoveNodes.size() == 0){
            selectedRect = ofRectangle(0,0,0,0);
        }
    }
    dragModulesInitialPoint = glm::vec2(NAN, NAN);
    selecting = false;
}

void ofxOceanodeCanvas::mouseScrolled(ofMouseEventArgs &e){
#ifdef TARGET_OSX
    if(ofGetKeyPressed(OF_KEY_COMMAND)){
#else
    if(ofGetKeyPressed(OF_KEY_CONTROL)){
#endif
        float scrollValue = -e.scrollY/100.0;
        transformationMatrix->set(translateMatrixWithoutScale(transformationMatrix->get(), glm::vec3(e, 0) * getMatrixScale(transformationMatrix->get()) * scrollValue));
        transformationMatrix->set(glm::scale(transformationMatrix->get(), glm::vec3(1-(scrollValue), 1-(scrollValue), 1)));
    }else if(ofGetKeyPressed(OF_KEY_ALT)){
        transformationMatrix->set(translateMatrixWithoutScale(transformationMatrix->get(), glm::vec3(e.scrollY*2, 0, 0)));
    }else{
        transformationMatrix->set(translateMatrixWithoutScale(transformationMatrix->get(), glm::vec3(-e.scrollX*2, -e.scrollY*2, 0)));
    }
}

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

#endif
