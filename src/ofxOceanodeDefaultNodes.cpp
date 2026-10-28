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
    nodeRegistry->registerModel<oscillator>("Generators");
    nodeRegistry->registerModel<chaoticOscillator>("Generators");
    nodeRegistry->registerModel<phasor>("Generators");
#ifdef OFXOCEANODE_USE_MIDI
    nodeRegistry->registerModel<midiClockNode>("Generators");
#endif
    nodeRegistry->registerModel<simpleNumberGenerator>("Generators");
    nodeRegistry->registerModel<simpleNormalizedNumberGenerator>("Generators");
    nodeRegistry->registerModel<counter>("Generators");
    nodeRegistry->registerModel<ramp>("Generators");
    nodeRegistry->registerModel<mapper>("Modifiers");
    nodeRegistry->registerModel<ranger>("Modifiers");
    nodeRegistry->registerModel<indexer>("Generators");
    nodeRegistry->registerModel<reindexer>("Modifiers");
    nodeRegistry->registerModel<smoother>("Modifiers");
    nodeRegistry->registerModel<switcher>("Modifiers");
    nodeRegistry->registerModel<curve>("Modifiers");
	//nodeRegistry->registerModel<curve2>("Modifiers");
    nodeRegistry->registerModel<ofxOceanodeNodeMacro>("MACRO");
    nodeRegistry->registerModel<noise>("Generators");
    nodeRegistry->registerModel<randomGenerator>("Generators");
    nodeRegistry->registerModel<beatTransport>("Timeline");
    nodeRegistry->registerModel<presetLoadTrigger>("General");
    nodeRegistry->registerModel<globalPitch>("General");

    nodeRegistry->registerModel<ofxOceanodeMacroMixer>("Experimental");
}
