//
//  lfoTrack.h
//  ofxOceanode
//
//  Reads the LFO clips of a timeline track and outputs them to the patch:
//  Value (the LFO as drawn), Phase (raw 0..1 ramp), Cycle (whole cycles from the
//  timeline start, continuous across clips and repeats) and Reset (on each wrap).
//  Everything is a pure function of the transport beat, so scrubbing and seeking
//  give exactly what playback gives. Patch it like a Phasor in Sync To Transport
//  mode: Phase -> Phase and Cycle -> Step of the random nodes.
//

#ifndef lfoTrack_h
#define lfoTrack_h

#include "ofxOceanodeNodeModel.h"

class lfoTrack : public ofxOceanodeNodeModel {
public:
    lfoTrack() : ofxOceanodeNodeModel("LFO Track") {}

    void setup() override;
    void update(ofEventArgs &a) override;

    void presetSave(ofJson &json) override;
    void presetRecallAfterSettingParameters(ofJson &json) override;

private:
    void refreshTrackList();

    ofParameter<int> trackSelector;
    std::shared_ptr<ofxOceanodeParameter<int>> trackSelectorParam;
    ofParameter<float> valueOut;
    ofParameter<float> phaseOut;
    ofParameter<float> cycleOut;
    ofParameter<void> resetOut;
    ofParameter<bool> activeOut;

    // The track is remembered by id (names can change, and the timeline loads
    // after node presets); the dropdown index is derived from it every frame.
    std::string selectedTrackId;
    std::vector<std::string> trackIds;   // index 0 = none
    std::vector<std::string> trackNames;
    bool updatingSelector = false;

    bool hasLastCycle = false;
    double lastCycle = 0.0;

    ofEventListeners listeners;
};

#endif /* lfoTrack_h */
