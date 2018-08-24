//
//  ofxOceanodeMidiController.cpp
//  example-midi
//
//  Created by Eduard Frigola Bagué on 19/08/2018.
//

#include "ofxOceanodeMidiController.h"
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeMidiBinding.h"

ofxOceanodeMidiController::ofxOceanodeMidiController(shared_ptr<ofxOceanodeContainer> _container) : ofxOceanodeBaseController(_container, "MIDI"){
    midiLearn = gui->addToggle("Midi Learn", false);
    
    midiLearn->onToggleEvent(this, &ofxOceanodeMidiController::midiLearnPressed);
    
    
    listeners.push(container->midiBindingCreated.newListener(this, &ofxOceanodeMidiController::newParameterBinding));
    listeners.push(container->midiBindingDestroyed.newListener(this, &ofxOceanodeMidiController::removeParameterBinding));
}


void ofxOceanodeMidiController::midiLearnPressed(ofxDatGuiToggleEvent e){
    container->setIsListeningMidi(e.checked);
}

void ofxOceanodeMidiController::newParameterBinding(ofxOceanodeAbstractMidiBinding &binding){
    ofLog() << "MidiBind Created " << binding.getParameterName();
    label = gui->addLabel(binding.getParameterName());
//    control = gui->addTextInput(control)
    if(binding.type() == typeid(ofxOceanodeMidiBinding<float>).name()){
        auto &midiBindingCasted = static_cast<ofxOceanodeMidiBinding<float> &>(binding);
        min = gui->addSlider(midiBindingCasted.getMinParameter());
        max = gui->addSlider(midiBindingCasted.getMaxParameter());
    }
}

void ofxOceanodeMidiController::removeParameterBinding(ofxOceanodeAbstractMidiBinding &binding){
    ofLog() << "MidiBind Removed " << binding.getParameterName();
    
}
