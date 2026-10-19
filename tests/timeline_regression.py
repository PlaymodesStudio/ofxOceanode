#!/usr/bin/env python3
"""Exercise the full production timeline model using real OF parameters and JSON.

The container adapter supplies only transport and parameter lookup. No canvas,
audio backend, hardware MIDI, or application event loop is exercised. Requires
the sibling macOS openFrameworks libraries, or OCEANODE_TEST_OF_ROOT.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

CONTAINER = r'''
#pragma once
#include "ofMain.h"
#include "ofxOceanodeTransport.h"
#include "ofxOceanodeParameter.h"
class ofxOceanodeContainer {
public:
    std::shared_ptr<ofxOceanodeTransport> transport = std::make_shared<ofxOceanodeTransport>();
    std::map<std::string, ofxOceanodeAbstractParameter*> targets;
    auto getTransport() const { return transport; }
    auto getTransportState() const { return transport->getState(); }
    void setBpm(float bpm) { transport->setBpm(bpm); }
    std::string getTimelineParameterPath(ofxOceanodeAbstractParameter& parameter) const {
        return parameter.getEscapedName();
    }
    ofxOceanodeAbstractParameter* findTimelineParameter(const std::string& path) const {
        auto it = targets.find(path);
        return it == targets.end() ? nullptr : it->second;
    }
};
'''

CASES = r'''
#include "Managers/ofxOceanodeContainer.h"
#include "Timeline/ofxOceanodeTimeline.h"
#include <cassert>
#include <cmath>
#include <iostream>
// This fixture has no connections or scopes. Parameter destruction is isolated.
void ofxOceanodeAbstractParameter::removeAllConnections() {
    assert(!hasInConnection() && !hasOutConnections() && !isScoped());
}
static void checkNear(double actual, double expected, int line) {
    if(std::abs(actual - expected) >= 1e-5) {
        std::cerr << "Line " << line << ": expected " << expected << ", got " << actual << "\n";
        std::abort();
    }
}
#define near(actual, expected) checkNear(actual, expected, __LINE__)
int main() {
    ofSeedRandom(); // OF normally constructs its random engine during app startup.
    // Number lanes hold absolute values between arbitrarily placed entries.
    {
        ofxOceanodeContainer numberOwner;
        numberOwner.transport->setDriverMode(TransportDriverMode::FrameStep);
        ofParameter<float> output("Number", 0.0f, -100.0f, 100.0f);
        ofxOceanodeParameter<float> target; target.bindParameter(output);
        numberOwner.targets[target.getEscapedName()] = &target;
        ofxOceanodeTimelineManager numbers(&numberOwner);
        const auto t = numbers.createTrack("Numbers");
        const auto b = numbers.addBinding(t, target);
        const auto c = numbers.createClip(t, "Numbers", 0.0, 4.0);
        numbers.setClipContentDuration(t, c, 4.0, false);
        const auto l = numbers.createLane(t, c, "Number", ofxOceanodeTimelineLaneType::Number);
        numbers.addBindingToLane(t, c, l, b);
        assert(numbers.setClipNumber(t, c, l, 0.0, -12.5f));
        assert(numbers.setClipNumber(t, c, l, 1.375, 42.25f));
        assert(numbers.setClipNumber(t, c, l, 3.0, 7.0f));
        assert(numbers.setClipNumber(t, c, l, 3.0, 8.0f));
        assert(!numbers.setClipNumber(t, c, l, -1.0, 0.0f));
        assert(!numbers.setClipNumber(t, c, l, 2.0, std::numeric_limits<float>::infinity()));
        assert(numbers.getLane(t, c, l)->curvePoints.size() == 3);
        auto evaluate = [&](double beat, float expected) {
            numberOwner.transport->seekToBeat(beat); numbers.update();
            if(std::abs(output.get() - expected) >= 1e-5) std::cerr << "Number beat " << beat << "\n";
            near(output.get(), expected);
        };
        evaluate(0.0, -12.5f); evaluate(1.374, -12.5f);
        evaluate(1.375, 42.25f); evaluate(2.9, 42.25f); evaluate(3.5, 8.0f);
        evaluate(0.5, -12.5f); // seeking backwards recomputes the held value
        std::vector<ofxOceanodeScheduledParameterEvent> events;
        ofxOceanodeScheduling::registerParameterTarget(&target, &numbers,
            [&](const auto& event) { events.push_back(event); return true; });
        numberOwner.transport->seekToBeat(1.3);
        numberOwner.transport->play(); numbers.update();
        assert(std::any_of(events.begin(), events.end(), [](const auto& event) {
            return !event.isCorrection && std::abs(ofToFloat(event.value) - 42.25f) < 1e-5;
        }));
        numberOwner.transport->pause();
        ofxOceanodeScheduling::unregisterOwner(&numbers);
        const auto savedNumbers = numbers.toJson();
        numbers.fromJson(savedNumbers);
        assert(numbers.getLane(t, c, l)->type == ofxOceanodeTimelineLaneType::Number);
        evaluate(2.0, 42.25f);
        const auto right = numbers.splitClip(t, c, 2.0);
        assert(!right.empty());
        evaluate(1.9, 42.25f); evaluate(2.0, 42.25f); evaluate(3.5, 8.0f);
        numbers.getClip(t, right)->repeatContent = true;
        numbers.setClipTiming(t, right, 2.0, 4.0);
        evaluate(4.25, 42.25f); evaluate(5.5, 8.0f);
    }
    // Dropdown lanes select a dropdown parameter's options over time.
    {
        ofxOceanodeContainer dropdownOwner;
        dropdownOwner.transport->setDriverMode(TransportDriverMode::FrameStep);
        ofParameter<int> mode("Mode", 0, 0, 3);
        ofxOceanodeParameter<int> target; target.bindParameter(mode);
        target.setDropdownOptions({"Off", "Slow", "Fast", "Strobe"});
        dropdownOwner.targets[target.getEscapedName()] = &target;
        assert(ofxOceanodeTimelineDropdownOptions(target).size() == 4);
        ofxOceanodeTimelineManager dropdowns(&dropdownOwner);
        const auto t = dropdowns.createTrack("Modes");
        const auto b = dropdowns.addBinding(t, target);
        assert(dropdowns.getBinding(t, b)->laneType == ofxOceanodeTimelineLaneType::Dropdown);
        const auto c = dropdowns.createClip(t, "Modes", 0.0, 4.0);
        dropdowns.setClipContentDuration(t, c, 4.0, false);
        const auto l = dropdowns.createLane(t, c, "Mode", ofxOceanodeTimelineLaneType::Dropdown);
        dropdowns.addBindingToLane(t, c, l, b);
        assert(dropdowns.setClipNumber(t, c, l, 0.0, 2.0f));
        assert(dropdowns.setClipNumber(t, c, l, 1.0, 3.0f));
        assert(dropdowns.setClipNumber(t, c, l, 2.5, 1.0f));
        auto evaluate = [&](double beat, int expected) {
            dropdownOwner.transport->seekToBeat(beat); dropdowns.update();
            if(mode.get() != expected) std::cerr << "Dropdown beat " << beat << ": " << mode.get() << "\n";
            assert(mode.get() == expected);
        };
        evaluate(0.5, 2); evaluate(1.0, 3); evaluate(2.4, 3); evaluate(3.0, 1);
        const auto saved = dropdowns.toJson();
        dropdowns.fromJson(saved);
        assert(dropdowns.getLane(t, c, l)->type == ofxOceanodeTimelineLaneType::Dropdown);
        assert(dropdowns.getLane(t, c, l)->curveInterpolation == "Step");
        evaluate(1.5, 3);
        // switching a Number lane to Dropdown rounds its values to indices
        assert(dropdowns.setClipLaneType(t, c, l, ofxOceanodeTimelineLaneType::Number));
        dropdowns.getLane(t, c, l)->curvePoints[0].value = 1.6f;
        assert(dropdowns.setClipLaneType(t, c, l, ofxOceanodeTimelineLaneType::Dropdown));
        near(dropdowns.getLane(t, c, l)->curvePoints[0].value, 2.0);
        const auto right = dropdowns.splitClip(t, c, 2.0);
        assert(!right.empty());
        evaluate(2.2, 3); evaluate(3.0, 1);
    }
    ofxOceanodeContainer owner;
    owner.transport->setDriverMode(TransportDriverMode::FrameStep);
    ofParameter<float> value("Value", 0.0f, 0.0f, 127.0f);
    ofxOceanodeParameter<float> parameter;
    parameter.bindParameter(value);
    owner.targets[parameter.getEscapedName()] = &parameter;
    ofxOceanodeTimelineManager timeline(&owner);
    const auto track = timeline.createTrack("Notes");
    const auto binding = timeline.addBinding(track, parameter);
    const auto clip = timeline.createClip(track, "Phrase", 0.0, 2.0);
    const auto lane = timeline.createLane(track, clip, "Pitch", ofxOceanodeTimelineLaneType::Step);
    assert(!binding.empty() && !clip.empty() && !lane.empty());
    assert(timeline.addBindingToLane(track, clip, lane, binding));
    assert(timeline.setClipStep(track, clip, lane, 0.0, "64", 1.0));
    assert(timeline.setClipStep(track, clip, lane, 1.0, "72", 1.0));
    owner.transport->seekToBeat(0.5); timeline.update(); near(value.get(), 64.0);
    owner.transport->seekToBeat(1.5); timeline.update(); near(value.get(), 72.0);

    // A second track shares the parameter without stealing its original binding.
    const auto addTrack = timeline.createTrack("Transpose");
    const auto addBinding = timeline.addBinding(addTrack, parameter, ofxOceanodeTimelineAutomationMode::Add);
    const auto addClip = timeline.createClip(addTrack, "Offset", 0.0, 2.0);
    const auto addLane = timeline.createLane(addTrack, addClip, "Offset", ofxOceanodeTimelineLaneType::Step);
    assert(timeline.addBindingToLane(addTrack, addClip, addLane, addBinding));
    assert(timeline.setClipStep(addTrack, addClip, addLane, 0.0, "2", 2.0));
    owner.transport->seekToBeat(0.5); timeline.update(); near(value.get(), 66.0);
    timeline.setLoopRange(0.0, 2.0); timeline.setLoopEnabled(true);
    owner.transport->seekToBeat(1.9); owner.transport->play();
    owner.transport->advanceFrameStep(0.2); timeline.update();
    near(owner.transport->getState().beatPosition, 0.3); near(value.get(), 66.0);
    owner.transport->pause();

    // Capture manually edited states, then interpolate them after seeking.
    ofParameter<float> level("Level", 0.25f, 0.0f, 1.0f);
    ofxOceanodeParameter<float> levelParameter; levelParameter.bindParameter(level);
    owner.targets[levelParameter.getEscapedName()] = &levelParameter;
    const auto captureTrack = timeline.createTrack("Captured level");
    const auto captureBinding = timeline.addBinding(captureTrack, levelParameter);
    assert(timeline.captureStateAsCurvePoints(captureTrack, captureBinding, 0.0) == 1);
    level = 0.75f;
    assert(timeline.captureStateAsCurvePoints(captureTrack, captureBinding, 2.0) == 1);
    owner.transport->seekToBeat(1.0); timeline.update();
    near(level.get(), 0.5);

    const auto lfoTrack = timeline.createTrack("LFO");
    const auto lfo = timeline.createLfoClip(lfoTrack, "", "Modulator", 0.0, 4.0);
    assert(!lfo.empty() && timeline.getClip(lfoTrack, lfo)->isLfo);
    assert(timeline.evaluateLfoTrack(lfoTrack, 0.5).active);
    const auto waveTrack = timeline.createWaveTrack("Audio");
    assert(timeline.getTrack(waveTrack)->isWaveTrack);
    timeline.addWaveTrackVolumePoint(waveTrack, 0.0, 0.0f);
    timeline.addWaveTrackVolumePoint(waveTrack, 4.0, 1.0f);
    near(timeline.evaluateWaveTrackVolume(waveTrack, 2.0), 0.5);
    timeline.setTimeSignature(7, 8); near(timeline.getBeatsPerBar(), 3.5);
    timeline.addMarker(1.0, "Entry");
    timeline.getViewState().scrollX = 123.0f;
    timeline.getViewState().pixelsPerSecond = 80.0f;
    assert(!timeline.groupClips({{track, clip}, {addTrack, addClip}}).empty());

    // Save/load preserves clip data, groups, bindings, captures and editor state.
    const auto saved = timeline.toJson();
    ofxOceanodeTimelineManager restored(&owner); restored.fromJson(saved);
    assert(restored.getTracks().size() == timeline.getTracks().size());
    assert(restored.getClipGroups().size() == 1 && restored.getMarkers().size() == 1);
    assert(restored.getTrack(track)->bindings.front().parameterPath == parameter.getEscapedName());
    assert(restored.getTrack(captureTrack)->clips.front().isStateCapture);
    assert(restored.getClip(lfoTrack, lfo)->isLfo && restored.getTrack(waveTrack)->isWaveTrack);
    near(restored.getViewState().scrollX, 123.0); near(restored.getBeatsPerBar(), 3.5);
    owner.transport->seekToBeat(0.5); restored.update(); near(value.get(), 66.0);
    assert(restored.removeTrack(addTrack)); restored.update(); near(value.get(), 64.0);
    restored.requestNoteGroupSetup(&parameter);
    restored.clear(); assert(!parameter.isTimelined() && !levelParameter.isTimelined());
    assert(restored.consumePendingNoteGroupSetup() == nullptr);
    std::cout << "Timeline regression passed: automation, dropdown lanes, additive bindings, transport loops, state capture, LFO, wave envelope, JSON persistence, groups, markers, signature, cleanup.\n";
}
'''


def of_root():
    if os.environ.get("OCEANODE_TEST_OF_ROOT"):
        return Path(os.environ["OCEANODE_TEST_OF_ROOT"])
    candidate = ROOT.parent.parent
    if (candidate / "libs/openFrameworks").is_dir():
        return candidate
    common = Path(subprocess.check_output(
        ["git", "rev-parse", "--path-format=absolute", "--git-common-dir"],
        cwd=ROOT, text=True).strip())
    return common.parent.parent.parent


def main():
    of = of_root()
    libdir = of / "libs/openFrameworksCompiled/lib/macos"
    if not (libdir / "openFrameworks.a").is_file():
        raise RuntimeError("Set OCEANODE_TEST_OF_ROOT to a macOS OF tree with compiled libraries")
    includes = [ROOT / "src", ROOT / "src/Managers", ROOT / "src/Timeline"]
    includes += sorted({path.parent for path in (of / "libs/openFrameworks").rglob("*.h")})
    includes += [path for path in (of / "libs").glob("*/include") if path.is_dir()]
    includes += [of / "libs/freetype/include/freetype", of / "libs/cairo/include/cairo"]
    libraries = [str(path) for path in sorted(libdir.glob("*.a"))
                 if "openframeworks" not in path.name.lower()]
    frameworks = ["Accelerate", "AppKit", "ApplicationServices", "AudioToolbox",
                  "AVFoundation", "Cocoa", "CoreAudio", "CoreFoundation", "CoreMedia",
                  "CoreServices", "CoreVideo", "IOKit", "OpenGL", "QuartzCore", "Security"]
    with tempfile.TemporaryDirectory(prefix="oceanode-timeline-") as folder:
        folder = Path(folder)
        (folder / "Managers").mkdir()
        (folder / "Managers/ofxOceanodeContainer.h").write_text(CONTAINER)
        cpp = folder / "regression.cpp"; cpp.write_text(CASES)
        binary = folder / "regression"
        # The container adapter omits NodeModel RTTI; keep the other UB checks.
        command = [os.environ.get("CXX", "clang++"), "-std=c++17", "-O0", "-g", "-w",
                   "-fsanitize=address,undefined", "-fno-sanitize=vptr", "-fno-omit-frame-pointer", "-DOF_NO_FMOD",
                   "-I" + str(folder), *["-I" + str(path) for path in includes], str(cpp),
                   str(ROOT / "src/Timeline/ofxOceanodeTimeline.cpp"),
                   str(ROOT / "src/Timeline/ofxOceanodeScheduling.cpp"),
                   str(ROOT / "src/Managers/ofxOceanodeTransport.cpp"),
                   str(ROOT / "src/Nodes/Default_Nodes/Base/baseOscillator.cpp"),
                   str(of / "libs/openFrameworks/types/ofParameter.cpp"),
                   str(of / "libs/openFrameworks/types/ofParameterGroup.cpp"),
                   str(libdir / "openFrameworks.a"), *libraries, "-Wl,-dead_strip"]
        for framework in frameworks:
            command += ["-framework", framework]
        subprocess.run(command + ["-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
