#pragma once
#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeTransport.h"

#include <cfloat>
#include <climits>

class beatTransport : public ofxOceanodeNodeModel {
public:
    beatTransport() : ofxOceanodeNodeModel("Beat Transport") {
        description = "Status of the timeline chosen as Time Source in the inspector (Auto: the one this canvas plays), "
                      "and Play, Stop and Seek for it, optionally quantized to the next beat or bar.";
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
        addSeparator("Control", ofColor(200));
        addParameter(playIn.set("Play"));
        addParameter(stopIn.set("Stop"));
        addParameter(seekIn.set("Seek", 0.0f, 0.0f, FLT_MAX));
        addParameterDropdown(quantize, "Quantize", 0, {"Off", "Beat", "Bar"});
        addTimeSourceParameter();
        listeners.push(playIn.newListener([this](){ act(Action::Play); }));
        listeners.push(stopIn.newListener([this](){ act(Action::Stop); }));
        listeners.push(seekIn.newListener([this](float &){ act(Action::Seek); }));
    }

    void update(ofEventArgs &) override;

private:
    enum class Action { Play, Stop, Seek };
    // Acts on the followed timeline, at the next grid instant when quantized.
    void act(Action action);
    ofEventListeners listeners;
    ofParameter<void> playIn;
    ofParameter<void> stopIn;
    ofParameter<float> seekIn;
    ofParameter<int> quantize;

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
