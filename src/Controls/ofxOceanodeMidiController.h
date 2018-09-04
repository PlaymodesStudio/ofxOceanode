//
//  ofxOceanodeMidiController.h
//  example-midi
//
//  Created by Eduard Frigola Bagué on 19/08/2018.
//

#ifndef ofxOceanodeMidiController_h
#define ofxOceanodeMidiController_h

#ifdef OFXOCEANODE_USE_MIDI

#include "ofxOceanodeBaseController.h"
#include "ofxOceanodePresetsController.h"
#include "ofxMidi.h"

class ofxOceanodeAbstractMidiBinding;


class ofxOceanodePresetsMidiControl : public ofxMidiListener{
public:
    ofxOceanodePresetsMidiControl(){
        channel.set("Channel", 0, 0, 16);
        midiDevice == "-";
        type == "--";
    }
    ~ofxOceanodePresetsMidiControl(){};
    
    void newMidiMessage(ofxMidiMessage &message){
        if(message.portName == midiDevice && (message.channel == channel || channel == 0)){
            if(message.status == MIDI_NOTE_ON && type == "Note On"){
                if(message.velocity!=0) presetsController->loadPresetFromNumber(message.pitch);
            }else if(message.status == MIDI_CONTROL_CHANGE && type == "Control Change"){
                
            }else if(message.status == MIDI_PROGRAM_CHANGE && type == "Program Change"){
                
            }
        }
    }
    
    ofParameter<int> &getChannel(){return channel;};
    
    void setMidiDevice(string device){midiDevice = device;};
    void setType(string _type){type = _type;};
    void setPresetsController(shared_ptr<ofxOceanodePresetsController> _presetsController){presetsController = _presetsController;};
    
private:
    ofParameter<int> channel;
    string type;
    string midiDevice;
    shared_ptr<ofxOceanodePresetsController> presetsController;
};

class ofxOceanodeMidiController : public ofxOceanodeBaseController{
public:
    ofxOceanodeMidiController(shared_ptr<ofxOceanodePresetsController> presetsController, shared_ptr<ofxOceanodeContainer> _container);
    ~ofxOceanodeMidiController(){};
    
    void midiLearnPressed(ofxDatGuiToggleEvent e);
    
    void newParameterBinding(ofxOceanodeAbstractMidiBinding &midiBinding);
    void removeParameterBinding(ofxOceanodeAbstractMidiBinding &midiBinding);
    
    void onGuiDropdownEvent(ofxDatGuiDropdownEvent e);
    void onGuiTextInputEvent(ofxDatGuiTextInputEvent e);
    
private:
    ofxDatGuiToggle* midiLearn;
    ofEventListeners listeners;
    vector<string> midiDevices;

    //midiBindingControls
    map<string, ofxDatGuiFolder*> folders;
    
    ofxOceanodePresetsMidiControl presetsControl;
};

#endif

#endif /* ofxOceanodeMidiController_h */
