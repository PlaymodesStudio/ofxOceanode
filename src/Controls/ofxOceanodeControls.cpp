//
//  ofxOceanodeControls.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 12/03/2018.
//

#include "ofxOceanodeControls.h"
#include "ofxOceanodePresetsController.h"
#include "ofxOceanodeBPMController.h"

ofxOceanodeControls::ofxOceanodeControls(shared_ptr<ofxOceanodeContainer> _container) : container(_container){
    
    ofGLFWWindowSettings prevSettings;
//    if(reindexWindowRect.getPosition() == glm::vec3(-1, -1, 0)){
        prevSettings.setSize(500, ofGetScreenHeight());
        prevSettings.setPosition(glm::vec2(0, 0));
//    }
//    else{
//        prevSettings.setSize(reindexWindowRect.width, reindexWindowRect.height);
//        prevSettings.setPosition(reindexWindowRect.position);
//    }
    prevSettings.windowMode = OF_WINDOW;
    prevSettings.resizable = true;
//    prevSettings.shareContextWith = ofGetCurrentWindow();
    prevSettings.setGLVersion(ofGetGLRenderer()->getGLVersionMajor(), ofGetGLRenderer()->getGLVersionMinor());
    controlsWindow = ofCreateWindow(prevSettings);
    controlsWindow->setWindowTitle("Controls");
    ofAddListener(controlsWindow->events().draw, this, &ofxOceanodeControls::draw);
    ofAddListener(controlsWindow->events().update, this, &ofxOceanodeControls::update);
    ofAddListener(controlsWindow->events().keyPressed, this, &ofxOceanodeControls::keyPressed);
    ofAddListener(controlsWindow->events().mouseMoved, this, &ofxOceanodeControls::mouseMoved);
    ofAddListener(controlsWindow->events().mousePressed, this, &ofxOceanodeControls::mousePressed);
    ofAddListener(controlsWindow->events().mouseReleased, this, &ofxOceanodeControls::mouseReleased);
    ofAddListener(controlsWindow->events().mouseDragged, this, &ofxOceanodeControls::mouseDragged);
    ofAddListener(controlsWindow->events().windowResized, this, &ofxOceanodeControls::windowResized);
//    ofAppGLFWWindow * ofWindow = (ofAppGLFWWindow*)controlsWindow.get();
//    GLFWwindow * glfwWindow = ofWindow->getGLFWWindow();
    //        glfwSetWindowCloseCallback(glfwWindow, window_no_close_indexer);
    
    controllers.push_back(make_unique<ofxOceanodePresetsController>(container));
    controllers.push_back(make_unique<ofxOceanodeBPMController>());
    controllers.push_back(make_unique<ofxOceanodeBaseController>("Midi"));
    controllers.push_back(make_unique<ofxOceanodeBaseController>("Osc"));
    controllers.push_back(make_unique<ofxOceanodeBaseController>("Artnet"));
    
    if(controllers.size() > 0){
        controllers[0]->activate();
    }
    
    resizeButtons();
}


void ofxOceanodeControls::draw(ofEventArgs &e){
    for(int i = 0; i < controllersButtons.size(); i++){
        controllers[i]->draw();
        controllers[i]->getButton().draw(controllersButtons[i]);
    }
}

void ofxOceanodeControls::update(ofEventArgs &e){
    for(auto &c : controllers){
        c->update();
    }
}

void ofxOceanodeControls::mouseMoved(ofMouseEventArgs &a){
    
}

void ofxOceanodeControls::mousePressed(ofMouseEventArgs &a){
    for(int i = 0; i < controllersButtons.size(); i++){
        if(controllersButtons[i].inside(a)){
            controllers[i]->activate();
            for(int j = 0; j < controllers.size(); j++){
                if(j != i){
                    controllers[j]->deactivate();
                }
            }
            break;
        }
    }
}

void ofxOceanodeControls::mouseReleased(ofMouseEventArgs &a){
    
}

void ofxOceanodeControls::mouseDragged(ofMouseEventArgs &a){
    
}

void ofxOceanodeControls::windowResized(ofResizeEventArgs &a){
    resizeButtons();
    for(auto &controller : controllers){
        controller->windowResized(a);
    }
}

void ofxOceanodeControls::resizeButtons(){
    int numControllers = controllers.size();
    controllersButtons.resize(numControllers);
    float itemSize = controlsWindow->getWidth()/numControllers;
    for(int i = 0; i < numControllers; i++){
        controllersButtons[i].setPosition(i*itemSize, 0);
        controllersButtons[i].setSize(itemSize, 30);
    }
}
