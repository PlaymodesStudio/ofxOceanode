//
//  ofxOceanodeBPMController.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 13/03/2018.
//

#include "ofxOceanodeBPMController.h"
#include "ofxOceanodeContainer.h"
#include <numeric>

//DatGui

ofxOceanodeBPMController::ofxOceanodeBPMController(shared_ptr<ofxOceanodeContainer> _container) : container(_container), ofxOceanodeBaseController("BPM"){
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
    gui->addButton("Tap Tempo");
    container->setBpm(bpm);
    
    gui->setVisible(false);
    
    //ControlGui Events
    bpmListener = bpm.newListener([&](float &bpm){
        container->setBpm(bpm);
    });
    
    gui->onButtonEvent(this, &ofxOceanodeBPMController::tapTempoPress);
    lastButtonPressTime = -1;
}

void ofxOceanodeBPMController::tapTempoPress(ofxDatGuiButtonEvent e){
    if(lastButtonPressTime == -1){
        lastButtonPressTime = ofGetElapsedTimef();
    }else{
        float currentTime = ofGetElapsedTimef();
        float lastInterval = currentTime - lastButtonPressTime;
        lastButtonPressTime = currentTime;
        storedIntervals.push_back(lastInterval);
        if(storedIntervals.size() == 1){
            averageInterval = lastInterval;
        }
        else{
            averageInterval = (float)std::accumulate(storedIntervals.begin(), storedIntervals.end(), 0.0) / (float)storedIntervals.size();
            if(lastInterval > averageInterval*2 || lastInterval < averageInterval/2){
                storedIntervals.clear();
            }
            else{
                bpm = 60.0/averageInterval;
            }
        }
    }
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
