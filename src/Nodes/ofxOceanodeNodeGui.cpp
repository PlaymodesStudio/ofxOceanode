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

ofxOceanodeNodeGui::ofxOceanodeNodeGui(ofxOceanodeContainer& _container, ofxOceanodeNode& _node, shared_ptr<ofAppBaseWindow> window) : container(_container), node(_node){
    color = node.getColor();
    //color.setBrightness(255);
    guiRect = ofRectangle(10, 10, 10, 10);
    guiToBeDestroyed = false;
    lastExpandedState = true;
    isGuiCreated = false;
#ifdef OFXOCEANODE_USE_MIDI
    isListeningMidi = false;
#endif
    
    if(window == nullptr){
//        keyAndMouseListeners.push(ofEvents().keyPressed.newListener(this,&ofxOceanodeNodeGui::keyPressed));
//        keyAndMouseListeners.push(ofEvents().keyReleased.newListener(this,&ofxOceanodeNodeGui::keyReleased));
//        keyAndMouseListeners.push(ofEvents().mouseDragged.newListener(this,&ofxOceanodeNodeGui::mouseDragged));
//        keyAndMouseListeners.push(ofEvents().mouseMoved.newListener(this,&ofxOceanodeNodeGui::mouseMoved));
//        keyAndMouseListeners.push(ofEvents().mousePressed.newListener(this,&ofxOceanodeNodeGui::mousePressed));
//        keyAndMouseListeners.push(ofEvents().mouseReleased.newListener(this,&ofxOceanodeNodeGui::mouseReleased));
//        keyAndMouseListeners.push(ofEvents().mouseScrolled.newListener(this,&ofxOceanodeNodeGui::mouseScrolled));
//        keyAndMouseListeners.push(ofEvents().mouseEntered.newListener(this,&ofxOceanodeNodeGui::mouseEntered));
//        keyAndMouseListeners.push(ofEvents().mouseExited.newListener(this,&ofxOceanodeNodeGui::mouseExited));
    }else{
//        keyAndMouseListeners.push(window->events().keyPressed.newListener(this,&ofxOceanodeNodeGui::keyPressed));
//        keyAndMouseListeners.push(window->events().keyReleased.newListener(this,&ofxOceanodeNodeGui::keyReleased));
//        keyAndMouseListeners.push(window->events().mouseDragged.newListener(this,&ofxOceanodeNodeGui::mouseDragged));
//        keyAndMouseListeners.push(window->events().mouseMoved.newListener(this,&ofxOceanodeNodeGui::mouseMoved));
//        keyAndMouseListeners.push(window->events().mousePressed.newListener(this,&ofxOceanodeNodeGui::mousePressed));
//        keyAndMouseListeners.push(window->events().mouseReleased.newListener(this,&ofxOceanodeNodeGui::mouseReleased));
//        keyAndMouseListeners.push(window->events().mouseScrolled.newListener(this,&ofxOceanodeNodeGui::mouseScrolled));
//        keyAndMouseListeners.push(window->events().mouseEntered.newListener(this,&ofxOceanodeNodeGui::mouseEntered));
//        keyAndMouseListeners.push(window->events().mouseExited.newListener(this,&ofxOceanodeNodeGui::mouseExited));
    }
}

ofxOceanodeNodeGui::~ofxOceanodeNodeGui(){
    
}

bool ofxOceanodeNodeGui::constructGui(){
    string moduleName = getParameters()->getName();
    ImGui::BeginGroup(); // Lock horizontal position
    
    bool deleteModule = false;
    
//    ImGui::SameLine(0, 10);
    ImGui::Text("%s", moduleName.c_str());
    
    ImGui::SameLine(guiRect.width - 30);
    if (ImGui::Button("x"))
        ImGui::OpenPopup("Delete?");
    if (ImGui::BeginPopupModal("Delete?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("%s", ("Are you sure you want to delete.\n " + moduleName + "\n").c_str());
        ImGui::Separator();
        
        if (ImGui::Button("OK", ImVec2(120,0)) || ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Enter))) {
            int index = 0;
            for(auto &key : ImGui::GetIO().KeysDown){
                if(key){
                    ofLog() << index;
                }
                index++;
            }
            ofLog() << "ImguiEnter = " << ImGuiKey_Enter;
            ofLog() << "OF_KEY_RETURN = " << OF_KEY_RETURN;
            ImGui::CloseCurrentPopup();
            deleteModule = true;
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120,0))) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    
    for(int i=0 ; i<getParameters()->size(); i++){
        ofAbstractParameter &absParam = getParameters()->get(i);
        string uniqueId = absParam.getName();
        if(absParam.type() == typeid(ofParameter<float>).name()){
            auto tempCast = absParam.cast<float>();
            if (ImGui::SliderFloat(uniqueId.c_str(), (float *)&tempCast.get(), tempCast.getMin(), tempCast.getMax()))
            {
                tempCast = tempCast;
            }
        }else if(absParam.type() == typeid(ofParameter<int>).name()){
            auto tempCast = absParam.cast<int>();
            if (ImGui::SliderInt(uniqueId.c_str(), (int *)&tempCast.get(), tempCast.getMin(), tempCast.getMax()))
            {
                tempCast = tempCast;
            }
        }else if(absParam.type() == typeid(ofParameter<bool>).name()){
            auto tempCast = absParam.cast<bool>();
            if (ImGui::Checkbox(uniqueId.c_str(), (bool *)&tempCast.get()))
            {
                tempCast = tempCast;
            }
        }else if(absParam.type() == typeid(ofParameter<void>).name()){
            if (ImGui::Button(uniqueId.c_str()))
            {
                absParam.cast<void>().trigger();
            }
        }else if(absParam.type() == typeid(ofParameter<string>).name()){
            auto tempCast = absParam.cast<string>();
            char * cString = new char[256];
            strcpy(cString, tempCast.get().c_str());
            auto result = false;
            if (ImGui::InputText(uniqueId.c_str(), cString, 256, ImGuiInputTextFlags_EnterReturnsTrue))
            {
                tempCast = tempCast;
            }
            delete[] cString;
        }else if(absParam.type() == typeid(ofParameter<char>).name()){
            ImGui::Text("%s", absParam.getName().c_str());
        }else if(absParam.type() == typeid(ofParameter<ofColor>).name()){
            auto tempCast = absParam.cast<ofFloatColor>();
            if (ImGui::ColorEdit4(uniqueId.c_str(), (float*)&tempCast.get().r))
            {
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
            if(ImGui::Combo(uniqueId.c_str(), (int*)&tempCast.getInt(1).get(), vector_getter, static_cast<void*>(&options), options.size())){
                tempCast.getInt(1) = tempCast.getInt(1);
            }
        }else if(absParam.type() == typeid(ofParameter<vector<float>>).name()){
            auto tempCast = absParam.cast<vector<float>>();
            if(tempCast->size() == 1){
                if (ImGui::SliderFloat(uniqueId.c_str(), (float *)&tempCast->at(0), tempCast.getMin()[0], tempCast.getMax()[0]))
                {
                    tempCast = vector<float>(1, tempCast->at(0));
                }
            }else{
                ImGui::PlotHistogram(uniqueId.c_str(), tempCast->data(), tempCast->size(), 0, NULL, tempCast.getMin()[0], tempCast.getMax()[0]);
            }
        }else if(absParam.type() == typeid(ofParameter<vector<int>>).name()){
            auto tempCast = absParam.cast<vector<int>>();
            if(tempCast->size() == 1){
                if (ImGui::SliderInt(uniqueId.c_str(), (int *)&tempCast->at(0), tempCast.getMin()[0], tempCast.getMax()[0]))
                {
                    tempCast = vector<int>(1, tempCast->at(0));
                }
            }else{
                std::vector<float> floatVec(tempCast.get().begin(), tempCast.get().end());
                ImGui::PlotHistogram(uniqueId.c_str(), floatVec.data(), tempCast->size(), 0, NULL, tempCast.getMin()[0], tempCast.getMax()[0]);
            }
            
        }else if(absParam.type() == typeid(ofParameter<pair<int, bool>>).name()){
//            auto pairParam = absParam.cast<pair<int, bool>>();
//            auto matrix = gui->addMatrix(absParam.getName(), pairParam.get().first, true);
//            matrix->setRadioMode(true);
//            matrix->setHoldMode(pairParam.get().second);
//            parameterChangedListeners.push(pairParam.newListener([&, matrix](pair<int, bool> &pair){
//                if(pair.second){
//                    matrix->select(pair.first);
//                }else{
//                    matrix->deselect(pair.first);
//                }
//            }));
        }else {
            ImGui::Text("%s", absParam.getName().c_str());
        }
        inputPositions[uniqueId] = glm::vec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMin().y + ImGui::GetItemRectSize().y/2);
        outputPositions[uniqueId] = glm::vec2(ImGui::GetItemRectMax().x, ImGui::GetItemRectMin().y + ImGui::GetItemRectSize().y/2);
        
        if(ImGui::IsItemClicked(1)){
            auto connection = node.parameterConnectionPress(container, absParam);
        }else if(container.isOpenConnection() && ImGui::IsItemHovered() && !ImGui::IsMouseDown(1)){
            auto connection = node.parameterConnectionRelease(container, absParam);
        }
    }
    ImGui::EndGroup();
    for(auto &outPos : outputPositions){
        outPos.second.x = inputPositions[outPos.first].x + ImGui::GetItemRectSize().x;
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

void ofxOceanodeNodeGui::setWindow(shared_ptr<ofAppBaseWindow> window){
    keyAndMouseListeners.unsubscribeAll();
    
//    if(window != nullptr && !isGuiCreated){
//        createGuiFromParameters(window);
//    }else{
//        gui->setWindow(window);
//    }
    
    if(window == nullptr){
//        keyAndMouseListeners.push(ofEvents().keyPressed.newListener(this,&ofxOceanodeNodeGui::keyPressed));
//        keyAndMouseListeners.push(ofEvents().keyReleased.newListener(this,&ofxOceanodeNodeGui::keyReleased));
//        keyAndMouseListeners.push(ofEvents().mouseDragged.newListener(this,&ofxOceanodeNodeGui::mouseDragged));
//        keyAndMouseListeners.push(ofEvents().mouseMoved.newListener(this,&ofxOceanodeNodeGui::mouseMoved));
//        keyAndMouseListeners.push(ofEvents().mousePressed.newListener(this,&ofxOceanodeNodeGui::mousePressed));
//        keyAndMouseListeners.push(ofEvents().mouseReleased.newListener(this,&ofxOceanodeNodeGui::mouseReleased));
//        keyAndMouseListeners.push(ofEvents().mouseScrolled.newListener(this,&ofxOceanodeNodeGui::mouseScrolled));
//        keyAndMouseListeners.push(ofEvents().mouseEntered.newListener(this,&ofxOceanodeNodeGui::mouseEntered));
//        keyAndMouseListeners.push(ofEvents().mouseExited.newListener(this,&ofxOceanodeNodeGui::mouseExited));
    }else{
//        keyAndMouseListeners.push(window->events().keyPressed.newListener(this,&ofxOceanodeNodeGui::keyPressed));
//        keyAndMouseListeners.push(window->events().keyReleased.newListener(this,&ofxOceanodeNodeGui::keyReleased));
//        keyAndMouseListeners.push(window->events().mouseDragged.newListener(this,&ofxOceanodeNodeGui::mouseDragged));
//        keyAndMouseListeners.push(window->events().mouseMoved.newListener(this,&ofxOceanodeNodeGui::mouseMoved));
//        keyAndMouseListeners.push(window->events().mousePressed.newListener(this,&ofxOceanodeNodeGui::mousePressed));
//        keyAndMouseListeners.push(window->events().mouseReleased.newListener(this,&ofxOceanodeNodeGui::mouseReleased));
//        keyAndMouseListeners.push(window->events().mouseScrolled.newListener(this,&ofxOceanodeNodeGui::mouseScrolled));
//        keyAndMouseListeners.push(window->events().mouseEntered.newListener(this,&ofxOceanodeNodeGui::mouseEntered));
//        keyAndMouseListeners.push(window->events().mouseExited.newListener(this,&ofxOceanodeNodeGui::mouseExited));
    }
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

//void ofxOceanodeNodeGui::keyPressed(ofKeyEventArgs &args){
////    if(args.key == 'r' && !args.isRepeat){
////        if(gui->hitTest(ofVec2f(ofGetMouseX(), ofGetMouseY()))){
////            guiToBeDestroyed = true;
////            gui->setOpacity(0.2);
////        }
////    }
//}
//
//void ofxOceanodeNodeGui::keyReleased(ofKeyEventArgs &args){
////    if(args.key == 'r'){
////        guiToBeDestroyed = false;
////        gui->setOpacity(1);
////    }
//}
//
//void ofxOceanodeNodeGui::mouseDragged(ofMouseEventArgs &args){
////    glm::vec2 guiCurrentPos = glm::vec2(gui->getPosition().x, gui->getPosition().y);
////    if(guiCurrentPos != position){
////        node.moveConnections(guiCurrentPos - position);
////        position = guiCurrentPos;
////    }
//}
//
//void ofxOceanodeNodeGui::mousePressed(ofMouseEventArgs &args){
////    if(gui->hitTest(args) && args.button != OF_MOUSE_BUTTON_RIGHT){
////       if(args.hasModifier(OF_KEY_ALT)){
////           node.duplicateSelf(toGlm(gui->getPosition() + ofPoint(gui->getWidth() + 10, 0)));
////       }
////       else if(guiToBeDestroyed){
////           node.deleteSelf();
////       }
////    }
//}
//
//void ofxOceanodeNodeGui::mouseReleased(ofMouseEventArgs &args){
//    if(gui->hitTest(args)){
//        if(!gui->getExpanded() && lastExpandedState){
//            auto header = gui->getHeader();
//            node.collapseConnections(glm::vec2(header->getX(), header->getY() + header->getHeight()/2), glm::vec2(header->getX() + header->getWidth(), header->getY() + header->getHeight()/2));
//            lastExpandedState = gui->getExpanded();
//        }
//        else if(gui->getExpanded() && !lastExpandedState){
//            node.expandConnections();
//            lastExpandedState = gui->getExpanded();
//        }
//    }
//}

//void ofxOceanodeNodeGui::onGuiMatrixEvent(ofxDatGuiMatrixEvent e){
//    getParameters()->get(e.target->getName()).cast<pair<int, bool>>() = make_pair(e.child+1, e.enabled);
//}
//
//void ofxOceanodeNodeGui::onGuiRightClickEvent(ofxDatGuiRightClickEvent e){
//    ofAbstractParameter *p = &getParameters()->get(e.target->getName());
//    if(e.target->getType() == ofxDatGuiType::DROPDOWN){
//        p = &getParameters()->getGroup(e.target->getName()).getInt(1);
//    }
//#ifdef OFXOCEANODE_USE_MIDI
//    if(isListeningMidi){
//        if(e.down == 1){
//            if(ofGetKeyPressed(OF_KEY_SHIFT)){
//                container.removeLastMidiBinding(*p);
//            }else{
//                container.createMidiBinding(*p);
//            }
//        }
//    }
//    else
//#endif
//    {
//        if(e.down == 1){
//            auto connection = node.parameterConnectionPress(container, *p);
//            if(connection != nullptr){
//                connection->setTransformationMatrix(transformationMatrix);
//                connection->setSinkPosition(transformationMatrix->get() * glm::vec4(ofGetMouseX(), ofGetMouseY(), 0, 1));
//            }
//        }else{
//            auto connection = node.parameterConnectionRelease(container, *p);
//            if(connection != nullptr){
//                connection->setSinkPosition(getSinkConnectionPositionFromParameter(getParameters()->get(e.target->getName())));
//                connection->setTransformationMatrix(transformationMatrix);
//            }
//        }
//    }
//}

glm::vec2 ofxOceanodeNodeGui::getSourceConnectionPositionFromParameter(ofAbstractParameter& parameter){
    return outputPositions[parameter.getName()];
}

glm::vec2 ofxOceanodeNodeGui::getSinkConnectionPositionFromParameter(ofAbstractParameter& parameter){
    return inputPositions[parameter.getName()];
}

#endif
