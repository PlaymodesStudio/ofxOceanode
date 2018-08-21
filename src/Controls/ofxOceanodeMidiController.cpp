//
//  ofxOceanodeMidiController.cpp
//  example-midi
//
//  Created by Eduard Frigola Bagué on 19/08/2018.
//

#include "ofxOceanodeMidiController.h"
#include "ofxOceanodeContainer.h"


ofxOceanodeMidiController::ofxOceanodeMidiController(shared_ptr<ofxOceanodeContainer> _container) : ofxOceanodeBaseController(_container, "MIDI"){
    midiLearn = gui->addToggle("Midi Learn", false);
    
    midiLearn->onToggleEvent(this, &ofxOceanodeMidiController::midiLearnPressed);
}


void ofxOceanodeMidiController::midiLearnPressed(ofxDatGuiToggleEvent e){
    container->setIsListeningMidi(e.checked);
}
