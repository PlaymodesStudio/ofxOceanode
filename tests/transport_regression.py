#!/usr/bin/env python3
"""Run production transport, MIDI clock core and scheduling with ASan/UBSan.

Requires Python 3 and clang++; no openFrameworks app or MIDI hardware is used.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

CASES = r'''
#include "ofxOceanodeTransport.h"
#include "ofxOceanodeMidiClockCore.h"
#include "ofxOceanodeDeterministicRandom.h"
#include "ofxOceanodeScheduling.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <thread>
// Scheduling only uses a parameter's identity, so no OF fixture is needed.
class ofxOceanodeAbstractParameter {};
static void near(double actual, double expected) { assert(std::abs(actual - expected) < 1e-7); }
int main() {
    ofxOceanodeTransport transport;
    transport.setDriverMode(TransportDriverMode::FrameStep);
    assert(!transport.getState().isPlaying);
    transport.advanceFrameStep(1.0);
    near(transport.getState().beatPosition, 0.0);
    transport.play();
    transport.advanceFrameStep(0.5);
    near(transport.getState().beatPosition, 1.0);
    near(transport.getTimeInSeconds(), 0.5);
    transport.pause();
    transport.advanceFrameStep(1.0);
    near(transport.getState().beatPosition, 1.0);
    transport.setBpm(60.0f);
    near(transport.getTimeInSeconds(), 0.5);
    transport.play();
    transport.advanceFrameStep(0.5);
    near(transport.getState().beatPosition, 1.5);
    near(transport.getTimeInSeconds(), 1.0);
    transport.latchFrameState();
    transport.seekToBeat(8.0);
    transport.latchFrameState();
    auto frame = transport.getFrameState();
    near(frame.previous.beatPosition, 1.5);
    near(frame.current.beatPosition, 8.0);
    assert(frame.current.generation > frame.previous.generation);
    transport.seekToBeat(-1.0);
    near(transport.getState().beatPosition, 0.0);
    transport.stop();
    assert(!transport.getState().isPlaying);
    near(transport.getTimeInSeconds(), 0.0);

    transport.setBpm(120.0f);
    transport.setLoop(true, 2.0, 6.0);
    transport.seekToBeat(5.5);
    const auto generation = transport.getState().generation;
    transport.play();
    transport.advanceFrameStep(0.5);
    near(transport.getState().beatPosition, 2.5);
    assert(transport.getState().loopCount == 1);
    assert(transport.getState().generation == generation);
    transport.seekToBeat(2.0);
    for(int i = 0; i < 600; ++i) transport.advanceFrameStep(1.0 / 60.0);
    near(transport.getState().beatPosition, 2.0);
    assert(transport.getState().loopCount == 6);
    transport.seekToBeat(7.0); // A seek beyond the loop end continues outside the loop.
    transport.advanceFrameStep(0.5);
    near(transport.getState().beatPosition, 8.0);

    int clockOwner = 0, otherOwner = 0;
    transport.claimExternalClock(&clockOwner);
    transport.setDriverMode(TransportDriverMode::External);
    ofxOceanodeTransport::ExternalClockUpdate report;
    report.beat = 16.0; report.bpm = 90.0f; report.playing = true; report.jump = true;
    transport.updateExternalClock(&clockOwner, report);
    transport.stop(); transport.pause(); transport.seekToBeat(0.0); transport.setBpm(200.0f);
    near(transport.getState().beatPosition, 16.0);
    assert(transport.getState().isPlaying);
    near(transport.getState().bpm, 90.0);
    transport.releaseExternalClock(&otherOwner);
    assert(transport.hasExternalClock());
    transport.releaseExternalClock(&clockOwner);
    assert(!transport.hasExternalClock());
    assert(!transport.getState().isPlaying);

    ofxOceanodeMidiClockCore clock;
    clock.songPosition(16, 1000000);
    clock.start(1010000); // REAPER-style SPP immediately followed by Start.
    near(clock.status(1010000).beat, 4.0);
    for(int i = 0; i <= 24; ++i) clock.tick(1020000 + i * 20833);
    near(clock.status(1520000).beat, 5.0);
    assert(std::abs(clock.status(1520000).bpm - 120.0f) < 0.02f);
    clock.stop(1600000);
    assert(!clock.status(1600000).playing);
    clock.resume(1700000);
    assert(clock.status(1700000).playing);
    clock.poll(2300000);
    assert(!clock.status(2300000).playing);

    using namespace ofxOceanodeDeterministicRandom;
    const auto key = seedKey(1234, 99);
    std::vector<float> samples;
    for(int64_t step = 0; step < 1000; ++step) {
        const auto value = uniform(key, step);
        assert(value >= 0 && value < 1);
        samples.push_back(value);
    }
    for(int64_t step = 999; step >= 0; --step) assert(uniform(key, step) == samples[step]);
    assert(seedKey(1234, 1) == seedKey(1234, 2));
    assert(seedKey(0, 1) != seedKey(0, 2));

    using namespace ofxOceanodeScheduling;
    ofxOceanodeAbstractParameter first, second;
    int calls = 0;
    registerParameterTarget(&first, &clockOwner, [&](const auto& event) {
        assert(event.value == "60" && event.generation == 3);
        ++calls;
        unregisterParameterTarget(&first); // Dispatch must allow reentrant unregister.
        return true;
    });
    registerParameterTarget(&second, &clockOwner, [](const auto&) { return true; });
    assert(dispatch(&first, {"60", 100, 3, false}));
    assert(calls == 1 && !hasParameterTarget(&first));
    unregisterOwner(&clockOwner);
    assert(!dispatch(&second, {}));
    assert(!isBackendSendSuppressed());
    {
        ScopedBackendSuppression outer;
        { ScopedBackendSuppression inner; assert(isBackendSendSuppressed()); }
        assert(isBackendSendSuppressed());
        std::thread independent([] { assert(!isBackendSendSuppressed()); });
        independent.join();
    }
    assert(!isBackendSendSuppressed());
    std::cout << "Transport regression passed: play/pause/stop, seek, tempo, loops, snapshots, external clock, MIDI, random replay, scheduling.\n";
}
'''


def main():
    with tempfile.TemporaryDirectory(prefix="oceanode-transport-") as folder:
        folder = Path(folder)
        cpp = folder / "regression.cpp"
        cpp.write_text(CASES)
        binary = folder / "regression"
        subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++17", "-O1", "-g",
                        "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                        "-I" + str(ROOT / "src"), "-I" + str(ROOT / "src/Managers"),
                        "-I" + str(ROOT / "src/Timeline"), str(cpp),
                        str(ROOT / "src/Managers/ofxOceanodeTransport.cpp"),
                        str(ROOT / "src/Timeline/ofxOceanodeScheduling.cpp"),
                        "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
