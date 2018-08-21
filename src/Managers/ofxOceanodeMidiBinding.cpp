//
//  ofxOceanodeMidiBinding.cpp
//  example-midi
//
//  Created by Eduard Frigola Bagué on 20/08/2018.
//

#include "ofxOceanodeMidiBinding.h"
#include "ofMath.h"

template<>
void ofxOceanodeMidiBinding<float>::newMidiMessage(ofxMidiMessage& message){
    ofxOceanodeAbstractMidiBinding::newMidiMessage(message);
    if(message.portName == firstMidiMessage.portName &&
       message.channel == firstMidiMessage.channel &&
       message.control == firstMidiMessage.control){
        modifiyingParameter.lock();
        parameter.set(ofMap(message.value, 0, 127, parameter.getMin(), parameter.getMax()));
        modifiyingParameter.unlock();
    }
}

template<>
void ofxOceanodeMidiBinding<float>::bindParameter(){
    listener = parameter.newListener([this](float &f){
        if(!modifiyingParameter.try_lock()){
            ofxMidiMessage message;
            message.status = firstMidiMessage.status;
            message.channel = firstMidiMessage.channel;
            message.control = firstMidiMessage.control;
            message.value = ofMap(f, parameter.getMin(), parameter.getMax(), 0, 127);
            midiMessageSender.notify(this, message);
        }
    });
    
}
