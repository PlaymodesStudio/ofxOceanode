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
        parameter.set(ofMap(message.value, 0, 127, parameter.getMin(), parameter.getMax()));
    }
}
