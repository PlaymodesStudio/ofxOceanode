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
static void nearAt(double actual, double expected, int line) {
    if(std::abs(actual - expected) >= 1e-7) std::cerr << "line " << line << ": " << actual << " != " << expected << "\n";
    assert(std::abs(actual - expected) < 1e-7);
}
#define near(actual, expected) nearAt((actual), (expected), __LINE__)
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
    // ---- Derived transports: one clock, independent players ----
    {
        auto rootTransport = std::make_shared<ofxOceanodeTransport>();
        rootTransport->setDriverMode(TransportDriverMode::FrameStep);
        auto a = ofxOceanodeTransport::makeDerived(rootTransport);
        auto b = ofxOceanodeTransport::makeDerived(rootTransport);
        assert(a->isDerived() && a->getRoot() == rootTransport.get());
        b->setBpm(90.0f);
        // (Real time ran from construction until frame stepping began.)
        const double c0 = rootTransport->getState().clockSeconds;
        // The clock runs while the root is stopped.
        rootTransport->advanceFrameStep(1.0);
        near(rootTransport->getState().clockSeconds, c0 + 1.0);
        near(rootTransport->getState().beatPosition, 0.0);
        near(a->getState().clockSeconds, c0 + 1.0);
        // Timestamped starts land exactly at their instant, between frames.
        a->playAt(c0 + 1.5, 0.0);
        b->playAt(c0 + 1.5, 0.0);
        rootTransport->advanceFrameStep(1.0);
        near(a->getState().beatPosition, 1.0);   // 0.5 s at 120 BPM
        near(b->getState().beatPosition, 0.75);  // 0.5 s at 90 BPM
        near(a->getState().seconds, 0.5);
        assert(a->getState().isPlaying && !rootTransport->getState().isPlaying);
        // No drift: positions stay a function of clock time.
        for(int i = 0; i < 100000; ++i) rootTransport->advanceFrameStep(1.0 / 60.0);
        const double clock = rootTransport->getState().clockSeconds;
        assert(std::abs(a->getState().beatPosition - (clock - c0 - 1.5) * 2.0) < 1e-6);
        assert(std::abs(b->getState().beatPosition - (clock - c0 - 1.5) * 1.5) < 1e-6);
        // Stopping one leaves the other untouched.
        const auto bGeneration = b->getState().generation;
        a->stop();
        rootTransport->advanceFrameStep(0.5);
        near(a->getState().beatPosition, 0.0);
        assert(!a->getState().isPlaying && b->getState().generation == bGeneration && b->getState().isPlaying);
        // Loops wrap at the exact crossing instant and keep the overshoot.
        a->setLoop(true, 0.0, 4.0);
        a->play();
        rootTransport->advanceFrameStep(2.25); // 4.5 beats
        near(a->getState().beatPosition, 0.5);
        assert(a->getState().loopCount == 1);
        // Frame snapshots latch with the root.
        rootTransport->latchFrameState();
        rootTransport->advanceFrameStep(0.25);
        rootTransport->latchFrameState();
        near(a->getFrameState().previous.beatPosition, 0.5);
        near(a->getFrameState().current.beatPosition, 1.0);
        // Tempo changes re-anchor without a jump.
        a->setLoop(false, 0.0, 0.0);
        a->setBpm(60.0f);
        rootTransport->advanceFrameStep(1.0);
        near(a->getState().beatPosition, 2.0);
        // The next bar boundary on a transport's grid, in clock seconds
        // (frame-stepped, so "now" is the last step).
        const double nowClock = a->getState().clockSeconds;
        near(a->nextGridClockSeconds(4.0), nowClock + 2.0);
        near(a->nextGridClockSeconds(1.0, 0.25), nowClock + 1.0);
        // stopAt / seekAt.
        a->seekAt(nowClock + 0.5, 10.0);
        a->stopAt(nowClock + 1.0);
        double pendingClock = 0.0; bool startsPlaying = true;
        assert(a->nextPendingAction(pendingClock, startsPlaying) && !startsPlaying);
        near(pendingClock, nowClock + 0.5);
        rootTransport->advanceFrameStep(2.0);
        near(a->getState().beatPosition, 10.5);
        assert(!a->getState().isPlaying);
        // The Clock source plays whenever the clock runs, at the root's tempo.
        auto clockSource = rootTransport->getClockTransport();
        assert(clockSource != nullptr && clockSource == b->getClockTransport());
        rootTransport->setBpm(60.0f);
        rootTransport->advanceFrameStep(1.0);
        const double clockBeat = clockSource->getState().beatPosition;
        rootTransport->advanceFrameStep(1.0);
        near(clockSource->getState().beatPosition, clockBeat + 1.0);
        assert(clockSource->getState().isPlaying);
        // Time sources: registered by id, found while alive and selectable.
        const auto revision = rootTransport->getTimeSourcesRevision();
        a->registerTimeSource({"canvas/a", "A", "Canvas", true, ""}, a);
        rootTransport->registerTimeSource({"canvas/c", "C", "Canvas", false, "only plays as clips"}, nullptr);
        assert(rootTransport->getTimeSourcesRevision() > revision);
        assert(rootTransport->findTimeSource("canvas/a") == a);
        assert(rootTransport->findTimeSource("canvas/c") == nullptr);
        assert(rootTransport->getTimeSources().size() == 2);
        rootTransport->unregisterTimeSource("canvas/a");
        assert(rootTransport->findTimeSource("canvas/a") == nullptr);
        // External clocks are taken by the root; derived transports forward.
        int owner = 0;
        b->claimExternalClock(&owner);
        assert(rootTransport->hasExternalClock() && b->hasExternalClock());
        b->releaseExternalClock(&owner);
        assert(!rootTransport->hasExternalClock());
        // Children expire with their owners.
        a.reset();
        rootTransport->advanceFrameStep(0.1);
        // Steady time and clock time convert both ways.
        rootTransport->setDriverMode(TransportDriverMode::RealTime);
        rootTransport->syncRealTime();
        const auto now = ofxOceanodeTransport::steadyNowUs();
        const double atNow = rootTransport->clockSecondsAt(now);
        assert(std::llabs(static_cast<long long>(rootTransport->steadyUsAtClock(atNow)) - static_cast<long long>(now)) <= 1);
    }
    std::cout << "Transport regression passed: play/pause/stop, seek, tempo, loops, snapshots, external clock, MIDI, random replay, scheduling, derived transports.\n";
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
                        "-I" + str(ROOT / "src/Managers"), str(cpp),
                        str(ROOT / "src/Managers/ofxOceanodeTransport.cpp"),
                        str(ROOT / "src/Managers/ofxOceanodeScheduling.cpp"),
                        "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
