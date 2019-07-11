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

void ofxOceanodeCanvas::setup(std::shared_ptr<ofAppBaseWindow> window){
    ofSetWindowTitle("Canvas");
    
    listeners.push(window->events().update.newListener(this, &ofxOceanodeCanvas::update));
    listeners.push(window->events().draw.newListener(this, &ofxOceanodeCanvas::draw));
    
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
    
    ofxDatGui::setAssetPath("");
    
    ///POP UP MENuS
    popUpMenu = new ofxDatGui(-1, -1, window);
    popUpMenu->setVisible(false);
    searchField = popUpMenu->addTextInput("Search: ");
    searchField->setNotifyEachChange(true);
    auto const &models = container->getRegistry()->getRegisteredModels();
    auto const &categories = container->getRegistry()->getCategories();
    auto const &categoriesModelsAssociation = container->getRegistry()->getRegisteredModelsCategoryAssociation();
    
    vector<string> categoriesVector;
    for(auto cat : categories){
        categoriesVector.push_back(cat);
    }
    
    vector<vector<string>> options = vector<vector<string>>(categories.size());
    for(int i = 0; i < categories.size(); i++){
        vector<string> options;
        for(auto &model : models){
            if(categoriesModelsAssociation.at(model.first) == categoriesVector[i]){
                options.push_back(model.first);
            }
        }
        std::sort(options.begin(), options.end());
        modulesSelectors.push_back(popUpMenu->addDropdown(categoriesVector[i], options));
        //modulesSelectors.back()->expand();
    }
    
    std::unique_ptr<ofxDatGuiTheme> theme = make_unique<ofxDatGuiThemeCharcoal>();
    //    theme->color.textInput.text = ;
    //    theme->color.icons = color;
    theme->layout.width = 290;
    popUpMenu->setTheme(theme.get(), true);
    searchField->setLabelColor(theme->color.label*2);
    for(auto drop : modulesSelectors){
        drop->setLabelColor(theme->color.label*2);
    }
    
    popUpMenu->onDropdownEvent(this, &ofxOceanodeCanvas::newModuleListener);
    popUpMenu->onTextInputEvent(this, &ofxOceanodeCanvas::searchListener);
    selectedRect = ofRectangle(0, 0, 0, 0);
    dragModulesInitialPoint = glm::vec2(NAN, NAN);
    selecting = false;
}

void ofxOceanodeCanvas::draw(ofEventArgs &args){
    if(selecting){
        ofPushStyle();
        ofSetColor(255, 255, 255, 40);
        ofDrawRectangle(ofRectangle(canvasToScreen(selectInitialPoint), canvasToScreen(selectEndPoint)));
        ofPopStyle();
    }
    else if(selectedRect != ofRectangle(0,0,0,0)){
        ofPushStyle();
        if(entireSelect)
            ofSetColor(255, 80, 0, 40);
        else
            ofSetColor(0, 80, 255, 40);
        ofDrawRectangle(ofRectangle(canvasToScreen(selectedRect.getTopLeft()), canvasToScreen(selectedRect.getBottomRight())));
        ofPopStyle();
    }
}

void ofxOceanodeCanvas::newModuleListener(ofxDatGuiDropdownEvent e){
    unique_ptr<ofxOceanodeNodeModel> type = container->getRegistry()->create(e.target->getChildAt(e.child)->getName());
    
    if (type)
    {
        auto &node = container->createNode(std::move(type));
        
        node.getNodeGui().setPosition(screenToCanvas(glm::vec2(popUpMenu->getPosition().x, popUpMenu->getPosition().y)));
        node.getNodeGui().setTransformationMatrix(transformationMatrix);
    }
    popUpMenu->setVisible(false);
    searchField->setFocused(false);
    popUpMenu->setPosition(-1, -1);
    for(auto drop : modulesSelectors){
        drop->setLabel(drop->getName());
        drop->collapse();
        for(int i = 0; i < drop->getNumOptions(); i++){
            drop->getChildAt(i)->setVisible(true);
        }
    }
}

void ofxOceanodeCanvas::searchListener(ofxDatGuiTextInputEvent e){
    searchedOptions.clear();
    if(e.text != ""){
        for(auto drop : modulesSelectors){
            int numMathes = 0;
            for(int i = 0; i < drop->getNumOptions(); i++){
                auto item = drop->getChildAt(i);
                string lowercaseName = item->getName();
                std::transform(lowercaseName.begin(), lowercaseName.end(), lowercaseName.begin(), ::tolower);
                if(ofStringTimesInString(item->getName(), e.text) || ofStringTimesInString(lowercaseName, e.text)){
                    searchedOptions.push_back(make_pair(drop, i));
                    numMathes++;
                }else{
                    item->setVisible(false);
                }
            }
            if(numMathes == 0){
                for(int i = 0; i < drop->getNumOptions(); i++){
                    drop->getChildAt(i)->setVisible(true);
                }
                drop->collapse();
            }else{
                drop->expand();
            }
        }
        if(searchedOptions.size() == 0){
            for(auto drop : modulesSelectors){
                drop->collapse();
                for(int i = 0; i < drop->getNumOptions(); i++){
                    drop->getChildAt(i)->setVisible(true);
                }
            }
        }
    }else{
        for(auto drop : modulesSelectors){
            drop->setLabel(drop->getName());
            drop->collapse();
            for(int i = 0; i < drop->getNumOptions(); i++){
                drop->getChildAt(i)->setVisible(true);
            }
        }
    }
}

void ofxOceanodeCanvas::keyPressed(ofKeyEventArgs &e){
    if(searchField->ofxDatGuiComponent::getFocused()){
        if(e.key == OF_KEY_RETURN){
            if(searchedOptions.size() != 0){
                ofxDatGuiDropdownEvent e(searchedOptions[0].first, 0, searchedOptions[0].second);
                newModuleListener(e);
            }
        }
    }else{
        if(e.key == ' '){
            //glfwSetCursor((GLFWwindow*)ofGetWindowPtr()->getWindowContext(), openedHandCursor);
            if(ofGetMousePressed()) dragCanvasInitialPoint = glm::vec2(ofGetMouseX(), ofGetMouseY());
        }else if(e.key == OF_KEY_BACKSPACE){
            popUpMenu->setVisible(false);
            searchField->setFocused(false);
            toMoveNodes.clear();
            selectedRect = ofRectangle(0,0,0,0);
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
            }
        }
    }
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
            searchField->setText("");
            searchField->setFocused(true);
            popUpMenu->setPosition(e.x, e.y);
            popUpMenu->setVisible(true);
        }
        else if(e.button == 2){
            transformationMatrix->set(glm::mat4(1));
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
    return glm::translate(glm::mat4(), translationVector) * m;
}

#endif
