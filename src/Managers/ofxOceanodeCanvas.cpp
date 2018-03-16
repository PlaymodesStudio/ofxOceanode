//
//  ofxOceanode.cpp
//  example-basic
//
//  Created by Eduard Frigola on 19/06/2017.
//
//

#include "ofxOceanodeCanvas.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>

void ofxOceanodeCanvas::setup(){
    ofSetWindowTitle("Canvas");
    
    ofAddListener(ofEvents().update, this, &ofxOceanodeCanvas::update);
    ofAddListener(ofEvents().draw, this, &ofxOceanodeCanvas::draw);
    
    ofRegisterKeyEvents(this);
    ofRegisterMouseEvents(this);
    
    ///POP UP MENuS
    popUpMenu = new ofxDatGui();
    popUpMenu->setVisible(false);
    popUpMenu->setPosition(-1, -1);
    auto const &models = container->getRegistry().getRegisteredModels();
    vector<string> options;
    for(auto &model : models){
        options.push_back(model.first);
    }
    std::sort(options.begin(), options.end());
    
    transformationMatrix = &container->getTransformationMatrix();
    
    popUpMenu->addDropdown("Choose module", options)->expand();
    popUpMenu->onDropdownEvent(this, &ofxOceanodeCanvas::newModuleListener);
}

void ofxOceanodeCanvas::newModuleListener(ofxDatGuiDropdownEvent e){
    unique_ptr<ofxOceanodeNodeModel> type = container->getRegistry().create(e.target->getSelected()->getName());
    
    if (type)
    {
        auto &node = container->createNode(std::move(type));
        
        node.getNodeGui().setPosition(canvasToScreen(glm::vec2(popUpMenu->getPosition().x, popUpMenu->getPosition().y)));
        node.getNodeGui().setTransformationMatrix(transformationMatrix);
    }
    popUpMenu->setVisible(false);
    popUpMenu->setPosition(-1, -1);
    ofxDatGuiDropdown* drop = popUpMenu->getDropdown("Choose module");
    drop->setLabel("Choose module");
}

void ofxOceanodeCanvas::keyPressed(ofKeyEventArgs &e){
    if(e.key == ' '){
        //glfwSetCursor((GLFWwindow*)ofGetWindowPtr()->getWindowContext(), openedHandCursor);
        if(ofGetMousePressed()) dragCanvasInitialPoint = glm::vec2(ofGetMouseX(), ofGetMouseY());
    }else if(e.key == OF_KEY_BACKSPACE && popUpMenu->getVisible()){
        popUpMenu->setVisible(false);
    }
}

void ofxOceanodeCanvas::mouseDragged(ofMouseEventArgs &e){
    if(ofGetKeyPressed(' ')){
        transformationMatrix->set(translateMatrixWithoutScale(transformationMatrix->get(), glm::vec3(e-dragCanvasInitialPoint, 0)));
        dragCanvasInitialPoint = e;
    }
}

void ofxOceanodeCanvas::mousePressed(ofMouseEventArgs &e){
    glm::vec2 transformedPos = screenToCanvas(e);
    if(ofGetKeyPressed(OF_KEY_COMMAND)){
        if(e.button == 0){
            popUpMenu->setPosition(e.x, e.y);
            popUpMenu->setVisible(true);
            popUpMenu->getDropdown("Choose module")->expand();
        }
        else if(e.button == 2){
            transformationMatrix->set(glm::mat4(1));
        }
    }
    if(ofGetKeyPressed(' ')){
        dragCanvasInitialPoint = e;
    }
//    if(ofGetKeyPressed(OF_KEY_CONTROL)){
//        bool cablePressed = false;
//        for(auto connection : connections){
//            if(connection->hitTest(transformedPos) && !cablePressed){
//                connection->toggleGui(true, e);
//                cablePressed = true;
//            }
//            else if(connection->closedLine){
//                connection->toggleGui(false);
//            }
//        }
//    }
}

void ofxOceanodeCanvas::mouseScrolled(ofMouseEventArgs &e){
    glm::vec2 transformedPos = canvasToScreen(e);
    if(ofGetKeyPressed(OF_KEY_COMMAND)){
        float scrollValue = e.scrollY/100.0;
        transformationMatrix->set(translateMatrixWithoutScale(transformationMatrix->get(), glm::vec3(transformedPos, 0) * getMatrixScale(transformationMatrix->get()) * scrollValue));
        transformationMatrix->set(glm::scale(transformationMatrix->get(), glm::vec3(1-(scrollValue), 1-(scrollValue), 1)));
//        if(e.scrollY < 0)
//        glfwSetCursor((GLFWwindow*)ofGetWindowPtr()->getWindowContext(), zoomInCursor);
//        else
//        glfwSetCursor((GLFWwindow*)ofGetWindowPtr()->getWindowContext(), zoomOutCursor);
    }else if(ofGetKeyPressed(OF_KEY_ALT)){
        transformationMatrix->set(translateMatrixWithoutScale(transformationMatrix->get(), glm::vec3(-e.scrollY*2, 0, 0)));
    }else{
        transformationMatrix->set(translateMatrixWithoutScale(transformationMatrix->get(), glm::vec3(e.scrollX*2, e.scrollY*2, 0)));
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
    return glm::translate(glm::mat4(), translationVector) * m;
}
