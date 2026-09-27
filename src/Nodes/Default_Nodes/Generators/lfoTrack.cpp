//
//  lfoTrack.cpp
//  ofxOceanode
//

#include "lfoTrack.h"
#include "ofxOceanodeContainer.h"

void lfoTrack::setup() {
    color = ofColor::red;
    description = "Outputs the LFO clips of a timeline track. Value is the LFO as drawn (0..1), "
                  "Phase the raw 0..1 ramp, Cycle the whole cycles since the timeline start "
                  "(continuous across clips and repeats) and Reset fires on each wrap during playback. "
                  "Connect Phase -> Phase and Cycle -> Step of random nodes in Sync To Transport mode "
                  "for results that are identical when scrubbing.";

    trackIds = {""};
    trackNames = {"None"};
    // Not saved as an index: the track id is saved in presetSave instead.
    trackSelectorParam = addParameterDropdown(trackSelector, "Track", 0, trackNames,
        ofxOceanodeParameterFlags_DisableSavePreset | ofxOceanodeParameterFlags_DisableSaveProject);
    addOutputParameter(valueOut.set("Value", 0, 0, 1));
    addOutputParameter(phaseOut.set("Phase", 0, 0, 1));
    addOutputParameter(cycleOut.set("Cycle", 0, 0, FLT_MAX));
    addOutputParameter(resetOut.set("Reset"));
    addOutputParameter(activeOut.set("Active", false));

    listeners.push(trackSelector.newListener([this](int &index) {
        if(updatingSelector) return;
        if(index >= 0 && index < static_cast<int>(trackIds.size())) {
            selectedTrackId = trackIds[index];
            hasLastCycle = false;
        }
    }));
}

void lfoTrack::refreshTrackList() {
    if(hostContainer == nullptr) return;
    std::vector<std::string> ids = {""};
    std::vector<std::string> names = {"None"};
    for(const auto &track : hostContainer->getTimelineManager().getTracks()) {
        if(track.isWaveTrack) continue;
        ids.push_back(track.id);
        names.push_back(track.name);
    }
    if(ids != trackIds || names != trackNames) {
        trackIds = ids;
        trackNames = names;
        trackSelector.setMax(static_cast<int>(trackNames.size()) - 1);
        if(trackSelectorParam) trackSelectorParam->setDropdownOptions(trackNames);
    }
    // Keep the dropdown showing the selected track (by id).
    int index = 0;
    for(size_t i = 1; i < trackIds.size(); ++i) {
        if(trackIds[i] == selectedTrackId) { index = static_cast<int>(i); break; }
    }
    if(trackSelector.get() != index) {
        updatingSelector = true;
        trackSelector = index;
        updatingSelector = false;
    }
}

void lfoTrack::update(ofEventArgs &a) {
    refreshTrackList();
    if(hostContainer == nullptr) return;

    const auto frameState = getFrameTransportState();
    const auto sample = hostContainer->getTimelineManager().evaluateLfoTrack(
        selectedTrackId, frameState.current.beatPosition);

    // Cycle before Phase (as the Phasor does): phase-driven receivers store the step
    // and compute once when the phase arrives. Cycle is only sent when it changes.
    const float cycle = static_cast<float>(sample.cycle);
    if(cycle != cycleOut.get()) cycleOut = cycle;
    phaseOut = sample.phase;
    valueOut = sample.value;
    if(activeOut.get() != sample.active) activeOut = sample.active;

    // Reset on each wrap during continuous forward playback only (not on seeks,
    // stop or loop jumps), so scrubbing does not spray triggers.
    const bool continuous = !ofxOceanodeTimeUtils::didTransportDiscontinuity(frameState) &&
        frameState.current.beatPosition > frameState.previous.beatPosition;
    if(hasLastCycle && continuous && sample.active && sample.cycle > lastCycle) {
        resetOut.trigger();
    }
    lastCycle = sample.cycle;
    hasLastCycle = true;
}

void lfoTrack::presetSave(ofJson &json) {
    json["trackId"] = selectedTrackId;
}

void lfoTrack::presetRecallAfterSettingParameters(ofJson &json) {
    if(json.contains("trackId") && json["trackId"].is_string()) {
        selectedTrackId = json["trackId"].get<std::string>();
        hasLastCycle = false;
    }
}
