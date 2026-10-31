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

ofxOceanodeTransport::ofxOceanodeTransport(std::shared_ptr<ofxOceanodeTransport> rootTransport)
    : root(std::move(rootTransport)) {
    // A derived transport of a derived transport follows the same root.
    while(root != nullptr && root->root != nullptr) root = root->root;
    if(root != nullptr) {
        const auto rootState = root->getState();
        state.steadyTimeUs = rootState.steadyTimeUs;
        state.driverMode = rootState.driverMode;
        state.clockSeconds = rootState.clockSeconds;
        state.bpm = rootState.bpm;
        state.beatsPerBar = rootState.beatsPerBar;
    } else {
        state.steadyTimeUs = getSteadyNowUs();
    }
    lastClock = anchorClock = state.clockSeconds;
    frameState.previous = state;
    frameState.current = state;
}

std::shared_ptr<ofxOceanodeTransport> ofxOceanodeTransport::makeDerived(const std::shared_ptr<ofxOceanodeTransport>& rootTransport) {
    if(rootTransport == nullptr) return nullptr;
    auto* owner = rootTransport->getRoot();
    auto derived = std::make_shared<ofxOceanodeTransport>(rootTransport);
    std::lock_guard<std::mutex> lock(owner->childrenMutex);
    owner->children.push_back(derived);
    return derived;
}

void ofxOceanodeTransport::setBpm(float bpm) {
    std::lock_guard<std::mutex> lock(mutex);
    if(externalOwner != nullptr) return; // following an external clock
    if(root != nullptr) {
        state.bpm = std::max(0.0f, bpm);
        reanchorLocked();
        return;
    }
    const uint64_t nowUs = getSteadyNowUs();
    advanceStateToNowLocked(nowUs);
    state.bpm = std::max(0.0f, bpm);
    state.steadyTimeUs = nowUs;
}

void ofxOceanodeTransport::setIsPlaying(bool isPlaying) {
    std::lock_guard<std::mutex> lock(mutex);
    if(externalOwner != nullptr) return; // following an external clock
    if(root != nullptr) {
        if(state.isPlaying != isPlaying) {
            state.isPlaying = isPlaying;
            reanchorLocked();
        }
        return;
    }
    const uint64_t nowUs = getSteadyNowUs();
    advanceStateToNowLocked(nowUs);
    state.isPlaying = isPlaying;
    state.steadyTimeUs = nowUs;
}

void ofxOceanodeTransport::togglePlay() {
    bool playing;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if(externalOwner != nullptr) return; // following an external clock
        playing = state.isPlaying;
    }
    setIsPlaying(!playing);
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
    if(root != nullptr) {
        state.isPlaying = false;
        setPositionLocked(0.0);
        state.generation++;
        reanchorLocked();
        return;
    }
    const uint64_t nowUs = getSteadyNowUs();
    advanceStateToNowLocked(nowUs);
    state.isPlaying = false;
    setPositionLocked(0.0);
    state.generation++;
    state.steadyTimeUs = nowUs;
}

void ofxOceanodeTransport::setDriverMode(TransportDriverMode mode) {
    if(root != nullptr) return; // follows the root's
    std::lock_guard<std::mutex> lock(mutex);
    const uint64_t nowUs = getSteadyNowUs();
    advanceStateToNowLocked(nowUs);
    state.driverMode = mode;
    state.steadyTimeUs = nowUs;
}

void ofxOceanodeTransport::syncRealTime() {
    if(root != nullptr) return; // advanced by its root
    {
        std::lock_guard<std::mutex> lock(mutex);
        advanceStateToNowLocked(getSteadyNowUs());
    }
    advanceChildren();
}

void ofxOceanodeTransport::advanceFrameStep(double deltaSeconds) {
    if(root != nullptr) return; // advanced by its root
    {
        std::lock_guard<std::mutex> lock(mutex);
        const uint64_t nowUs = getSteadyNowUs();
        advanceStateToNowLocked(nowUs);
        if(state.driverMode == TransportDriverMode::FrameStep)
            advanceRootClockLocked(std::max(0.0, deltaSeconds), true);
        state.steadyTimeUs = nowUs;
    }
    advanceChildren();
}

void ofxOceanodeTransport::seekToBeat(double beatPosition) {
    std::lock_guard<std::mutex> lock(mutex);
    if(externalOwner != nullptr) return; // following an external clock
    if(root != nullptr) {
        setPositionLocked(std::max(0.0, beatPosition));
        state.generation++;
        reanchorLocked();
        return;
    }
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
    if(root == nullptr) advanceStateToNowLocked(getSteadyNowUs());
    state.loopEnabled = enabled;
    state.loopStartBeat = std::max(0.0, startBeat);
    state.loopEndBeat = std::max(state.loopStartBeat, endBeat);
}

void ofxOceanodeTransport::setBeatsPerBar(double beatsPerBar) {
    std::lock_guard<std::mutex> lock(mutex);
    state.beatsPerBar = std::max(1e-3, beatsPerBar);
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
    if(root != nullptr) {
        state.generation++;
        return;
    }
    const uint64_t nowUs = getSteadyNowUs();
    advanceStateToNowLocked(nowUs);
    state.generation++;
    state.steadyTimeUs = nowUs;
}

void ofxOceanodeTransport::latchFrameState() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        frameState.previous = frameState.current;
        frameState.current = state;
    }
    if(root == nullptr) latchChildren();
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
        // The clock runs while the external clock plays.
        if(externalPlaying && state.steadyTimeUs != 0 && nowUs > state.steadyTimeUs)
            state.clockSeconds += static_cast<double>(nowUs - state.steadyTimeUs) / 1000000.0;
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

    if(nowUs > state.steadyTimeUs) {
        const double deltaSeconds = static_cast<double>(nowUs - state.steadyTimeUs) / 1000000.0;
        advanceRootClockLocked(deltaSeconds, true);
    }

    state.steadyTimeUs = nowUs;
}

// ---- Timestamped actions ----

void ofxOceanodeTransport::addPendingLocked(const PendingAction& action) {
    auto at = std::upper_bound(pending.begin(), pending.end(), action.clockSeconds,
                               [](double clock, const PendingAction& other) { return clock < other.clockSeconds; });
    pending.insert(at, action);
}

void ofxOceanodeTransport::applyActionLocked(const PendingAction& action) {
    switch(action.type) {
        case ActionType::Play:
            if(action.beat >= 0.0) {
                setPositionLocked(action.beat);
                state.generation++;
            }
            state.isPlaying = true;
            break;
        case ActionType::Stop:
            state.isPlaying = false;
            if(action.rewind) {
                setPositionLocked(0.0);
                state.generation++;
            }
            break;
        case ActionType::Seek:
            setPositionLocked(std::max(0.0, action.beat));
            state.generation++;
            break;
    }
}

void ofxOceanodeTransport::playAt(double clockSeconds, double beat) {
    std::lock_guard<std::mutex> lock(mutex);
    if(externalOwner != nullptr) return;
    addPendingLocked({clockSeconds, ActionType::Play, beat, false});
}

void ofxOceanodeTransport::stopAt(double clockSeconds, bool rewind) {
    std::lock_guard<std::mutex> lock(mutex);
    if(externalOwner != nullptr) return;
    addPendingLocked({clockSeconds, ActionType::Stop, -1.0, rewind});
}

void ofxOceanodeTransport::seekAt(double clockSeconds, double beat) {
    std::lock_guard<std::mutex> lock(mutex);
    if(externalOwner != nullptr) return;
    addPendingLocked({clockSeconds, ActionType::Seek, beat, false});
}

void ofxOceanodeTransport::cancelPendingActions() {
    std::lock_guard<std::mutex> lock(mutex);
    pending.clear();
}

bool ofxOceanodeTransport::nextPendingAction(double& clockSeconds, bool& startsPlaying) const {
    std::lock_guard<std::mutex> lock(mutex);
    if(pending.empty()) return false;
    clockSeconds = pending.front().clockSeconds;
    startsPlaying = pending.front().type == ActionType::Play;
    return true;
}

void ofxOceanodeTransport::advanceRootClockLocked(double deltaSeconds, bool advancePosition) {
    auto move = [&](double seconds) {
        if(seconds <= 0.0) return;
        state.clockSeconds += seconds;
        if(advancePosition && state.isPlaying)
            advanceBeatsLocked(seconds * bpmToBeatsPerSecond(state.bpm), seconds);
    };
    double remaining = std::max(0.0, deltaSeconds);
    while(!pending.empty() && pending.front().clockSeconds <= state.clockSeconds + remaining) {
        const double span = std::max(0.0, pending.front().clockSeconds - state.clockSeconds);
        move(span);
        remaining -= span;
        applyActionLocked(pending.front());
        pending.erase(pending.begin());
    }
    move(remaining);
}

double ofxOceanodeTransport::clockSecondsAt(uint64_t steadyUs) const {
    if(root != nullptr) return root->clockSecondsAt(steadyUs);
    std::lock_guard<std::mutex> lock(mutex);
    const bool running = state.driverMode == TransportDriverMode::RealTime ||
        (state.driverMode == TransportDriverMode::External && externalOwner != nullptr && externalPlaying);
    if(!running) return state.clockSeconds;
    return state.clockSeconds + (static_cast<double>(steadyUs) - static_cast<double>(state.steadyTimeUs)) / 1000000.0;
}

uint64_t ofxOceanodeTransport::steadyUsAtClock(double clockSeconds) const {
    if(root != nullptr) return root->steadyUsAtClock(clockSeconds);
    std::lock_guard<std::mutex> lock(mutex);
    const double us = static_cast<double>(state.steadyTimeUs) + (clockSeconds - state.clockSeconds) * 1000000.0;
    return us <= 0.0 ? 0 : static_cast<uint64_t>(std::llround(us));
}

double ofxOceanodeTransport::nextGridClockSeconds(double gridBeats, double minimumLeadSeconds) const {
    const double nowClock = clockSecondsAt(getSteadyNowUs());
    const double earliest = nowClock + std::max(0.0, minimumLeadSeconds);
    std::lock_guard<std::mutex> lock(mutex);
    const double beatsPerSecond = bpmToBeatsPerSecond(state.bpm);
    if(!state.isPlaying || beatsPerSecond <= 0.0 || gridBeats <= 0.0) return earliest;
    const double beatAtEarliest = state.beatPosition + (earliest - state.clockSeconds) * beatsPerSecond;
    const double boundary = std::ceil(beatAtEarliest / gridBeats - 1e-9) * gridBeats;
    return state.clockSeconds + (boundary - state.beatPosition) / beatsPerSecond;
}

// ---- Derived ----

void ofxOceanodeTransport::reanchorLocked() {
    anchorClock = lastClock;
    anchorBeat = state.beatPosition;
}

void ofxOceanodeTransport::advanceDerivedLinearLocked(double toClock) {
    if(toClock <= lastClock) return;
    const double beatsPerSecond = bpmToBeatsPerSecond(state.bpm);
    if(state.isPlaying && beatsPerSecond > 0.0) {
        // Each loop wrap re-anchors at the exact instant the end is crossed,
        // so the overshoot is kept and the loop never drifts.
        for(int wraps = 0; ; ++wraps) {
            const double target = anchorBeat + (toClock - anchorClock) * beatsPerSecond;
            const bool wraps_here = state.loopActive() && state.beatPosition < state.loopEndBeat &&
                target >= state.loopEndBeat - 1e-9;
            if(!wraps_here) {
                state.beatPosition = target;
                state.seconds += toClock - lastClock;
                break;
            }
            const double length = state.loopEndBeat - state.loopStartBeat;
            if(wraps > 1000) { // a very short loop over a long span: wrap arithmetically
                state.beatPosition = state.loopStartBeat + std::fmod(std::max(0.0, target - state.loopEndBeat), length);
                state.loopCount++;
                state.seconds = state.beatPosition / beatsPerSecond;
                anchorClock = toClock;
                anchorBeat = state.beatPosition;
                break;
            }
            const double crossing = std::max(lastClock, anchorClock + (state.loopEndBeat - anchorBeat) / beatsPerSecond);
            lastClock = crossing;
            anchorClock = crossing;
            anchorBeat = state.loopStartBeat;
            state.beatPosition = state.loopStartBeat;
            state.seconds = state.loopStartBeat / beatsPerSecond;
            state.loopCount++;
        }
    }
    lastClock = toClock;
    state.clockSeconds = toClock;
}

void ofxOceanodeTransport::advanceDerivedLocked(double toClock) {
    while(!pending.empty() && pending.front().clockSeconds <= toClock) {
        advanceDerivedLinearLocked(pending.front().clockSeconds);
        applyActionLocked(pending.front());
        pending.erase(pending.begin());
        reanchorLocked();
    }
    advanceDerivedLinearLocked(toClock);
}

void ofxOceanodeTransport::followRoot(const ofxOceanodeTransportState& rootState) {
    std::lock_guard<std::mutex> lock(mutex);
    advanceDerivedLocked(rootState.clockSeconds);
    state.steadyTimeUs = rootState.steadyTimeUs;
    state.driverMode = rootState.driverMode;
    state.clockSeconds = std::max(state.clockSeconds, rootState.clockSeconds);
    if(followRootTempo && state.bpm != rootState.bpm) {
        state.bpm = rootState.bpm;
        reanchorLocked();
    }
}

std::vector<std::shared_ptr<ofxOceanodeTransport>> ofxOceanodeTransport::liveChildren() {
    std::vector<std::shared_ptr<ofxOceanodeTransport>> live;
    std::lock_guard<std::mutex> lock(childrenMutex);
    children.erase(std::remove_if(children.begin(), children.end(), [&](const auto& weak) {
        auto child = weak.lock();
        if(child == nullptr) return true;
        live.push_back(std::move(child));
        return false;
    }), children.end());
    return live;
}

void ofxOceanodeTransport::advanceChildren() {
    const auto rootState = getState();
    for(auto& child : liveChildren()) child->followRoot(rootState);
}

void ofxOceanodeTransport::latchChildren() {
    for(auto& child : liveChildren()) child->latchFrameState();
}

std::shared_ptr<ofxOceanodeTransport> ofxOceanodeTransport::getClockTransport() {
    if(root != nullptr) return root->getClockTransport();
    {
        std::lock_guard<std::mutex> lock(childrenMutex);
        if(clockTransport != nullptr) return clockTransport;
    }
    auto self = weak_from_this().lock();
    if(self == nullptr) return nullptr;
    auto clock = makeDerived(self);
    {
        std::lock_guard<std::mutex> lock(clock->mutex);
        clock->followRootTempo = true;
        clock->state.isPlaying = true;
        clock->state.beatPosition = clock->state.clockSeconds * bpmToBeatsPerSecond(clock->state.bpm);
        clock->state.seconds = clock->state.clockSeconds;
        clock->reanchorLocked();
    }
    std::lock_guard<std::mutex> lock(childrenMutex);
    if(clockTransport == nullptr) clockTransport = clock;
    return clockTransport;
}

// ---- Time sources ----

void ofxOceanodeTransport::registerTimeSource(const TimeSourceInfo& info, std::shared_ptr<ofxOceanodeTransport> transport) {
    if(root != nullptr) { root->registerTimeSource(info, std::move(transport)); return; }
    std::lock_guard<std::mutex> lock(childrenMutex);
    for(auto& entry : timeSources) {
        if(entry.info.id != info.id) continue;
        const bool changed = entry.info.label != info.label || entry.info.group != info.group ||
            entry.info.selectable != info.selectable || entry.info.note != info.note ||
            entry.transport.lock() != transport;
        entry.info = info;
        entry.transport = transport;
        if(changed) timeSourcesRevision++;
        return;
    }
    timeSources.push_back({info, transport});
    timeSourcesRevision++;
}

void ofxOceanodeTransport::unregisterTimeSource(const std::string& id) {
    if(root != nullptr) { root->unregisterTimeSource(id); return; }
    std::lock_guard<std::mutex> lock(childrenMutex);
    const auto before = timeSources.size();
    timeSources.erase(std::remove_if(timeSources.begin(), timeSources.end(),
                                     [&](const auto& entry) { return entry.info.id == id; }), timeSources.end());
    if(timeSources.size() != before) timeSourcesRevision++;
}

std::vector<ofxOceanodeTransport::TimeSourceInfo> ofxOceanodeTransport::getTimeSources() const {
    if(root != nullptr) return root->getTimeSources();
    std::lock_guard<std::mutex> lock(childrenMutex);
    std::vector<TimeSourceInfo> result;
    for(const auto& entry : timeSources) {
        if(entry.transport.expired() && entry.info.selectable) continue;
        result.push_back(entry.info);
    }
    return result;
}

std::shared_ptr<ofxOceanodeTransport> ofxOceanodeTransport::findTimeSource(const std::string& id) const {
    if(root != nullptr) return root->findTimeSource(id);
    std::lock_guard<std::mutex> lock(childrenMutex);
    for(const auto& entry : timeSources)
        if(entry.info.id == id) return entry.info.selectable ? entry.transport.lock() : nullptr;
    return nullptr;
}

uint64_t ofxOceanodeTransport::getTimeSourcesRevision() const {
    if(root != nullptr) return root->getTimeSourcesRevision();
    std::lock_guard<std::mutex> lock(childrenMutex);
    return timeSourcesRevision;
}

// ---- External clock ----

void ofxOceanodeTransport::claimExternalClock(const void* owner) {
    if(root != nullptr) { root->claimExternalClock(owner); return; }
    std::lock_guard<std::mutex> lock(mutex);
    externalOwner = owner;
    externalAnchorBeat = state.beatPosition;
    externalAnchorUs = getSteadyNowUs();
    externalMaxLeadBeats = 0.0;
    externalPlaying = false;
    state.isPlaying = false;
    pending.clear();
}

void ofxOceanodeTransport::releaseExternalClock(const void* owner) {
    if(root != nullptr) { root->releaseExternalClock(owner); return; }
    std::lock_guard<std::mutex> lock(mutex);
    if(externalOwner != owner) return;
    externalOwner = nullptr;
    externalPlaying = false;
    state.isPlaying = false;
    state.steadyTimeUs = getSteadyNowUs();
}

bool ofxOceanodeTransport::hasExternalClock() const {
    if(root != nullptr) return root->hasExternalClock();
    std::lock_guard<std::mutex> lock(mutex);
    return externalOwner != nullptr;
}

const void* ofxOceanodeTransport::getExternalClockOwner() const {
    if(root != nullptr) return root->getExternalClockOwner();
    std::lock_guard<std::mutex> lock(mutex);
    return externalOwner;
}

void ofxOceanodeTransport::updateExternalClock(const void* owner, const ExternalClockUpdate& update) {
    if(root != nullptr) { root->updateExternalClock(owner, update); return; }
    {
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
    advanceChildren();
}
