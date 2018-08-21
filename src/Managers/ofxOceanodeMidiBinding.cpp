//
//  ofxOceanodeMidiBinding.cpp
//  example-midi
//
//  Created by Eduard Frigola Bagué on 20/08/2018.
//

#include "ofxOceanodeMidiBinding.h"
#include "ofMath.h"


//Float
template<>
void ofxOceanodeMidiBinding<float>::newMidiMessage(ofxMidiMessage& message){
    ofxOceanodeAbstractMidiBinding::newMidiMessage(message);
    if(message.portName == firstMidiMessage.portName &&
       message.channel == firstMidiMessage.channel &&
       message.control == firstMidiMessage.control){
        modifiyingParameter = true;
        parameter.set(ofMap(message.value, 0, 127, parameter.getMin(), parameter.getMax()));
        modifiyingParameter = false;
    }
}

template<>
void ofxOceanodeMidiBinding<float>::bindParameter(){
    listener = parameter.newListener([this](float &f){
        if(!modifiyingParameter){
            ofxMidiMessage message;
            message.status = firstMidiMessage.status;
            message.channel = firstMidiMessage.channel;
            message.control = firstMidiMessage.control;
            message.value = ofMap(f, parameter.getMin(), parameter.getMax(), 0, 127);
            midiMessageSender.notify(this, message);
        }
    });
}

//Int
template<>
void ofxOceanodeMidiBinding<int>::newMidiMessage(ofxMidiMessage& message){
    ofxOceanodeAbstractMidiBinding::newMidiMessage(message);
    if(message.portName == firstMidiMessage.portName &&
       message.channel == firstMidiMessage.channel &&
       message.control == firstMidiMessage.control){
        modifiyingParameter = true;
        parameter.set(ofMap(message.value, 0, 127, parameter.getMin(), parameter.getMax()));
        modifiyingParameter = false;
    }
}

template<>
void ofxOceanodeMidiBinding<int>::bindParameter(){
    listener = parameter.newListener([this](int &i){
        if(!modifiyingParameter){
            ofxMidiMessage message;
            message.status = firstMidiMessage.status;
            message.channel = firstMidiMessage.channel;
            message.control = firstMidiMessage.control;
            message.value = ofMap(i, parameter.getMin(), parameter.getMax(), 0, 127);
            midiMessageSender.notify(this, message);
        }
    });
}

//vector<float>
template<>
void ofxOceanodeMidiBinding<vector<float>>::newMidiMessage(ofxMidiMessage& message){
    ofxOceanodeAbstractMidiBinding::newMidiMessage(message);
    if(message.portName == firstMidiMessage.portName &&
       message.channel == firstMidiMessage.channel &&
       message.control == firstMidiMessage.control){
        modifiyingParameter = true;
        parameter.set(vector<float>(1, ofMap(message.value, 0, 127, parameter.getMin()[0], parameter.getMax()[0])));
        modifiyingParameter = false;
    }
}

template<>
void ofxOceanodeMidiBinding<vector<float>>::bindParameter(){
    listener = parameter.newListener([this](vector<float> &vf){
        if(!modifiyingParameter && vf.size() == 1){
            ofxMidiMessage message;
            message.status = firstMidiMessage.status;
            message.channel = firstMidiMessage.channel;
            message.control = firstMidiMessage.control;
            message.value = ofMap(vf[0], parameter.getMin()[0], parameter.getMax()[0], 0, 127);
            midiMessageSender.notify(this, message);
        }
    });
}
