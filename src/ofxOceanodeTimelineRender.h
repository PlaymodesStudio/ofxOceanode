//
//  ofxOceanodeTimelineRender.h
//  ofxOceanode
//
//  "Render Audio..." in the timeline's actions menu, without the timeline
//  knowing who renders. The timeline (ofxOceanodeTimeline) fills the host
//  half and draws the provider's menu items; an audio engine (the
//  SuperCollider addon) registers the provider. Either can be missing: with
//  no provider the timeline draws nothing, with no timeline nothing calls the
//  provider. Header-only so the core stays free of both.
//

#pragma once

#define OFXOCEANODE_HAS_TIMELINE_RENDER 1

#include <functional>
#include <string>

struct ofxOceanodeTimelineRenderRange {
    std::string timelineName;
    // The shown timeline's loop range, in beats of the global transport.
    bool hasRange = false;
    double inBeat = 0.0;
    double outBeat = 0.0;
    double beatsPerBar = 4.0;
    float bpm = 120.0f;
    // A timeline on a clock of its own (Free, or placed on another timeline)
    // counts its beats from somewhere else: its In/Out are not positions of
    // the global transport, which is what a render plays.
    bool onGlobalClock = true;
};

// What the timeline offers the renderer while a render runs.
struct ofxOceanodeTimelineRenderHost {
    // The global transport's position this frame, unlooped.
    std::function<double()> globalBeat;
    // Every timeline plays straight through, ignoring its loop, while true
    // (the loop settings themselves are untouched).
    std::function<void(bool)> suspendLoops;
};

namespace ofxOceanodeTimelineRender {
    // Draws the provider's items inside the timeline's actions menu.
    inline std::function<void(const ofxOceanodeTimelineRenderRange&)>& menuProvider(){
        static std::function<void(const ofxOceanodeTimelineRenderRange&)> provider;
        return provider;
    }
    inline ofxOceanodeTimelineRenderHost& host(){
        static ofxOceanodeTimelineRenderHost instance;
        return instance;
    }
}
