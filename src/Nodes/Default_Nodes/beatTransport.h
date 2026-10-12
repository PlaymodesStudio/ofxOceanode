#pragma once
#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeTransport.h"

#include <algorithm>
#include <cfloat>
#include <climits>
#include <cmath>

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

    void update(ofEventArgs &) override {
        const auto frameState = getFrameTransportState();
        const auto &previous = frameState.previous;
        const auto &current = frameState.current;
        const auto transportState = getTransportState();

        beatOut = static_cast<float>(current.beatPosition);
        prevBeatOut = static_cast<float>(previous.beatPosition);
        deltaBeatOut = static_cast<float>(current.beatPosition - previous.beatPosition);
        beatPhaseOut = static_cast<float>(ofxOceanodeTransportUtils::wrapPhase(current.beatPosition, 1.0));
        bpmOut = current.bpm;
        playingOut = current.isPlaying ? 1 : 0;

        double beatsPerBar = 4.0;
        bool loopEnabled = false;
        double loopStart = 0.0;
        double loopEnd = 0.0;
        if(auto* container = getHostContainer()) {
            const auto& timeline = container->getTimelineManager();
            beatsPerBar = std::max(0.25, timeline.getBeatsPerBar());
            loopEnabled = timeline.isLoopEnabled();
            loopStart = timeline.getLoopStartBeat();
            loopEnd = timeline.getLoopEndBeat();
        }

        const double nonNegativeBeat = std::max(0.0, current.beatPosition);
        barOut = static_cast<int>(std::min<double>(INT_MAX, std::floor(nonNegativeBeat / beatsPerBar) + 1.0));
        beatInBarOut = static_cast<float>(std::fmod(nonNegativeBeat, beatsPerBar));
        loopingOut = loopEnabled ? 1 : 0;
        if(loopEnabled && loopEnd > loopStart) {
            const double loopLength = loopEnd - loopStart;
            double relativeBeat = current.beatPosition - loopStart;
            relativeBeat = std::fmod(relativeBeat, loopLength);
            if(relativeBeat < 0.0) relativeBeat += loopLength;
            loopBeatOut = static_cast<float>(relativeBeat);

            const double phaseBeat = transportState.beatPosition - loopStart;
            loopPhaseOut = static_cast<float>(ofClamp(phaseBeat / loopLength, 0.0, 1.0));
        } else {
            loopBeatOut = static_cast<float>(nonNegativeBeat);
            loopPhaseOut = 0.0f;
        }

        const bool startedFromZero = current.isPlaying && current.beatPosition <= kBeatEpsilon &&
            ((!previous.isPlaying && current.isPlaying) || current.generation != previous.generation);
        if(startedFromZero) {
            resetOut.trigger();
        }

        if(auto* container = getHostContainer()) {
            if(container->getTimelineManager().didLoopWrapThisFrame()) {
                loopResetOut.trigger();
            }
        }
    }

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
