#pragma once
#include <vector>

class ofxOceanodeNodeModel;
class ofxOceanodeNodeMacro;
class ofxOceanodeAbstractParameter;

// What moving a patch from one state to another may touch, and how a value
// moves between two states. The MacroMixer's morph and anything else that
// interpolates between states of a patch (the Timeline's Project Track) take
// their rules from here, so they agree on both.
// Defined in MacroMixerSource.cpp.
namespace ofxOceanodeMorph {

// Whether a parameter is a control of the patch: a value the user sets, not
// one something else produces or that is the node's configuration. False for
//  - every parameter of a Portal node (its sender decides the value);
//  - a node's window toggle;
//  - a saved key the node morphs itself (morphKeys().blended);
//  - MorphHint::Skip and MorphHint::Structural;
//  - a parameter with an incoming cable, read-only, not saved with a preset
//    or that takes no cable;
//  - on a router: anything but Value, a router left out of snapshots, and a
//    router whose pin on the owning Macro has an incoming cable.
// owner: the Macro whose canvas holds the node; nullptr on a canvas no Macro owns.
bool isControl(ofxOceanodeNodeModel& model, ofxOceanodeAbstractParameter& p, ofxOceanodeNodeMacro* owner);

// How a control moves between two states.
struct Motion {
    bool discrete = false;    // switches: booleans, strings, dropdowns, MorphHint::Switch, types that cannot blend
    bool logarithmic = false; // MorphHint::Log: glides geometrically while every value is positive
};
Motion motionOf(ofxOceanodeNodeModel& model, ofxOceanodeAbstractParameter& p);

// A numeric control's value at t (0: a, 1: b). Lists blend by the default
// vector policy (Resample / changing length), so lists of different lengths
// glide into each other; integers round.
std::vector<float> blend(const std::vector<float>& a, const std::vector<float>& b, float t, bool logarithmic = false);
std::vector<int> blend(const std::vector<int>& a, const std::vector<int>& b, float t);

}
