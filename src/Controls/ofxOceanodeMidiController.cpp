//
//  ofxOceanodeMidiController.cpp
//  example-midi
//
//  Created by Eduard Frigola Bagué on 19/08/2018.
//

#include "ofxOceanodeMidiController.h"
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeMidiBinding.h"

ofxOceanodeMidiController::ofxOceanodeMidiController(shared_ptr<ofxOceanodePresetsController> _presetsController, shared_ptr<ofxOceanodeContainer> _container) : ofxOceanodeBaseController(_container, "MIDI"){
    midiLearn = gui->addToggle("Midi Learn", false);
    midiDevices = container->getMidiDevices();
    presetsControl.setPresetsController(_presetsController);
    
    gui->addLabel("Presets");
    gui->addDropdown("Midi Device", midiDevices);
    gui->addSlider(presetsControl.getChannel());
    gui->addDropdown("Type", {"Note On", "Control Change", "Program Change"});
    
    gui->addLabel("===========Bindings===========");
    
    midiLearn->onToggleEvent(this, &ofxOceanodeMidiController::midiLearnPressed);
    
    gui->onDropdownEvent(this, &ofxOceanodeMidiController::onGuiDropdownEvent);
    gui->onTextInputEvent(this, &ofxOceanodeMidiController::onGuiTextInputEvent);
    
    container->addNewMidiMessageListener(&presetsControl);
    
    listeners.push(container->midiBindingCreated.newListener(this, &ofxOceanodeMidiController::newParameterBinding));
    listeners.push(container->midiBindingDestroyed.newListener(this, &ofxOceanodeMidiController::removeParameterBinding));
}


void ofxOceanodeMidiController::midiLearnPressed(ofxDatGuiToggleEvent e){
    container->setIsListeningMidi(e.checked);
}

void ofxOceanodeMidiController::newParameterBinding(ofxOceanodeAbstractMidiBinding &binding){
    ofLog() << "MidiBind Created " << binding.getName();
    for(auto folder : folders){
        folder.second->collapse();
    }
    folders[binding.getName()] = gui->addFolder(binding.getName());
    auto folder = folders[binding.getName()];
    auto textInput = folder->addTextInput("MessageType", binding.getMessageType());
    listeners.push(binding.getMessageType().newListener([&, textInput](string &val){
        textInput->setText(val);
    }));
    
    textInput = folder->addTextInput("Channel", ofToString(binding.getChannel()));
    listeners.push(binding.getChannel().newListener([&, textInput](int &val){
        textInput->setText(ofToString(val));
    }));
    
    textInput = folder->addTextInput("Control", ofToString(binding.getControl()));
    listeners.push(binding.getControl().newListener([&, textInput](int &val){
        textInput->setText(ofToString(val));
    }));
    
    folder->addSlider(binding.getValue());
    
    if(binding.type() == typeid(ofxOceanodeMidiBinding<float>).name()){
        auto &midiBindingCasted = static_cast<ofxOceanodeMidiBinding<float> &>(binding);
        folder->addSlider(midiBindingCasted.getMinParameter())->setPrecision(1000);
        folder->addSlider(midiBindingCasted.getMaxParameter())->setPrecision(1000);
    }
    else if(binding.type() == typeid(ofxOceanodeMidiBinding<int>).name()){
        auto &midiBindingCasted = static_cast<ofxOceanodeMidiBinding<int> &>(binding);
        folder->addSlider(midiBindingCasted.getMinParameter())->setPrecision(1000);
        folder->addSlider(midiBindingCasted.getMaxParameter())->setPrecision(1000);
    }
    else if(binding.type() == typeid(ofxOceanodeMidiBinding<vector<float>>).name()){
        auto &midiBindingCasted = static_cast<ofxOceanodeMidiBinding<vector<float>> &>(binding);
        folder->addSlider(midiBindingCasted.getMinParameter())->setPrecision(1000);
        folder->addSlider(midiBindingCasted.getMaxParameter())->setPrecision(1000);
    }
    else if(binding.type() == typeid(ofxOceanodeMidiBinding<vector<int>>).name()){
        auto &midiBindingCasted = static_cast<ofxOceanodeMidiBinding<vector<int>> &>(binding);
        folder->addSlider(midiBindingCasted.getMinParameter())->setPrecision(1000);
        folder->addSlider(midiBindingCasted.getMaxParameter())->setPrecision(1000);
    }
    folder->expand();
}

void ofxOceanodeMidiController::removeParameterBinding(ofxOceanodeAbstractMidiBinding &binding){
    gui->removeComponent(folders[binding.getName()]->getName());
    folders.erase(binding.getName());
}

void ofxOceanodeMidiController::onGuiDropdownEvent(ofxDatGuiDropdownEvent e){
    if(e.target->getName() == "Midi Device"){
        presetsControl.setMidiDevice(e.target->getSelected()->getName());
    }
    else if(e.target->getName() == "Type"){
        presetsControl.setType(e.target->getSelected()->getName());
    }
}

void ofxOceanodeMidiController::onGuiTextInputEvent(ofxDatGuiTextInputEvent e){
    
}
