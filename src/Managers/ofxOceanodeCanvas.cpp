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

void ofxOceanodeCanvas::setup(std::shared_ptr<ofAppBaseWindow> window){    
//    listeners.push(window->events().update.newListener(this, &ofxOceanodeCanvas::update));
//    listeners.push(window->events().draw.newListener(this, &ofxOceanodeCanvas::draw));
    
    listeners.push(window->events().mouseDragged.newListener(this,&ofxOceanodeCanvas::mouseDragged));
    listeners.push(window->events().mouseMoved.newListener(this,&ofxOceanodeCanvas::mouseMoved));
    listeners.push(window->events().mousePressed.newListener(this,&ofxOceanodeCanvas::mousePressed));
    listeners.push(window->events().mouseReleased.newListener(this,&ofxOceanodeCanvas::mouseReleased));
    listeners.push(window->events().mouseScrolled.newListener(this,&ofxOceanodeCanvas::mouseScrolled));
    listeners.push(window->events().mouseEntered.newListener(this,&ofxOceanodeCanvas::mouseEntered));
    listeners.push(window->events().mouseExited.newListener(this,&ofxOceanodeCanvas::mouseExited));
    listeners.push(window->events().keyPressed.newListener(this, &ofxOceanodeCanvas::keyPressed));
    listeners.push(window->events().keyReleased.newListener(this, &ofxOceanodeCanvas::keyReleased));
    
    transformationMatrix = &container->getTransformationMatrix();
    
    auto const &models = container->getRegistry()->getRegisteredModels();
    auto const &categories = container->getRegistry()->getCategories();
    auto const &categoriesModelsAssociation = container->getRegistry()->getRegisteredModelsCategoryAssociation();
    
    for(auto cat : categories){
        categoriesVector.push_back(cat);
    }
    
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
    
    selectedRect = ofRectangle(0, 0, 0, 0);
    dragModulesInitialPoint = glm::vec2(NAN, NAN);
    selecting = false;
    
    uniqueID = "Canvas";
    
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

    ImGui::Begin(uniqueID.c_str());
    ImGui::SameLine();
    ImGui::BeginGroup();
    
    const float NODE_SLOT_RADIUS = 4.0f;
    const ImVec2 NODE_WINDOW_PADDING(8.0f, 8.0f);
    
    // Create our child canvas
    ImGui::Text("[%.2f,%.2f]", scrolling.x, scrolling.y);
    //ImGui::SameLine(ImGui::GetWindowWidth() - 100);
    //ImGui::Checkbox("Show grid", &show_grid);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(60, 60, 60, 200));
    ImGui::BeginChild("scrolling_region", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);
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
        for(auto &node : container->getModulesGuiInRectangle(ofGetWindowRect(), false)){
            if(node->getParameters()->getEscapedName()  == param.getGroupHierarchyNames()[0]){
                return node->getSourceConnectionPositionFromParameter(param);
            }
        }
    };
    auto getSinkConnectionPositionFromParameter = [this](ofAbstractParameter& param) -> glm::vec2{
        for(auto &node : container->getModulesGuiInRectangle(ofGetWindowRect(), false)){
            if(node->getParameters()->getEscapedName() == param.getGroupHierarchyNames()[0]){
                return node->getSinkConnectionPositionFromParameter(param);
            }
        }
    };
    
    // Display links
    draw_list->ChannelsSplit(2);
    draw_list->ChannelsSetCurrent(0); // Background
    for(auto &connection : container->getAllConnections()){
        glm::vec2 p1 = getSourceConnectionPositionFromParameter(connection->getSourceParameter());
        glm::vec2 p2 = getSinkConnectionPositionFromParameter(connection->getSinkParameter());
        glm::vec2  controlPoint(0,0);
        controlPoint.x = ofMap(glm::distance(p1,p2),0,1500,25,400);
        draw_list->AddBezierCurve(p1, p1 + controlPoint, p2 - controlPoint, p2, IM_COL32(200, 200, 200, 128), 2.0f);
    }
    if(container->isOpenConnection()){
        glm::vec2 p1 = getSourceConnectionPositionFromParameter(container->getTemporalConnectionParameter());
        glm::vec2 p2 = ImGui::GetMousePos();
        glm::vec2  controlPoint(0,0);
        controlPoint.x = ofMap(glm::distance(p1,p2),0,1500,25,400);
        draw_list->AddBezierCurve(p1, p1 + controlPoint, p2 - controlPoint, p2, IM_COL32(255, 255, 255, 128), 1.0f);
    }
    
    // Display nodes
    for(auto &node : container->getModulesGuiInRectangle(ofGetWindowRect(), false))
    {
        string nodeId = node->getParameters()->getName();
        ImGui::PushID(nodeId.c_str());
        
        glm::vec2 node_rect_min = offset + node->getPosition();
        // Display node contents first
        draw_list->ChannelsSetCurrent(1); // Foreground
        bool old_any_active = ImGui::IsAnyItemActive();
        ImGui::SetCursorScreenPos(node_rect_min + NODE_WINDOW_PADDING);
        
        //Draw Parameters
        if(node->constructGui()){
            
            // Save the size of what we have emitted and whether any of the widgets are being used
            bool node_widgets_active = (!old_any_active && ImGui::IsAnyItemActive());
            glm::vec2 size = ImGui::GetItemRectSize() + NODE_WINDOW_PADDING + NODE_WINDOW_PADDING;
            ImVec2 node_rect_max = node_rect_min + size;
            ImVec2 node_rect_header = node_rect_min + ImVec2(size.x,23);
            
            node->setSize(size);
            
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
            if (node_widgets_active || node_moving_active)
                node_selected = nodeId;
            if (node_moving_active && ImGui::IsMouseDragging(0))
                node->setPosition(node->getPosition() + ImGui::GetIO().MouseDelta);
            
            
            
            ImU32 node_bg_color = /*(node_hovered_in_list == node->ID || node_hovered_in_scene == node->ID || (node_hovered_in_list == -1 && node_selected == node->ID)) ? IM_COL32(75, 75, 75, 255) :*/ IM_COL32(40, 40, 40, 255);
            ImU32 node_hd_color = IM_COL32(node->getColor().r,node->getColor().g,node->getColor().b,64);
            
            
            
            draw_list->AddRectFilled(node_rect_min, node_rect_max, node_bg_color, 4.0f);
            draw_list->AddRectFilled(node_rect_min, node_rect_header, node_hd_color, 4.0f);
            
            //draw_list->AddRect(node_rect_min, node_rect_max, IM_COL32(0, 0, 0, 255), 4.0f);
            
            if (ImGui::IsItemHovered() && ImGui::IsMouseDown(0) && ImGui::GetIO().KeySuper /*&& ImGui::IsKeyDown(OF_KEY_ALT)*/ && !isNodeDuplicated){
                node->duplicate();
                //TODO: Change focus to new duplicated node
                isNodeDuplicated = true;
            }
            
            for (auto &param : *node->getParameters().get())
                draw_list->AddCircleFilled(node->getSinkConnectionPositionFromParameter(*param) - glm::vec2(NODE_WINDOW_PADDING.x, 0), 3, IM_COL32(0, 0, 0, 255));
            for (auto &param : *node->getParameters().get())
                draw_list->AddCircleFilled(node->getSourceConnectionPositionFromParameter(*param) + glm::vec2(NODE_WINDOW_PADDING.x, 0), 3, IM_COL32(0, 0, 0, 255));
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
    
    if(container->isOpenConnection()){
        if(!ImGui::IsMouseDown(1)){
            container->destroyTemporalConnection();
        }
    }
    
    draw_list->ChannelsMerge();
    
    
    // Open context menu
    if (!ImGui::IsAnyItemHovered() && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(1))
    {
        newNodeClickPos = ImGui::GetMousePos();
        ImGui::OpenPopup("New Node");
        searchField = "";
        numTimesPopup = 0;
    }
    
    // Draw New Node menu
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    if (ImGui::BeginPopup("New Node"))
    {
        char * cString = new char[256];
        strcpy(cString, searchField.c_str());
        
        if(ImGui::InputText("Search", cString, 256)){
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
            if(searchField != "") ImGui::SetNextTreeNodeOpen(true);
            
            if(ImGui::TreeNode(categoriesVector[i].c_str())){
                for(auto &op : options[i]){
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
                    if(showThis){
                        if(ImGui::Selectable(op.c_str()) || isEnterPressed){
                            unique_ptr<ofxOceanodeNodeModel> type = container->getRegistry()->create(op);
                            
                            if (type)
                            {
                                auto &node = container->createNode(std::move(type));
                                
                                node.getNodeGui().setPosition(newNodeClickPos - offset);
                            }
                            ImGui::CloseCurrentPopup();
                            isEnterPressed = false; //Next options we dont want to create them;
                            break;
                        }
                    }
                }
                ImGui::TreePop();
            }
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
    
    // Scrolling
    if (ImGui::IsWindowHovered() /*&& !ImGui::IsAnyItemActive() */&& ImGui::IsMouseDragging(0, 0.0f)){
        scrolling = scrolling + ImGui::GetIO().MouseDelta;
    }
    
    ImGui::PopItemWidth();
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    ImGui::EndGroup();
    
    ImGui::End();
    
//    gui.end();
    
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
