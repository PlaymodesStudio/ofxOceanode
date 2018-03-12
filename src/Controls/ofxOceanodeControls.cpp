//
//  ofxOceanodeControls.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 12/03/2018.
//

#include "ofxOceanodeControls.h"

ofxOceanodeControls::ofxOceanodeControls(){
    ofAppBaseWindow* mainWindow = ofGetWindowPtr();
    
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
    controlsWindow = ofCreateWindow(prevSettings);
    controlsWindow->setWindowTitle("Controls");
    ofAddListener(controlsWindow->events().draw, this, &ofxOceanodeControls::draw);
    ofAddListener(controlsWindow->events().keyPressed, this, &ofxOceanodeControls::keyPressed);
    ofAddListener(controlsWindow->events().mouseMoved, this, &ofxOceanodeControls::mouseMoved);
    ofAddListener(controlsWindow->events().mousePressed, this, &ofxOceanodeControls::mousePressed);
    ofAddListener(controlsWindow->events().mouseReleased, this, &ofxOceanodeControls::mouseReleased);
    ofAddListener(controlsWindow->events().mouseDragged, this, &ofxOceanodeControls::mouseDragged);
//    ofAppGLFWWindow * ofWindow = (ofAppGLFWWindow*)controlsWindow.get();
//    GLFWwindow * glfwWindow = ofWindow->getGLFWWindow();
    //        glfwSetWindowCloseCallback(glfwWindow, window_no_close_indexer);
    
    ofGetMainLoop()->setCurrentWindow((ofAppGLFWWindow*)mainWindow);
}


void ofxOceanodeControls::draw(ofEventArgs &e){
    ofDrawCircle(ofRandom(ofGetWidth()), ofRandom(ofGetHeight()), 50);
}



void ofxOceanodeControls::mouseMoved(ofMouseEventArgs &a){
    
}

void ofxOceanodeControls::mousePressed(ofMouseEventArgs &a){
 
}

void ofxOceanodeControls::mouseReleased(ofMouseEventArgs &a){
    
}

void ofxOceanodeControls::mouseDragged(ofMouseEventArgs &a){
    
}
