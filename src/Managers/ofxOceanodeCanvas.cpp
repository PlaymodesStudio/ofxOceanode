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
    
    transformationMatrix = glm::mat4(1);
    
    popUpMenu->addDropdown("Choose module", options)->expand();
    popUpMenu->onDropdownEvent(this, &ofxOceanodeCanvas::newModuleListener);
}

void ofxOceanodeCanvas::newModuleListener(ofxDatGuiDropdownEvent e){
    unique_ptr<ofxOceanodeNodeModel> type = container->getRegistry().create(e.target->getSelected()->getName());
    
    if (type)
    {
        auto &node = container->createNode(std::move(type));
        
        node.getNodeGui().setPosition(canvasToScreen(glm::vec2(popUpMenu->getPosition().x, popUpMenu->getPosition().y)));
        node.getNodeGui().setTransformationMatrix(&transformationMatrix);
    }
    popUpMenu->setVisible(false);
    popUpMenu->setPosition(-1, -1);
    ofxDatGuiDropdown* drop = popUpMenu->getDropdown("Choose module");
    drop->setLabel("Choose module");
}


void ofxOceanodeCanvas::mouseDragged(ofMouseEventArgs &e){
    if(ofGetKeyPressed(' ')){
        transformationMatrix = translateMatrixWithoutScale(transformationMatrix.get(), glm::vec3(e-dragCanvasInitialPoint, 0));
        dragCanvasInitialPoint = e;
    }
}

void ofxOceanodeCanvas::mousePressed(ofMouseEventArgs &e){
    glm::vec2 transformedPos = transformationMatrix.get() * glm::vec4(e, 0, 0);
//    ofVec4f transformedPos = e;
//    transformedPos -= transformMatrix.getTranslation();
//    transformedPos = transformMatrix.getInverse().postMult(transformedPos);
    if(ofGetKeyPressed(OF_KEY_COMMAND)){
        if(e.button == 0){
            popUpMenu->setPosition(e.x, e.y);
            popUpMenu->setVisible(true);
            popUpMenu->getDropdown("Choose module")->expand();
        }
//        else if(e.button == 1){
//            transformMatrix.translate(-transformedPos * (1-transformMatrix.getScale()));
//            transformMatrix = ofMatrix4x4::newTranslationMatrix(transformMatrix.getTranslation());
//            for(auto &gui : datGuis)
//                gui->setTransformMatrix(transformMatrix);//gui->setTransformMatrix(ofMatrix4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1));
//        }
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
        transformationMatrix = translateMatrixWithoutScale(transformationMatrix.get(), glm::vec3(transformedPos, 0) * getMatrixScale(transformationMatrix.get()) * scrollValue);
        transformationMatrix = glm::scale(transformationMatrix.get(), glm::vec3(1-(scrollValue), 1-(scrollValue), 1));
//        if(e.scrollY < 0)
//        glfwSetCursor((GLFWwindow*)ofGetWindowPtr()->getWindowContext(), zoomInCursor);
//        else
//        glfwSetCursor((GLFWwindow*)ofGetWindowPtr()->getWindowContext(), zoomOutCursor);
    }
}

glm::vec2 ofxOceanodeCanvas::screenToCanvas(glm::vec2 p){
    glm::vec4 result = transformationMatrix.get() * glm::vec4(p, 0, 1);
    return result;
}

glm::vec2 ofxOceanodeCanvas::canvasToScreen(glm::vec2 p){
    glm::vec4 result = glm::inverse(transformationMatrix.get()) * glm::vec4(p, 0, 1);
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
    glm::vec3 scale = getMatrixScale(m);
    glm::mat4 mat = glm::scale(m, glm::vec3(1/scale.x, 1/scale.y, 1));
    mat = glm::translate(mat, translationVector);
    return glm::scale(mat, scale);
}
