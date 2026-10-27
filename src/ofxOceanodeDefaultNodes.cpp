//
//  ofxOceanodeDefaultNodes.cpp
//
//  The default nodes, registered by the ofxOceanode constructor.
//  Split from ofxOceanode.cpp so editing it does not compile every default
//  node again; the constructor runs the three parts in their old order.
//

#include "ofxOceanode.h"
#include "defaultNodes.h"
#include "ofxOceanodeMacroMixer.h"

void ofxOceanode::registerDefaultNodes(){
    //Register default nodes
    nodeRegistry->registerModel<oscillator>("Generators", ofxOceanodeNodeRegistrationFlags::CanRemoveWhenUnused);
    nodeRegistry->registerModel<chaoticOscillator>("Generators", ofxOceanodeNodeRegistrationFlags::CanRemoveWhenUnused);
    nodeRegistry->registerModel<phasor>("Generators", ofxOceanodeNodeRegistrationFlags::CanRemoveWhenUnused);
#ifdef OFXOCEANODE_USE_MIDI
    nodeRegistry->registerModel<midiClockNode>("Generators");
#endif
    nodeRegistry->registerModel<simpleNumberGenerator>("Generators", ofxOceanodeNodeRegistrationFlags::CanRemoveWhenUnused);
    nodeRegistry->registerModel<simpleNormalizedNumberGenerator>("Generators", ofxOceanodeNodeRegistrationFlags::CanRemoveWhenUnused);
    nodeRegistry->registerModel<counter>("Generators", ofxOceanodeNodeRegistrationFlags::CanRemoveWhenUnused);
    nodeRegistry->registerModel<ramp>("Generators", ofxOceanodeNodeRegistrationFlags::CanRemoveWhenUnused);
    nodeRegistry->registerModel<mapper>("Modifiers", ofxOceanodeNodeRegistrationFlags::CanRemoveWhenUnused);
    nodeRegistry->registerModel<ranger>("Modifiers", ofxOceanodeNodeRegistrationFlags::CanRemoveWhenUnused);
    nodeRegistry->registerModel<indexer>("Generators", ofxOceanodeNodeRegistrationFlags::CanRemoveWhenUnused);
    nodeRegistry->registerModel<reindexer>("Modifiers", ofxOceanodeNodeRegistrationFlags::CanRemoveWhenUnused);
    nodeRegistry->registerModel<smoother>("Modifiers", ofxOceanodeNodeRegistrationFlags::CanRemoveWhenUnused);
    nodeRegistry->registerModel<switcher>("Modifiers", ofxOceanodeNodeRegistrationFlags::CanRemoveWhenUnused);
    nodeRegistry->registerModel<curve>("Modifiers");
	//nodeRegistry->registerModel<curve2>("Modifiers");
    nodeRegistry->registerModel<ofxOceanodeNodeMacro>("MACRO");
    nodeRegistry->registerModel<noise>("Generators", ofxOceanodeNodeRegistrationFlags::CanRemoveWhenUnused);
    nodeRegistry->registerModel<randomGenerator>("Generators", ofxOceanodeNodeRegistrationFlags::CanRemoveWhenUnused);
    nodeRegistry->registerModel<beatTransport>("Timeline");
    nodeRegistry->registerModel<presetLoadTrigger>("General");
    nodeRegistry->registerModel<globalPitch>("General");

    nodeRegistry->registerModel<ofxOceanodeMacroMixer>("Experimental");
}
