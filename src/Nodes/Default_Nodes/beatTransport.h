#pragma once
#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeTransport.h"

#include <cfloat>
#include <climits>

class beatTransport : public ofxOceanodeNodeModel {
public:
    beatTransport() : ofxOceanodeNodeModel("Beat Transport") {
        description = "Read-only status from Oceanode's shared transport and current timeline.";
    }

    void setup() override {
        addSeparator("Transport", ofColor(200));
        addOutputParameter(beatOut.set("Beat Transport", 0.0f, 0.0f, FLT_MAX));
        addOutputParameter(prevBeatOut.set("Prev Beat", 0.0f, 0.0f, FLT_MAX));
        addOutputParameter(deltaBeatOut.set("Delta Beat", 0.0f, -FLT_MAX, FLT_MAX));
        addOutputParameter(beatPhaseOut.set("Beat Phase", 0.0f, 0.0f, 1.0f));
        addOutputParameter(bpmOut.set("BPM", 120.0f, 0.0f, 400.0f));
        addOutputParameter(playingOut.set("Playing", 1, 0, 1));
        addOutputParameter(barOut.set("Bar", 1, 1, INT_MAX));
        addOutputParameter(beatInBarOut.set("Beat In Bar", 0.0f, 0.0f, 64.0f));
        addOutputParameter(loopingOut.set("Looping", 0, 0, 1));
        addOutputParameter(loopBeatOut.set("Loop Beat", 0.0f, 0.0f, FLT_MAX));
        addOutputParameter(loopPhaseOut.set("Loop Phase", 0.0f, 0.0f, 1.0f));
        addOutputParameter(resetOut.set("Reset"));
        addOutputParameter(loopResetOut.set("Loop Reset"));
    }

    void update(ofEventArgs &) override;

private:
    static constexpr double kBeatEpsilon = 1e-6;

    ofParameter<float> beatOut;
    ofParameter<float> prevBeatOut;
    ofParameter<float> deltaBeatOut;
    ofParameter<float> beatPhaseOut;
    ofParameter<float> bpmOut;
    ofParameter<int> playingOut;
    ofParameter<int> barOut;
    ofParameter<float> beatInBarOut;
    ofParameter<int> loopingOut;
    ofParameter<float> loopBeatOut;
    ofParameter<float> loopPhaseOut;
    ofParameter<void> resetOut;
    ofParameter<void> loopResetOut;
};
