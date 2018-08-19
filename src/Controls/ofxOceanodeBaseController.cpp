//
//  ofxOceanodeBaseController.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 12/03/2018.
//

#include "ofxOceanodeBaseController.h"

ofxOceanodeBaseController::ofxOceanodeBaseController(shared_ptr<ofxOceanodeContainer> _container, string name) : container(_container), controllerName(name){
    isActive = false;
    button.setHighlight(false);
    button.setColor(ofColor(0, 0,0));
    button.setName(name);
    
    
    //GUI
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
    
    gui->setVisible(false);
}

void ofxOceanodeBaseController::draw(){
    if(isActive)
        gui->draw();
}

void ofxOceanodeBaseController::update(){
    if(isActive)
        gui->update();
}

void ofxOceanodeBaseController::activate(){
    isActive = true;
    button.setHighlight(true);
    gui->setVisible(true);
}

void ofxOceanodeBaseController::deactivate(){
    isActive = false;
    button.setHighlight(false);
    gui->setVisible(false);
}

void ofxOceanodeBaseController::windowResized(ofResizeEventArgs &a){
    int layoutHeight = ofGetWidth()/15;
    mainGuiTheme->font.size = ofGetWidth()/40;
    mainGuiTheme->layout.height = layoutHeight;
    mainGuiTheme->layout.width = ofGetWidth();
    mainGuiTheme->init();
    
    gui->setTheme(mainGuiTheme);
    gui->setWidth(ofGetWidth());
}
