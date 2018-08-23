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
    
    virtual void newMidiMessage(ofxMidiMessage& message) {
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
    
    bool modifiyingParameter;
    ofEventListener listener;
};


template<typename T, typename Enable = void>
class ofxOceanodeMidiBinding : public ofxOceanodeAbstractMidiBinding{
public:
    ofxOceanodeMidiBinding(ofParameter<T>& _parameter) : parameter(_parameter), ofxOceanodeAbstractMidiBinding(){
        parameterEscapedName = parameter.getEscapedName();
    }
    
    ~ofxOceanodeMidiBinding(){};
    
    void newMidiMessage(ofxMidiMessage& message){};
    void bindParameter(){};
    
private:
    ofParameter<T>& parameter;
};

template<typename T>
class ofxOceanodeMidiBinding<T, typename std::enable_if<std::is_same<T, int>::value || std::is_same<T, float>::value>::type>: public ofxOceanodeAbstractMidiBinding{
public:
    ofxOceanodeMidiBinding(ofParameter<T>& _parameter) : parameter(_parameter), ofxOceanodeAbstractMidiBinding(){
        parameterEscapedName = parameter.getEscapedName();
    }

    ~ofxOceanodeMidiBinding(){};

    void newMidiMessage(ofxMidiMessage& message){
        ofxOceanodeAbstractMidiBinding::newMidiMessage(message);
        if(message.portName == firstMidiMessage.portName &&
           message.channel == firstMidiMessage.channel &&
           message.control == firstMidiMessage.control){
            modifiyingParameter = true;
            parameter.set(ofMap(message.value, 0, 127, parameter.getMin(), parameter.getMax()));
            modifiyingParameter = false;
        }
    };
    void bindParameter(){
        listener = parameter.newListener([this](T &f){
            if(!modifiyingParameter){
                ofxMidiMessage message;
                message.status = firstMidiMessage.status;
                message.channel = firstMidiMessage.channel;
                message.control = firstMidiMessage.control;
                message.value = ofMap(f, parameter.getMin(), parameter.getMax(), 0, 127);
                midiMessageSender.notify(this, message);
            }
        });
    };
    T getParameterMin(){return parameter.getMin();};
    T getParameterMax(){return parameter.getMax();};

private:
    ofParameter<T>& parameter;
};

template<typename>
struct is_std_vector2 : std::false_type {};

template<typename T, typename A>
struct is_std_vector2<std::vector<T,A>> : std::true_type {};

template<typename T>
class ofxOceanodeMidiBinding<vector<T>> : public ofxOceanodeAbstractMidiBinding{
public:
    ofxOceanodeMidiBinding(ofParameter<vector<T>>& _parameter) : parameter(_parameter), ofxOceanodeAbstractMidiBinding(){
        parameterEscapedName = parameter.getEscapedName();
    }
    
    ~ofxOceanodeMidiBinding(){};
    
    void newMidiMessage(ofxMidiMessage& message){
        ofxOceanodeAbstractMidiBinding::newMidiMessage(message);
        if(message.portName == firstMidiMessage.portName &&
           message.channel == firstMidiMessage.channel &&
           message.control == firstMidiMessage.control){
            modifiyingParameter = true;
            parameter.set(vector<T>(1, ofMap(message.value, 0, 127, parameter.getMin()[0], parameter.getMax()[0])));
            modifiyingParameter = false;
        }
    };
    
    void bindParameter(){
        listener = parameter.newListener([this](vector<T> &vf){
            if(!modifiyingParameter && vf.size() == 1){
                ofxMidiMessage message;
                message.status = firstMidiMessage.status;
                message.channel = firstMidiMessage.channel;
                message.control = firstMidiMessage.control;
                message.value = ofMap(vf[0], parameter.getMin()[0], parameter.getMax()[0], 0, 127);
                midiMessageSender.notify(this, message);
            }
        });
    };
    T getParameterMin(){return parameter.getMin()[0];};
    T getParameterMax(){return parameter.getMax()[0];};
    
private:
    ofParameter<vector<T>>& parameter;
};

#endif

#endif /* ofxOceanodeMidiBinding_h */
