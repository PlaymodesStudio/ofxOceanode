//
//  ofxOceanodeMidiBinding.h
//  example-midi
//
//  Created by Eduard Frigola Bagué on 20/08/2018.
//

#ifndef ofxOceanodeMidiBinding_h
#define ofxOceanodeMidiBinding_h

#ifdef OFXOCEANODE_USE_MIDI

#include "ofParameter.h"
#include "ofxMidi.h"

using namespace std;

class ofxOceanodeAbstractMidiBinding : public ofxMidiListener{
public:
    ofxOceanodeAbstractMidiBinding(){
        isListening = true;
    };
    ~ofxOceanodeAbstractMidiBinding(){};
    
    void newMidiMessage(ofxMidiMessage& message){
        if(isListening){
            firstMidiMessage = message;
            portName = firstMidiMessage.portName;
            midiChannel = firstMidiMessage.channel;
            isListening = false;
            unregisterUnusedMidiIns.notify(this, portName);
        }
    }
    
    string getParameterName(){return parameterEscapedName;};
    
    virtual ofxMidiMessage& sendMidiMessage(){};
    virtual void bindParameter(){};
    
    ofEvent<string> unregisterUnusedMidiIns;
    ofEvent<ofxMidiMessage> midiMessageSender;
protected:
    bool isListening;
    
    std::string portName;
    int midiChannel;
    
    string parameterEscapedName;
    ofxMidiMessage firstMidiMessage;
};

template<typename T>
class ofxOceanodeMidiBinding : public ofxOceanodeAbstractMidiBinding{
public:
    ofxOceanodeMidiBinding(ofParameter<T>& _parameter) : parameter(_parameter), ofxOceanodeAbstractMidiBinding(){
        parameterEscapedName = parameter.getEscapedName();
    }
    
    ~ofxOceanodeMidiBinding(){};
    
    void newMidiMessage(ofxMidiMessage& message){};
    void bindParameter(){};
    
private:
    ofEventListener listener;
    ofParameter<T>& parameter;
    
    std::mutex modifiyingParameter;
};

#endif

#endif /* ofxOceanodeMidiBinding_h */
