//
//  ofxOceanodeControls.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 12/03/2018.
//

#ifndef OFXOCEANODE_HEADLESS

#include "ofxOceanodeControls.h"
#include "ofxOceanodePresetsController.h"
#include "ofxOceanodeBPMController.h"

#ifdef OFXOCEANODE_USE_OSC
    #include "ofxOceanodeOSCController.h"
#endif

#ifdef OFXOCEANODE_USE_MIDI
    #include "ofxOceanodeMidiController.h"
#endif

ofxOceanodeControls::ofxOceanodeControls(shared_ptr<ofxOceanodeContainer> _container) : container(_container){
    
    ofGLFWWindowSettings prevSettings;
    prevSettings.setSize(500, ofGetScreenHeight());
    prevSettings.setPosition(glm::vec2(0, 0));
    prevSettings.windowMode = OF_WINDOW;
    prevSettings.resizable = true;
    prevSettings.shareContextWith = ofGetCurrentWindow();
    prevSettings.setGLVersion(ofGetGLRenderer()->getGLVersionMajor(), ofGetGLRenderer()->getGLVersionMinor());
    controlsWindow = ofCreateWindow(prevSettings);
    controlsWindow->setWindowTitle("Controls");
    controlsWindow->setVerticalSync(false);
    listeners.push(controlsWindow->events().draw.newListener(this, &ofxOceanodeControls::draw));
    listeners.push(controlsWindow->events().update.newListener(this, &ofxOceanodeControls::update));
    listeners.push(controlsWindow->events().exit.newListener(this, &ofxOceanodeControls::exit));
    listeners.push(controlsWindow->events().keyPressed.newListener(this, &ofxOceanodeControls::keyPressed));
    listeners.push(controlsWindow->events().mouseMoved.newListener(this, &ofxOceanodeControls::mouseMoved));
    listeners.push(controlsWindow->events().mousePressed.newListener(this, &ofxOceanodeControls::mousePressed));
    listeners.push(controlsWindow->events().mouseReleased.newListener(this, &ofxOceanodeControls::mouseReleased));
    listeners.push(controlsWindow->events().mouseDragged.newListener(this, &ofxOceanodeControls::mouseDragged));
    listeners.push(controlsWindow->events().windowResized.newListener(this, &ofxOceanodeControls::windowResized));
    
    controllers.push_back(make_unique<ofxOceanodePresetsController>(container));
    controllers.push_back(make_unique<ofxOceanodeBPMController>(container));
    
#ifdef OFXOCEANODE_USE_OSC
    controllers.push_back(make_unique<ofxOceanodeOSCController>(container));
#endif
    
#ifdef OFXOCEANODE_USE_MIDI
    controllers.push_back(make_unique<ofxOceanodeMidiController>(get<ofxOceanodePresetsController>(), container));
#endif
    
    if(controllers.size() > 0){
        controllers[0]->activate();
    }
    
    resizeButtons();
    windowRect = ofRectangle(0,0,0,0);
}


void ofxOceanodeControls::draw(ofEventArgs &e){
    ofSetColor(32,32,32);
    ofDrawRectangle(0,0,ofGetWidth(),ofGetHeight());
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

void ofxOceanodeControls::exit(ofEventArgs &e){
    windowRect.setPosition(glm::vec3(controlsWindow->getWindowPosition(), 0));
    windowRect.setSize(controlsWindow->getWidth(), controlsWindow->getHeight());
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

#endif
