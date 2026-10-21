//
//  beatTransport.cpp
//  ofxOceanode
//

#include "beatTransport.h"
#include "ofxOceanodeContainer.h"

#include <algorithm>
#include <cmath>

void beatTransport::update(ofEventArgs &) {
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
    const bool loopEnabled = current.loopEnabled;
    const double loopStart = current.loopStartBeat;
    const double loopEnd = current.loopEndBeat;
    if(auto* container = getHostContainer()) {
        if(auto* extension = container->getAutomationExtension())
            beatsPerBar = std::max(0.25, extension->getBeatsPerBar());
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

    if(current.loopCount != previous.loopCount) loopResetOut.trigger();
}
