//
//  ofxOceanodeNodeModelExternalWindow.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 23/03/2018.
//

#include "ofxOceanodeNodeModelExternalWindow.h"

ofxOceanodeNodeModelExternalWindow::ofxOceanodeNodeModelExternalWindow(string name)  : ofxOceanodeNodeModel(name){
    auto &info = addParameterToGroupAndInfo(showWindow.set("Show Window", false));
    info.acceptInConnection = false;
    info.acceptOutConnection = false;
    info.isSavePreset = false;
    windowListenerEvents.push_back(showWindow.newListener(this, &ofxOceanodeNodeModelExternalWindow::showExternalWindow));
    externalWindowRect.setPosition(-1, -1);
    externalWindow = nullptr;
}

ofxOceanodeNodeModelExternalWindow::~ofxOceanodeNodeModelExternalWindow(){
    if(externalWindow != nullptr){
        externalWindow->setWindowShouldClose();
    }
}

void ofxOceanodeNodeModelExternalWindow::showExternalWindow(bool &b){
    if(b && externalWindow == nullptr){
        ofGLFWWindowSettings prevSettings;
        if(externalWindowRect.getPosition() == glm::vec3(-1, -1, 0)){
            prevSettings.setSize(1024, 1024);
            prevSettings.setPosition(ofVec2f(ofGetScreenWidth()-1024, ofGetScreenHeight()-1024));
        }
        else{
            prevSettings.setSize(externalWindowRect.width, externalWindowRect.height);
            prevSettings.setPosition(externalWindowRect.position);
        }
        prevSettings.windowMode = OF_WINDOW;
        prevSettings.resizable = true;
        prevSettings.shareContextWith = ofGetCurrentWindow();
        prevSettings.setGLVersion(ofGetGLRenderer()->getGLVersionMajor(), ofGetGLRenderer()->getGLVersionMinor());
        externalWindow = ofCreateWindow(prevSettings);
        externalWindow->setWindowTitle(nameIdentifier + " " + ofToString(numIdentifier));
        externalWindow->setVerticalSync(false);
        windowListenerEvents.push_back(externalWindow->events().draw.newListener(this, &ofxOceanodeNodeModelExternalWindow::drawInExternalWindow));
        windowListenerEvents.push_back(externalWindow->events().update.newListener(this, &ofxOceanodeNodeModelExternalWindow::updateForExternalWindow));
        windowListenerEvents.push_back(externalWindow->events().exit.newListener(this, &ofxOceanodeNodeModelExternalWindow::closeExternalWindow));
        windowListenerEvents.push_back(externalWindow->events().keyPressed.newListener(this, &ofxOceanodeNodeModelExternalWindow::keyPressed));
        windowListenerEvents.push_back(externalWindow->events().keyReleased.newListener(this, &ofxOceanodeNodeModelExternalWindow::keyReleased));
        windowListenerEvents.push_back(externalWindow->events().mouseMoved.newListener(this, &ofxOceanodeNodeModelExternalWindow::mouseMoved));
        windowListenerEvents.push_back(externalWindow->events().mousePressed.newListener(this, &ofxOceanodeNodeModelExternalWindow::mousePressed));
        windowListenerEvents.push_back(externalWindow->events().mouseReleased.newListener(this, &ofxOceanodeNodeModelExternalWindow::mouseReleased));
        windowListenerEvents.push_back(externalWindow->events().mouseDragged.newListener(this, &ofxOceanodeNodeModelExternalWindow::mouseDragged));
        windowListenerEvents.push_back(externalWindow->events().mouseScrolled.newListener(this, &ofxOceanodeNodeModelExternalWindow::mouseScrolled));
        windowListenerEvents.push_back(externalWindow->events().mouseEntered.newListener(this, &ofxOceanodeNodeModelExternalWindow::mouseEntered));
        windowListenerEvents.push_back(externalWindow->events().mouseExited.newListener(this, &ofxOceanodeNodeModelExternalWindow::mouseExited));
        windowListenerEvents.push_back(externalWindow->events().windowResized.newListener(this, &ofxOceanodeNodeModelExternalWindow::windowResizedOwnEvent));
        windowListenerEvents.push_back(externalWindow->events().fileDragEvent.newListener(this, &ofxOceanodeNodeModelExternalWindow::dragEvent));
    }
    else if(!b && externalWindow != nullptr){
        externalWindow->setWindowShouldClose();
        externalWindow = nullptr;
    }
}

void ofxOceanodeNodeModelExternalWindow::windowResizedOwnEvent(ofResizeEventArgs &a){
    externalWindowRect.setPosition(glm::vec3(externalWindow->getWindowPosition(), 0));
    externalWindowRect.setSize(externalWindow->getWidth(), externalWindow->getHeight());
    windowResized(a);
}

void ofxOceanodeNodeModelExternalWindow::closeExternalWindow(ofEventArgs &e){
    externalWindowRect.setPosition(glm::vec3(externalWindow->getWindowPosition(), 0));
    externalWindowRect.setSize(externalWindow->getWidth(), externalWindow->getHeight());
    showWindow = false;
}

void ofxOceanodeNodeModelExternalWindow::presetSave(ofJson &json){
    json["ExtWindowRect"] = {externalWindowRect.x, externalWindowRect.y, externalWindowRect.width, externalWindowRect.height};
}

void ofxOceanodeNodeModelExternalWindow::presetRecallBeforeSettingParameters(ofJson &json){
    if(json.count("ExtWindowRect") == 1){
        auto infoVec = json["ExtWindowRect"];
        externalWindowRect = ofRectangle(infoVec[0], infoVec[1], infoVec[2], infoVec[3]);
    }
}

