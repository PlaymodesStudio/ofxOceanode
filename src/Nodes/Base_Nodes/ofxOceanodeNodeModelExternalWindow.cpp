//
//  ofxOceanodeNodeModelExternalWindow.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 23/03/2018.
//

#include "ofxOceanodeNodeModelExternalWindow.h"

ofxOceanodeNodeModelExternalWindow::ofxOceanodeNodeModelExternalWindow(string name)  : ofxOceanodeNodeModel(name){
    auto &info = addParameterToGroupAndInfo(showWindow.set("Show Window", true));
    info.acceptInConnection = false;
    info.acceptOutConnection = false;
    info.isSavePreset = false;
    windowListenerEvents.push(showWindow.newListener(this, &ofxOceanodeNodeModelExternalWindow::showExternalWindow));
    externalWindowRect.setPosition(-1, -1);
    externalWindow = nullptr;
    
}

ofxOceanodeNodeModelExternalWindow::~ofxOceanodeNodeModelExternalWindow(){
    if(externalWindow != nullptr){
        externalWindow->setWindowShouldClose();
    }
}

void ofxOceanodeNodeModelExternalWindow::setExternalWindowPosition(int px, int py)
{
    if(externalWindow != nullptr) externalWindow->setWindowPosition(px,py);
}

void ofxOceanodeNodeModelExternalWindow::setExternalWindowShape(int w, int h)
{
    if(externalWindow != nullptr) externalWindow->setWindowShape(w,h);
}
void ofxOceanodeNodeModelExternalWindow::setExternalWindowFullScreen(bool b)
{
    if(b && (externalWindow != nullptr)) externalWindow->setFullscreen(b);
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
        prevSettings.monitor = 1;
        externalWindow = ofCreateWindow(prevSettings);
        externalWindow->setWindowTitle(nameIdentifier + " " + ofToString(numIdentifier));
        externalWindow->setVerticalSync(true);
        windowListenerEvents.push(externalWindow->events().draw.newListener(this, &ofxOceanodeNodeModelExternalWindow::drawInExternalWindow));
        windowListenerEvents.push(externalWindow->events().update.newListener(this, &ofxOceanodeNodeModelExternalWindow::updateForExternalWindow));
        windowListenerEvents.push(externalWindow->events().exit.newListener(this, &ofxOceanodeNodeModelExternalWindow::closeExternalWindow));
        windowListenerEvents.push(externalWindow->events().keyPressed.newListener(this, &ofxOceanodeNodeModelExternalWindow::keyPressed));
        windowListenerEvents.push(externalWindow->events().keyReleased.newListener(this, &ofxOceanodeNodeModelExternalWindow::keyReleased));
        windowListenerEvents.push(externalWindow->events().mouseMoved.newListener(this, &ofxOceanodeNodeModelExternalWindow::mouseMoved));
        windowListenerEvents.push(externalWindow->events().mousePressed.newListener(this, &ofxOceanodeNodeModelExternalWindow::mousePressed));
        windowListenerEvents.push(externalWindow->events().mouseReleased.newListener(this, &ofxOceanodeNodeModelExternalWindow::mouseReleased));
        windowListenerEvents.push(externalWindow->events().mouseDragged.newListener(this, &ofxOceanodeNodeModelExternalWindow::mouseDragged));
        windowListenerEvents.push(externalWindow->events().mouseScrolled.newListener(this, &ofxOceanodeNodeModelExternalWindow::mouseScrolled));
        windowListenerEvents.push(externalWindow->events().mouseEntered.newListener(this, &ofxOceanodeNodeModelExternalWindow::mouseEntered));
        windowListenerEvents.push(externalWindow->events().mouseExited.newListener(this, &ofxOceanodeNodeModelExternalWindow::mouseExited));
        windowListenerEvents.push(externalWindow->events().windowResized.newListener(this, &ofxOceanodeNodeModelExternalWindow::windowResizedOwnEvent));
        windowListenerEvents.push(externalWindow->events().fileDragEvent.newListener(this, &ofxOceanodeNodeModelExternalWindow::dragEvent));
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

