#include "ofxOceanodeTransport.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace {
double bpmToBeatsPerSecond(float bpm) {
    return static_cast<double>(std::max(0.0f, bpm)) / 60.0;
}
}

ofxOceanodeTransport::ofxOceanodeTransport() {
    const uint64_t nowUs = getSteadyNowUs();
    state.steadyTimeUs = nowUs;
    frameState.previous = state;
    frameState.current = state;
}

void ofxOceanodeTransport::setBpm(float bpm) {
    std::lock_guard<std::mutex> lock(mutex);
    if(externalOwner != nullptr) return; // following an external clock
    const uint64_t nowUs = getSteadyNowUs();
    advanceStateToNowLocked(nowUs);
    state.bpm = std::max(0.0f, bpm);
    state.steadyTimeUs = nowUs;
}

void ofxOceanodeTransport::setIsPlaying(bool isPlaying) {
    std::lock_guard<std::mutex> lock(mutex);
    if(externalOwner != nullptr) return; // following an external clock
    const uint64_t nowUs = getSteadyNowUs();
    advanceStateToNowLocked(nowUs);
    state.isPlaying = isPlaying;
    state.steadyTimeUs = nowUs;
}

void ofxOceanodeTransport::togglePlay() {
    std::lock_guard<std::mutex> lock(mutex);
    if(externalOwner != nullptr) return; // following an external clock
    const uint64_t nowUs = getSteadyNowUs();
    advanceStateToNowLocked(nowUs);
    state.isPlaying = !state.isPlaying;
    state.steadyTimeUs = nowUs;
}

void ofxOceanodeTransport::play() {
    setIsPlaying(true);
}

void ofxOceanodeTransport::pause() {
    setIsPlaying(false);
}

void ofxOceanodeTransport::stop() {
    std::lock_guard<std::mutex> lock(mutex);
    if(externalOwner != nullptr) return; // following an external clock
    const uint64_t nowUs = getSteadyNowUs();
    advanceStateToNowLocked(nowUs);
    state.isPlaying = false;
    setPositionLocked(0.0);
    state.generation++;
    state.steadyTimeUs = nowUs;
}

void ofxOceanodeTransport::setDriverMode(TransportDriverMode mode) {
    std::lock_guard<std::mutex> lock(mutex);
    const uint64_t nowUs = getSteadyNowUs();
    advanceStateToNowLocked(nowUs);
    state.driverMode = mode;
    state.steadyTimeUs = nowUs;
}

void ofxOceanodeTransport::syncRealTime() {
    std::lock_guard<std::mutex> lock(mutex);
    advanceStateToNowLocked(getSteadyNowUs());
}

void ofxOceanodeTransport::advanceFrameStep(double deltaSeconds) {
    std::lock_guard<std::mutex> lock(mutex);
    const uint64_t nowUs = getSteadyNowUs();
    advanceStateToNowLocked(nowUs);
    if(state.driverMode == TransportDriverMode::FrameStep && state.isPlaying) {
        const double seconds = std::max(0.0, deltaSeconds);
        advanceBeatsLocked(seconds * bpmToBeatsPerSecond(state.bpm), seconds);
    }
    state.steadyTimeUs = nowUs;
}

void ofxOceanodeTransport::seekToBeat(double beatPosition) {
    std::lock_guard<std::mutex> lock(mutex);
    if(externalOwner != nullptr) return; // following an external clock
    const uint64_t nowUs = getSteadyNowUs();
    advanceStateToNowLocked(nowUs);
    setPositionLocked(std::max(0.0, beatPosition));
    state.generation++;
    state.steadyTimeUs = nowUs;
}

void ofxOceanodeTransport::setLoop(bool enabled, double startBeat, double endBeat) {
    std::lock_guard<std::mutex> lock(mutex);
    if(state.loopEnabled == enabled && state.loopStartBeat == startBeat && state.loopEndBeat == endBeat) return;
    // Finish the motion under the old range before the new one applies.
    advanceStateToNowLocked(getSteadyNowUs());
    state.loopEnabled = enabled;
    state.loopStartBeat = std::max(0.0, startBeat);
    state.loopEndBeat = std::max(state.loopStartBeat, endBeat);
}

void ofxOceanodeTransport::setPositionLocked(double beat) {
    state.beatPosition = beat;
    const double beatsPerSecond = bpmToBeatsPerSecond(state.bpm);
    state.seconds = beatsPerSecond > 0.0 ? beat / beatsPerSecond : 0.0;
}

void ofxOceanodeTransport::advanceBeatsLocked(double deltaBeats, double deltaSeconds) {
    if(deltaBeats <= 0.0) return;
    const double before = state.beatPosition;
    double after = before + deltaBeats;
    state.seconds += std::max(0.0, deltaSeconds);
    // Wrap only when crossing the loop end from before it: a playhead placed
    // past the end plays on.
    // (A tolerance on the end, so accumulated rounding one ulp short of it still wraps.)
    if(state.loopActive() && before < state.loopEndBeat && after >= state.loopEndBeat - 1e-9) {
        const double length = state.loopEndBeat - state.loopStartBeat;
        after = state.loopStartBeat + std::fmod(std::max(0.0, after - state.loopEndBeat), length);
        state.loopCount++;
        state.beatPosition = after;
        const double beatsPerSecond = bpmToBeatsPerSecond(state.bpm);
        state.seconds = beatsPerSecond > 0.0 ? after / beatsPerSecond : 0.0;
        return;
    }
    state.beatPosition = after;
}

void ofxOceanodeTransport::notifyReset() {
    std::lock_guard<std::mutex> lock(mutex);
    const uint64_t nowUs = getSteadyNowUs();
    advanceStateToNowLocked(nowUs);
    state.generation++;
    state.steadyTimeUs = nowUs;
}

void ofxOceanodeTransport::latchFrameState() {
    std::lock_guard<std::mutex> lock(mutex);
    frameState.previous = frameState.current;
    frameState.current = state;
}

ofxOceanodeTransportState ofxOceanodeTransport::getState() const {
    std::lock_guard<std::mutex> lock(mutex);
    return state;
}

ofxOceanodeFrameTransportState ofxOceanodeTransport::getFrameState() const {
    std::lock_guard<std::mutex> lock(mutex);
    return frameState;
}

double ofxOceanodeTransport::getTimeInSeconds() const {
    std::lock_guard<std::mutex> lock(mutex);
    return state.seconds;
}

uint64_t ofxOceanodeTransport::getSteadyNowUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

void ofxOceanodeTransport::advanceStateToNowLocked(uint64_t nowUs) {
    if(state.driverMode == TransportDriverMode::External && externalOwner != nullptr) {
        // Extrapolate from the last clock report at the current tempo, capped so we
        // never run more than maxLead past it, and never move backwards.
        if(externalPlaying && nowUs > externalAnchorUs) {
            const double elapsed = static_cast<double>(nowUs - externalAnchorUs) / 1000000.0;
            const double target = std::min(externalAnchorBeat + elapsed * bpmToBeatsPerSecond(state.bpm),
                                           externalAnchorBeat + externalMaxLeadBeats);
            if(target > state.beatPosition) {
                const double beatsPerSecond = bpmToBeatsPerSecond(state.bpm);
                if(beatsPerSecond > 0.0) state.seconds += (target - state.beatPosition) / beatsPerSecond;
                state.beatPosition = target;
            }
        }
        state.steadyTimeUs = nowUs;
        return;
    }
    if(state.driverMode != TransportDriverMode::RealTime) {
        state.steadyTimeUs = nowUs;
        return;
    }

    if(state.steadyTimeUs == 0) {
        state.steadyTimeUs = nowUs;
        return;
    }

    if(state.isPlaying && nowUs > state.steadyTimeUs) {
        const double deltaSeconds = static_cast<double>(nowUs - state.steadyTimeUs) / 1000000.0;
        advanceBeatsLocked(deltaSeconds * bpmToBeatsPerSecond(state.bpm), deltaSeconds);
    }

    state.steadyTimeUs = nowUs;
}

// ---- External clock ----

void ofxOceanodeTransport::claimExternalClock(const void* owner) {
    std::lock_guard<std::mutex> lock(mutex);
    externalOwner = owner;
    externalAnchorBeat = state.beatPosition;
    externalAnchorUs = getSteadyNowUs();
    externalMaxLeadBeats = 0.0;
    externalPlaying = false;
    state.isPlaying = false;
}

void ofxOceanodeTransport::releaseExternalClock(const void* owner) {
    std::lock_guard<std::mutex> lock(mutex);
    if(externalOwner != owner) return;
    externalOwner = nullptr;
    externalPlaying = false;
    state.isPlaying = false;
    state.steadyTimeUs = getSteadyNowUs();
}

bool ofxOceanodeTransport::hasExternalClock() const {
    std::lock_guard<std::mutex> lock(mutex);
    return externalOwner != nullptr;
}

const void* ofxOceanodeTransport::getExternalClockOwner() const {
    std::lock_guard<std::mutex> lock(mutex);
    return externalOwner;
}

void ofxOceanodeTransport::updateExternalClock(const void* owner, const ExternalClockUpdate& update) {
    std::lock_guard<std::mutex> lock(mutex);
    if(owner == nullptr || owner != externalOwner) return;
    const uint64_t nowUs = getSteadyNowUs();
    // Finish the motion under the previous report first.
    if(state.driverMode == TransportDriverMode::External) advanceStateToNowLocked(nowUs);
    if(update.bpm > 0.0f) state.bpm = update.bpm;
    if(update.jump) {
        setPositionLocked(std::max(0.0, update.beat));
        state.generation++;
    }
    externalAnchorBeat = std::max(0.0, update.beat);
    externalAnchorUs = update.steadyTimeUs != 0 ? update.steadyTimeUs : nowUs;
    externalMaxLeadBeats = std::max(0.0, update.maxLeadBeats);
    externalPlaying = update.playing;
    state.isPlaying = update.playing;
    state.steadyTimeUs = nowUs;
    // Bring the position up to date under the new report.
    if(state.driverMode == TransportDriverMode::External) advanceStateToNowLocked(nowUs);
}
