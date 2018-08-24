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

class ofxOceanodeAbstractMidiBinding;

class ofxOceanodeMidiController : public ofxOceanodeBaseController{
public:
    ofxOceanodeMidiController(shared_ptr<ofxOceanodeContainer> _container);
    ~ofxOceanodeMidiController(){};
    
    void midiLearnPressed(ofxDatGuiToggleEvent e);
    
    void newParameterBinding(ofxOceanodeAbstractMidiBinding &midiBinding);
    void removeParameterBinding(ofxOceanodeAbstractMidiBinding &midiBinding);
    
private:
    ofxDatGuiToggle* midiLearn;
    ofEventListeners listeners;

    //midiBindingControls
    ofxDatGuiLabel* label;
    ofxDatGuiTextInput* control;
    ofxDatGuiTextInput* value;
    ofxDatGuiSlider* min;
    ofxDatGuiSlider* max;
};

#endif

#endif /* ofxOceanodeMidiController_h */
