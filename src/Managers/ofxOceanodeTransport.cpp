#include "ofxOceanodeTransport.h"

#include <algorithm>
#include <chrono>

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
    const uint64_t nowUs = getSteadyNowUs();
    advanceStateToNowLocked(nowUs);
    state.bpm = std::max(0.0f, bpm);
    state.steadyTimeUs = nowUs;
}

void ofxOceanodeTransport::setIsPlaying(bool isPlaying) {
    std::lock_guard<std::mutex> lock(mutex);
    const uint64_t nowUs = getSteadyNowUs();
    advanceStateToNowLocked(nowUs);
    state.isPlaying = isPlaying;
    state.steadyTimeUs = nowUs;
}

void ofxOceanodeTransport::togglePlay() {
    std::lock_guard<std::mutex> lock(mutex);
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
    const uint64_t nowUs = getSteadyNowUs();
    advanceStateToNowLocked(nowUs);
    state.isPlaying = false;
    state.beatPosition = 0.0;
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
        state.beatPosition += std::max(0.0, deltaSeconds) * bpmToBeatsPerSecond(state.bpm);
    }
    state.steadyTimeUs = nowUs;
}

void ofxOceanodeTransport::seekToBeat(double beatPosition) {
    std::lock_guard<std::mutex> lock(mutex);
    const uint64_t nowUs = getSteadyNowUs();
    advanceStateToNowLocked(nowUs);
    state.beatPosition = std::max(0.0, beatPosition);
    state.generation++;
    state.steadyTimeUs = nowUs;
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
    const double beatsPerSecond = bpmToBeatsPerSecond(state.bpm);
    if(beatsPerSecond <= 0.0) {
        return 0.0;
    }
    return state.beatPosition / beatsPerSecond;
}

uint64_t ofxOceanodeTransport::getSteadyNowUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

void ofxOceanodeTransport::advanceStateToNowLocked(uint64_t nowUs) {
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
        state.beatPosition += deltaSeconds * bpmToBeatsPerSecond(state.bpm);
    }

    state.steadyTimeUs = nowUs;
}
