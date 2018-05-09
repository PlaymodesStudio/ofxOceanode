//
//  ofxOceanodeOSCController.cpp
//  PLedNodes
//
//  Created by Eduard Frigola on 09/05/2018.
//

#ifdef OFXOCEANODE_USE_OSC

#include "ofxOceanodeOSCController.h"
#include "ofxOceanodeContainer.h"

ofxOceanodeOSCController::ofxOceanodeOSCController(shared_ptr<ofxOceanodeContainer> _container) : container(_container), ofxOceanodeBaseController("OSC"){
    ofxDatGuiLog::quiet();
    ofxDatGui::setAssetPath("");
    
    mainGuiTheme = new ofxDatGuiThemeCharcoal;
    ofColor randColor =  ofColor::indianRed;
    mainGuiTheme->color.slider.fill = randColor;
    mainGuiTheme->color.textInput.text = randColor;
    mainGuiTheme->color.icons = randColor;
    int layoutHeight = ofGetWidth()/15;
    mainGuiTheme->font.size = ofGetWidth()/40;
    mainGuiTheme->layout.height = layoutHeight;
    mainGuiTheme->layout.width = ofGetWidth();
    mainGuiTheme->init();
    
    gui = new ofxDatGui();
    gui->setTheme(mainGuiTheme);
    gui->setPosition(0, 30);
    gui->setWidth(ofGetWidth());
    gui->setAutoDraw(false);
    
    //host = gui->addTextInput("HOST", "127.0.0.1");
    portIn = gui->addTextInput("Port IN", "12345");
    //portOut = gui->addTextInput("Port OUT", "54321");
    
    //container->setupOscSender(host->getText(), ofToInt(portOut->getText()));
    container->setupOscReceiver(ofToInt(portIn->getText()));
    
    gui->onTextInputEvent(this, &ofxOceanodeOSCController::onGuiTextInputEvent);
    
    gui->setVisible(false);
}

void ofxOceanodeOSCController::onGuiTextInputEvent(ofxDatGuiTextInputEvent e){
    if(e.target == host || e.target == portOut){
        //container->setupOscSender(host->getText(), ofToInt(portOut->getText()));
    }else if(e.target == portIn){
        container->setupOscReceiver(ofToInt(portIn->getText()));
    }
}

void ofxOceanodeOSCController::draw(){
    if(isActive)
        gui->draw();
}

void ofxOceanodeOSCController::update(){
    if(isActive)
        gui->update();
}


void ofxOceanodeOSCController::activate(){
    ofxOceanodeBaseController::activate();
    gui->setVisible(true);
}

void ofxOceanodeOSCController::deactivate(){
    ofxOceanodeBaseController::deactivate();
    gui->setVisible(false);
}

void ofxOceanodeOSCController::windowResized(ofResizeEventArgs &a){
    int layoutHeight = ofGetWidth()/15;
    mainGuiTheme->font.size = ofGetWidth()/40;
    mainGuiTheme->layout.height = layoutHeight;
    mainGuiTheme->layout.width = ofGetWidth();
    mainGuiTheme->init();
    
    gui->setTheme(mainGuiTheme);
    gui->setWidth(ofGetWidth());
}

#endif
