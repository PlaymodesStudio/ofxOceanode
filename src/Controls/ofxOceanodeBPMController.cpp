//
//  ofxOceanodeBPMController.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 13/03/2018.
//

#include "ofxOceanodeBPMController.h"

//DatGui

ofxOceanodeBPMController::ofxOceanodeBPMController() : ofxOceanodeBaseController("BPM"){
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
    
    gui->addSlider(bpm.set("BPM", 120, 0, 999));
    
    gui->setVisible(false);
    
    //ControlGui Events
//    gui->onDropdownEvent(this, &ofxOceanodePresetsController::onGuiDropdownEvent);
//    gui->onScrollViewEvent(this, &ofxOceanodePresetsController::onGuiScrollViewEvent);
//    gui->onTextInputEvent(this, &ofxOceanodePresetsController::onGuiTextInputEvent);
    
}

void ofxOceanodeBPMController::draw(){
    if(isActive)
        gui->draw();
}

void ofxOceanodeBPMController::update(){
    if(isActive)
        gui->update();
}


void ofxOceanodeBPMController::activate(){
    ofxOceanodeBaseController::activate();
    gui->setVisible(true);
}

void ofxOceanodeBPMController::deactivate(){
    ofxOceanodeBaseController::deactivate();
    gui->setVisible(false);
}

void ofxOceanodeBPMController::windowResized(ofResizeEventArgs &a){
    int layoutHeight = ofGetWidth()/15;
    mainGuiTheme->font.size = ofGetWidth()/40;
    mainGuiTheme->layout.height = layoutHeight;
    mainGuiTheme->layout.width = ofGetWidth();
    mainGuiTheme->init();
    
    gui->setTheme(mainGuiTheme);
    gui->setWidth(ofGetWidth());
}
