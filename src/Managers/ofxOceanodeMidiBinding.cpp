//
//  ofxOceanodeMidiBinding.cpp
//  example-midi
//
//  Created by Eduard Frigola Bagué on 20/08/2018.
//

#include "ofxOceanodeMidiBinding.h"

template<>
void ofxOceanodeMidiBinding<pair<int, bool>>::newMidiMessage(ofxMidiMessage& message){
    if(message.status == MIDI_NOTE_OFF){
        message.status = MIDI_NOTE_ON;
        message.velocity = 0;
    }
    if(isListening) ofxOceanodeAbstractMidiBinding::newMidiMessage(message);
    if(message.channel == channel && message.status == status){
        bool validMessage = false;
        switch(status){
            case MIDI_NOTE_ON:
            {
                if(message.pitch >= control){
                    value = message.velocity;
                    validMessage = true;
                    
                }
                break;
            }
            default:
            {
                ofLog() << "Midi Type " << ofxMidiMessage::getStatusString(message.status) << " not supported for parameter of type pair<int, bool>";
            }
        }
        if(validMessage){
            modifiyingParameter = true;
            parameter.set(make_pair<int, bool>(message.pitch - control, value != 0 ? true : false));
            modifiyingParameter = false;
        }
    }
}

template<>
void ofxOceanodeMidiBinding<pair<int, bool>>::bindParameter(){
    listener = parameter.newListener([this](pair<int, bool> &f){
        if(!modifiyingParameter){
            value = f.second ? 127 : 0;
            ofxMidiMessage message;
            message.status = status;
            message.channel = channel;
            switch(status){
                case MIDI_NOTE_ON:
                    message.pitch = control + f.first;
                    message.velocity = value;
                    break;
                default:
                    ofLog() << "Midi Type " << ofxMidiMessage::getStatusString(message.status) << " not supported for parameter of type pair<int, bool>";
            }
            midiMessageSender.notify(this, message);
        }
    });
}
