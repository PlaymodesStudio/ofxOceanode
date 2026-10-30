#!/usr/bin/env python3
"""Exercise the transport-indexed Buffer: history keyed by transport time.

Part one drives the production buffer.h directly and checks replacement over
the played span, hiding of samples ahead of the playhead, the eviction order,
gap overflow, available history, resizing and mode switches, then compares
random play/seek/loop sessions against a std::map model of the same rules.
Part two drives the production Buffer node with Sync To Transport through the
HeaderPro test fixtures: frame-grid recording while playing, replacement on
input change while paused, seeks into played and unplayed material, loop wraps.
"""
from pathlib import Path
import subprocess
import tempfile

import header_pro_regression

ROOT = Path(__file__).resolve().parents[1]

BUFFER_SOURCE = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <random>
#include <vector>
using namespace std;

#define ofxOceanodeTime_h // frame.h compiles against this mock Timestamp.
struct Timestamp {
    int64_t us = 0;
    Timestamp() = default;
    explicit Timestamp(int64_t value) : us(value) {}
    int64_t epochMicrosecondsSigned() const { return us; }
    void update() {}
};
template<typename T> struct ofEvent;
template<> struct ofEvent<void> {
    vector<function<void()>> listeners;
    template<typename F> void add(F f) { listeners.push_back(f); }
};
void ofNotifyEvent(ofEvent<void> &event) {
    for(auto &listener : event.listeners) listener();
}
#include "buffer.h"

using History = buffer<float>;
History make(int maxSize) {
    History history([](float &value, float &stored) { stored = value; },
                    [](float &stored) { return stored; });
    history.setMaxSize(maxSize);
    history.setTransportIndexed(true);
    history.setGapToleranceMs(33.333f);
    return history;
}
constexpr int64_t F = 16667; // one frame at 60 Hz
float readMs(History &h, float ms, bool &overflow) {
    return h.getClosestFrameDelayMs(ms, true, &overflow).getObjectRef();
}
// Samples stored in stamp order, playhead or not.
vector<float> all(History &h) {
    h.setPlayhead(Timestamp(numeric_limits<int64_t>::max() / 2));
    vector<float> values;
    for(size_t i = 0; i < h.getSize(); ++i) values.push_back(h.getFrame(static_cast<int>(i)).getObjectRef());
    return values;
}

void semantics() {
    History h = make(10);
    assert(h.isTransportIndexed() && h.isOrdered());
    int notifications = 0;
    h.onFrameAdded().add([&] { ++notifications; });
    for(int i = 0; i <= 3; ++i) h.addFrame(10.0f * i, Timestamp(i * F), Timestamp((i - 1) * F));
    assert(notifications == 4);
    const uint64_t before = h.revision();
    h.setPlayhead(Timestamp(3 * F));
    assert(h.revision() != before);
    assert(h.getSize() == 4);
    bool overflow = true;
    assert(readMs(h, 0, overflow) == 30 && !overflow);
    assert(readMs(h, 16.667f, overflow) == 20 && !overflow);
    assert(readMs(h, 50, overflow) == 0 && !overflow);
    assert(readMs(h, -5, overflow) == 30 && !overflow);
    assert(readMs(h, NAN, overflow) == 30 && !overflow);
    assert(h.getAvailableHistoryMs() == static_cast<float>(3 * F / 1000.0L));
    assert(h.getFirstFrameTimestamp().us == 3 * F && h.getLastFrameTimestamp().us == 0);

    // Samples ahead of the playhead are hidden from every read.
    h.setPlayhead(Timestamp(F + 3000));
    assert(h.getSize() == 2);
    assert(readMs(h, 0, overflow) == 10 && !overflow);
    assert(h.getFrame(0, true).getObjectRef() == 10 && h.getFrame(1, true).getObjectRef() == 0);
    assert(h.getFrame(5, true).getObjectRef() == 0); // Out of range: oldest.
    assert(h.getAvailableHistoryMs() == static_cast<float>((F + 3000) / 1000.0L));

    // Continuous playback replaces the span (from, stamp]; later samples stay.
    h.addFrame(99, Timestamp(2 * F + 7000), Timestamp(F + 3000));
    assert((all(h) == vector<float>{0, 10, 99, 30}));
    assert(h.getClosestFrame(Timestamp(2 * F)).getObjectRef() == 99);
    // The same time again replaces in place, whichever span is given.
    h.addFrame(98, Timestamp(2 * F + 7000));
    assert((all(h) == vector<float>{0, 10, 98, 30}));
    h.addFrame(97, Timestamp(2 * F + 7000), Timestamp(2 * F + 7000));
    assert((all(h) == vector<float>{0, 10, 97, 30}));
    // A seek replaces only the frame before the new position.
    h.addFrame(60, Timestamp(6 * F), Timestamp(5 * F));
    assert((all(h) == vector<float>{0, 10, 97, 30, 60}));

    // Full: samples ahead of the recorded time go first, then the oldest.
    h.setMaxSize(5);
    h.addFrame(45, Timestamp(4 * F + 500), Timestamp(3 * F));
    assert((all(h) == vector<float>{0, 10, 97, 30, 45}));
    h.addFrame(50, Timestamp(5 * F), Timestamp(4 * F + 500));
    assert((all(h) == vector<float>{10, 97, 30, 45, 50}));

    // Gap tolerance: nearest sample too far from the requested time overflows.
    h.setPlayhead(Timestamp(5 * F));
    assert(readMs(h, 66.667f, overflow) == 10 && !overflow);
    assert(readMs(h, 90, overflow) == 10 && !overflow);   // 23 ms short of a sample.
    assert(readMs(h, 110, overflow) == 10 && overflow);   // 43 ms short.
    assert(readMs(h, 1e30f, overflow) == 10 && overflow);
    assert(h.getAvailableHistoryMs() == static_cast<float>(4 * F / 1000.0L));
    // Seek far ahead: nothing near the playhead.
    h.setPlayhead(Timestamp(20 * F));
    assert(h.getSize() == 5);
    assert(readMs(h, 0, overflow) == 50 && overflow);
    assert(h.getAvailableHistoryMs() == 0.0f);
    h.addFrame(200, Timestamp(20 * F), Timestamp(19 * F));
    assert(readMs(h, 0, overflow) == 200 && !overflow);
    assert(readMs(h, 10, overflow) == 200 && !overflow);
    assert(readMs(h, 200, overflow) == 50 && overflow);   // Lands in the gap.
    assert(h.getAvailableHistoryMs() == 0.0f);          // Gap right behind the playhead.
    h.addFrame(210, Timestamp(21 * F), Timestamp(20 * F));
    h.setPlayhead(Timestamp(21 * F));
    assert(h.getAvailableHistoryMs() == static_cast<float>(F / 1000.0L));

    // Shrinking evicts by the same rule, relative to the playhead, and compacts.
    assert((all(h) == vector<float>{30, 45, 50, 200, 210}));
    h.setPlayhead(Timestamp(21 * F));
    h.setMaxSize(2);
    assert((all(h) == vector<float>{200, 210}));
    h.setPlayhead(Timestamp(21 * F));
    assert(readMs(h, 16.667f, overflow) == 200 && !overflow);
    h.addFrame(220, Timestamp(22 * F), Timestamp(21 * F));
    assert((all(h) == vector<float>{210, 220}));
    h.setMaxSize(4);
    for(int i = 23; i < 30; ++i) h.addFrame(10.0f * i, Timestamp(i * F), Timestamp((i - 1) * F));
    assert((all(h) == vector<float>{260, 270, 280, 290}));

    // Leaving the mode empties the history; the ring behaviour returns.
    h.setTransportIndexed(false);
    assert(!h.isTransportIndexed() && h.getSize() == 0);
    h.addFrame(1, Timestamp(500));
    h.addFrame(2, Timestamp(100));
    assert(h.getSize() == 2 && !h.isOrdered());
    h.setTransportIndexed(true);
    assert(h.getSize() == 0);
    h.clear();
    h.addFrame(5, Timestamp(0), Timestamp(-F));
    h.setPlayhead(Timestamp(0));
    assert(h.getSize() == 1 && readMs(h, 0, overflow) == 5 && !overflow);
}

// The same rules on a map keyed by transport time.
struct Model {
    size_t maxSize;
    int64_t tolerance;
    map<int64_t, float> samples;
    void add(float value, int64_t stamp, int64_t from) {
        const int64_t lower = min(from, stamp - 1);
        samples.erase(samples.upper_bound(lower), samples.upper_bound(stamp));
        if(samples.size() >= maxSize) {
            if(samples.rbegin()->first > stamp) samples.erase(prev(samples.end()));
            else samples.erase(samples.begin());
        }
        samples[stamp] = value;
    }
    size_t size(int64_t playhead) const { return distance(samples.begin(), samples.upper_bound(playhead)); }
    pair<float, bool> read(int64_t playhead, int64_t delayUs) const {
        auto end = samples.upper_bound(playhead);
        assert(end != samples.begin());
        const int64_t target = playhead - delayUs;
        auto above = samples.lower_bound(target);
        if(above == samples.begin()) {
        } else if(above == end || above->first != target) {
            auto below = prev(above);
            if(above == end || (above->first - target) > (target - below->first)) above = below;
        }
        if(above == end) above = prev(end);
        return {above->second, llabs(above->first - target) > tolerance};
    }
};

void randomSessions() {
    mt19937 rng(7);
    for(int session = 0; session < 200; ++session) {
        const int maxSize = 1 + static_cast<int>(rng() % 40);
        History h = make(maxSize);
        Model m{static_cast<size_t>(maxSize), 33333, {}};
        int64_t position = 0, last = -F;
        for(int step = 0; step < 300; ++step) {
            const int action = static_cast<int>(rng() % 10);
            if(action < 7) {                              // play one frame
                position += F;
            } else if(action < 9) {                       // seek
                position = static_cast<int64_t>(rng() % 60) * F + static_cast<int64_t>(rng() % 5000);
                last = position - F;
            } else {                                      // loop wrap
                position = static_cast<int64_t>(rng() % 10) * F;
                last = position - F;
            }
            const float value = static_cast<float>(step);
            h.addFrame(value, Timestamp(position), Timestamp(last));
            m.add(value, position, last);
            last = position;
            const int64_t playhead = (rng() % 4 == 0)
                ? static_cast<int64_t>(rng() % 70) * F + static_cast<int64_t>(rng() % 20000) : position;
            h.setPlayhead(Timestamp(playhead));
            assert(h.getSize() == m.size(playhead));
            if(h.getSize() == 0) continue;
            for(int q = 0; q < 4; ++q) {
                const int64_t delayUs = static_cast<int64_t>(rng() % 50) * F / 3;
                bool overflow = false;
                const float got = readMs(h, delayUs / 1000.0f, overflow);
                const auto want = m.read(playhead, delayUs);
                assert(got == want.first);
                assert(overflow == want.second);
            }
            assert(h.getFrame(0, true).getObjectRef() == prev(m.samples.upper_bound(playhead))->second);
            assert(h.getFrame(0).getObjectRef() == m.samples.begin()->second);
        }
    }
}

int main() {
    semantics();
    randomSessions();
    return 0;
}
'''

NODE_BODY = r'''
template<typename T, typename Node> ofParameter<T> &parameter(Node &node, string name) {
    return *static_cast<ofParameter<T>*>(node.parameters.at(name));
}
template<typename T, typename Node> ofParameter<T> &inspector(Node &node, string name) {
    return *static_cast<ofParameter<T>*>(node.inspectorParameters.at(name));
}
void expect(const vector<float> &actual, const vector<float> &wanted) {
    if(actual != wanted) {
        cerr << "expected:";
        for(float x : wanted) cerr << " " << x;
        cerr << " | actual:";
        for(float x : actual) cerr << " " << x;
        cerr << "\n";
        abort();
    }
}
constexpr double FRAME = 1.0 / 60.0;

struct Session {
    bufferNode<float> recorder{"test", 0.0f, true};
    bufferHeaderPro<float> reader{"test", 0.0f, false};
    ofEventArgs args;
    double seconds = 0;
    uint64_t generation = 0, loopCount = 0;
    Session() {
        recorder.setup();
        reader.setup();
        parameter<int>(recorder, "Max Size") = 1000;
        parameter<buffer<float>*>(reader, "Buffer Input") = parameter<buffer<float>*>(recorder, "Output").get();
        parameter<int>(reader, "Copies") = 3;
        parameter<int>(reader, "Offset Mode") = 1;
        parameter<vector<float>>(reader, "Offset") = vector<float>{0, 100, 500};
        inspector<bool>(recorder, "Sync To Transport") = true;
    }
    buffer<float> *history() { return parameter<buffer<float>*>(recorder, "Output").get(); }
    void tick(bool playing, double next, float value) {
        auto &state = recorder.frameTransport;
        state.previous = state.current;
        state.current.isPlaying = playing;
        state.current.seconds = next;
        state.current.generation = generation;
        state.current.loopCount = loopCount;
        seconds = next;
        parameter<float>(recorder, "Input") = value; // upstream node ran first
        recorder.update(args);
        reader.update(args);
    }
    void play(double until, float base) {
        while(seconds + FRAME <= until + 1e-9) tick(true, seconds + FRAME, base + static_cast<float>(std::round((seconds + FRAME) * 60)));
    }
    // Upstream nodes follow the transport, so they send the value for the new
    // position in the same frame.
    void seek(double to, float base) { ++generation; tick(false, to, base + static_cast<float>(std::round(to * 60))); }
    const vector<float> &out() { return parameter<vector<float>>(reader, "Output").get(); }
    bool overflow() { return parameter<bool>(reader, "Overflow").get(); }
};

int main() {
    Session s;
    s.tick(true, 0, 0);
    s.play(1.0, 0);
    assert(s.history()->getSize() == 61);
    expect(s.out(), {60, 54, 30});
    assert(!s.overflow());
    assert(std::fabs(inspector<float>(s.recorder, "Last Frame Ms").get() - 1000.0f) < 0.01f);

    // Paused seek into played material: exact samples for that time.
    s.seek(0.5, 0);
    expect(s.out(), {30, 24, 0});
    assert(!s.overflow());
    assert(s.history()->getSize() == 31);
    // Input changes while paused replace the sample at the playhead.
    parameter<float>(s.recorder, "Input") = 777;
    s.reader.update(s.args);
    expect(s.out(), {777, 24, 0});
    assert(s.history()->getSize() == 31);
    s.tick(false, 0.5, 777);
    assert(s.history()->getSize() == 31);

    // Seek into unplayed material: only the new position has a sample, so
    // reads behind it are flagged.
    s.seek(2.0, 1000);
    expect(s.out(), {1120, 1120, 1120});
    assert(s.overflow());
    assert(inspector<float>(s.recorder, "Last Frame Ms").get() == 0.0f);
    s.play(2.5, 1000);
    expect(s.out(), {1150, 1144, 1120});
    assert(!s.overflow());

    // Loop wrap: a new pass refreshes the loop region in place.
    ++s.loopCount;
    s.tick(true, 0.0, 2000);
    s.play(0.3, 2000);
    expect(s.out(), {2018, 2012, 2000});   // 0.3 s back 0.5 s: before the loop start.
    assert(s.overflow());
    s.seek(0.9, 0);
    expect(s.out(), {54, 48, 24});         // First pass, untouched by the loop.
    assert(!s.overflow());
    s.seek(0.2, 2000);
    expect(s.out(), {2012, 2006, 2000});
    assert(s.overflow());

    // Rec off and preset loading record nothing; Clear empties.
    parameter<bool>(s.recorder, "Rec") = false;
    s.seek(3.0, 3000);
    s.play(3.2, 3000);
    assert(s.overflow() && s.out().front() == 1150);
    parameter<bool>(s.recorder, "Rec") = true;
    ofxOceanodeShared::loading = true;
    s.play(3.4, 3000);
    assert(s.overflow() && s.out().front() == 1150);
    ofxOceanodeShared::loading = false;
    s.play(3.6, 3000);
    expect(s.out(), {3216, 3210, 3205});   // 3.1 s: nearest is the first sample after the gap.
    assert(s.overflow());
    parameter<void>(s.recorder, "Clear").trigger();
    assert(s.history()->getSize() == 0);
    s.tick(false, 3.6, 3216);
    assert(s.history()->getSize() == 1);
    expect(s.out(), {3216, 3216, 3216});

    // Switching the option off returns to wall-clock recording with an empty history.
    inspector<bool>(s.recorder, "Sync To Transport") = false;
    assert(!s.history()->isTransportIndexed() && s.history()->getSize() == 0);
    Timestamp::nowMs = 5000;
    parameter<float>(s.recorder, "Input") = 1;
    assert(s.history()->getSize() == 1);

    // Vector buffers follow the same clock.
    bufferNode<vector<float>> vectors("v", 0.0f, true);
    vectors.setup();
    inspector<bool>(vectors, "Sync To Transport") = true;
    auto *vh = parameter<buffer<vector<float>>*>(vectors, "Output").get();
    assert(vh->isTransportIndexed());
    ofEventArgs args;
    for(int i = 1; i <= 3; ++i) {
        vectors.frameTransport.previous = vectors.frameTransport.current;
        vectors.frameTransport.current.isPlaying = true;
        vectors.frameTransport.current.seconds = i * FRAME;
        parameter<vector<float>>(vectors, "Input") = vector<float>{static_cast<float>(i)};
        vectors.update(args);
    }
    assert(vh->getSize() == 3);
    bool overflow = false;
    assert(vh->getClosestFrameDelayMs(16.667f, true, &overflow).getObjectRef() == vector<float>{2} && !overflow);
    return 0;
}
'''


def build_and_run(folder, name, source, includes):
    path = folder / f"{name}.cpp"
    binary = folder / name
    path.write_text(source)
    subprocess.run([
        "clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
        *["-I" + str(ROOT / include) for include in includes],
        str(path), "-o", str(binary),
    ], check=True)
    subprocess.run([str(binary)], check=True)


def main():
    marker = "template<typename T, typename Node> ofParameter<T> &parameter"
    mocks = header_pro_regression.SOURCE[:header_pro_regression.SOURCE.index(marker)]
    with tempfile.TemporaryDirectory(prefix="buffer-transport-") as directory:
        folder = Path(directory)
        build_and_run(folder, "buffer_transport", BUFFER_SOURCE, ["src/Nodes/Default_Nodes/Buffer", "src"])
        build_and_run(folder, "buffer_transport_node", mocks + NODE_BODY, [
            "src/Nodes/Default_Nodes/Buffer", "src/Nodes", "src", "src/Nodes/Default_Nodes/Generators"])
    print("Buffer transport regression passed: span replacement, hidden future samples, eviction order, "
          "gap overflow, history span, resizing, mode switches, random sessions against a map model, "
          "and node recording through play, pause, seek, loop, Rec and preset loading.")


if __name__ == "__main__":
    main()
