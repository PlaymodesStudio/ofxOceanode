//
//  midiClockNode.h
//  ofxOceanode
//
//  "MIDI Clock" node: reads an external MIDI clock and outputs its data (play
//  state, Start/Stop/Continue/Jump triggers, beat, 24-PPQ position, tempo). With
//  "Drive Transport" on it also becomes the global transport's clock, like the
//  built-in "Sync: MIDI Clock" setting. Only built with OFXOCEANODE_USE_MIDI.
//

#pragma once

#ifdef OFXOCEANODE_USE_MIDI

#include "ofxOceanodeNodeModel.h"
#include <memory>

class ofxOceanodeMidiClock;

class midiClockNode : public ofxOceanodeNodeModel {
public:
    midiClockNode();
    ~midiClockNode() override;

    void setup() override;
    void update(ofEventArgs &a) override;
    void presetSave(ofJson &json) override;
    void presetRecallBeforeSettingParameters(ofJson &json) override;

private:
    void refreshPorts(bool force);
    void applyPort();
    void applyDrive();

    std::unique_ptr<ofxOceanodeMidiClock> clock;

    // inputs
    ofParameter<int> portSelector;
    std::shared_ptr<ofxOceanodeParameter<int>> portSelectorParam;
    ofParameter<bool> enable;
    ofParameter<bool> driveTransport;
    ofParameter<bool> reaperMode;
    ofParameter<bool> clockOnly;
    ofParameter<bool> stopOnLoss;
    ofParameter<float> offsetMs;
    ofParameter<int> tempoWindow;
    ofParameter<int> bpmDecimals;

    // outputs
    ofParameter<bool> playingOut;
    ofParameter<void> startOut, stopOut, continueOut, jumpOut;
    ofParameter<float> beatOut, smoothBeatOut, bpmOut;
    ofParameter<int> ppqOut;
    ofParameter<bool> receivingOut;

    std::vector<std::string> portNames;  // index 0 = "None"
    std::string selectedPort;            // remembered by name (ports can come and go)
    bool updatingSelector = false;
    int framesToPortScan = 0;
    uint32_t lastStart = 0, lastStop = 0, lastContinue = 0, lastJump = 0;

    ofEventListeners listeners;
};

#endif // OFXOCEANODE_USE_MIDI
