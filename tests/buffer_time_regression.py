#!/usr/bin/env python3
"""Exercise production Buffer timestamp lookup and frame notifications."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

SOURCE = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <vector>
using namespace std;

#define frame_h
struct Timestamp {
    int64_t us = 0;
    Timestamp() = default;
    explicit Timestamp(int64_t value) : us(value) {}
    int64_t epochMicrosecondsSigned() const { return us; }
    void update() {}
};
template<typename T1, typename T2 = T1> struct bufferFrame {
    T1 value;
    Timestamp timestamp;
    using Assign = function<void(T1&, T2&)>;
    using Return = function<T1(T2&)>;
    bufferFrame(T1 item, Timestamp time, Assign, Return) : value(item), timestamp(time) {}
    void update(T1 item, Timestamp time) { value = item; timestamp = time; }
    T1 getObjectRef() { return value; }
    Timestamp getTimestamp() { return timestamp; }
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

int main() {
    buffer<float> history(
        [](float &value, float &stored) { stored = value; },
        [](float &stored) { return stored; });
    history.setMaxSize(10);
    int notifications = 0;
    history.onFrameAdded().add([&] { ++notifications; });
    history.addFrame(10, Timestamp(0));
    history.addFrame(20, Timestamp(10000));
    history.addFrame(30, Timestamp(30000));
    history.addFrame(40, Timestamp(60000));
    assert(notifications == 4);
    assert(history.getAvailableHistoryMs() == 60.0f);

    bool overflow = true;
    assert(history.getClosestFrameDelayMs(0, true, &overflow).getObjectRef() == 40 && !overflow);
    assert(history.getClosestFrameDelayMs(1, true, &overflow).getObjectRef() == 40 && !overflow);
    assert(history.getClosestFrameDelayMs(9, true, &overflow).getObjectRef() == 40 && !overflow);
    assert(history.getClosestFrameDelayMs(20, true, &overflow).getObjectRef() == 30 && !overflow);
    assert(history.getClosestFrameDelayMs(60, true, &overflow).getObjectRef() == 10 && !overflow);
    assert(history.getClosestFrameDelayMs(61, true, &overflow).getObjectRef() == 10 && overflow);
    assert(history.getClosestFrameDelayMs(1e30f, true, &overflow).getObjectRef() == 10 && overflow);

    buffer<float> repeated(
        [](float &value, float &stored) { stored = value; },
        [](float &stored) { return stored; });
    repeated.setMaxSize(10);
    repeated.addFrame(10, Timestamp(0));
    repeated.addFrame(20, Timestamp(0));
    repeated.addFrame(30, Timestamp(0));
    assert(repeated.getAvailableHistoryMs() == 0.0f);
    assert(repeated.getClosestFrame(Timestamp(0)).getObjectRef() == 30);
    assert(repeated.getClosestFrameDelayMs(0, true, &overflow).getObjectRef() == 30 && !overflow);
    assert(repeated.getClosestFrameDelayMs(1, true, &overflow).getObjectRef() == 30 && overflow);

    buffer<float> looped(
        [](float &value, float &stored) { stored = value; },
        [](float &stored) { return stored; });
    looped.setMaxSize(10);
    looped.addFrame(10, Timestamp(50000));
    looped.addFrame(20, Timestamp(100000));
    looped.addFrame(30, Timestamp(10000));
    looped.addFrame(40, Timestamp(11000));
    assert(looped.getAvailableHistoryMs() == 1.0f);
    assert(looped.getClosestFrameDelayMs(0, true, &overflow).getObjectRef() == 40 && !overflow);
    assert(looped.getClosestFrameDelayMs(0.5f, true, &overflow).getObjectRef() == 40 && !overflow);
    assert(looped.getClosestFrameDelayMs(1, true, &overflow).getObjectRef() == 30 && !overflow);
    assert(looped.getClosestFrameDelayMs(2, true, &overflow).getObjectRef() == 30 && overflow);
    looped.addFrame(50, Timestamp(12000));
    assert(looped.getAvailableHistoryMs() == 2.0f);
    assert(looped.getClosestFrameDelayMs(1, true, &overflow).getObjectRef() == 40 && !overflow);
    assert(looped.getClosestFrame(Timestamp(11000)).getObjectRef() == 40);

    // A filled 100-sample buffer at 60 Hz contains about 1650 ms of history.
    buffer<float> sampled(
        [](float &value, float &stored) { stored = value; },
        [](float &stored) { return stored; });
    sampled.setMaxSize(100);
    for(int i = 0; i < 100; ++i)
        sampled.addFrame(static_cast<float>(i), Timestamp(static_cast<int64_t>(i) * 16667));
    assert(sampled.getSize() == 100);
    assert(sampled.getAvailableHistoryMs() > 1600.0f);
    assert(sampled.getClosestFrameDelayMs(16, true, &overflow).getObjectRef() == 98 && !overflow);
    assert(sampled.getClosestFrameDelayMs(160, true, &overflow).getObjectRef() == 89 && !overflow);
    assert(sampled.getClosestFrameDelayMs(1650.033f, true, &overflow).getObjectRef() == 0 && !overflow);
    assert(sampled.getClosestFrameDelayMs(1651, true, &overflow).getObjectRef() == 0 && overflow);
}
'''


def main():
    with tempfile.TemporaryDirectory(prefix="buffer-time-") as directory:
        folder = Path(directory)
        source = folder / "buffer_time.cpp"
        binary = folder / "buffer_time"
        source.write_text(SOURCE)
        subprocess.run([
            "clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
            "-I" + str(ROOT / "src/Nodes/Default_Nodes/Buffer"),
            str(source), "-o", str(binary),
        ], check=True)
        subprocess.run([str(binary)], check=True)
    print("Buffer timing regression passed: 16/160 ms at 60 Hz, boundaries, duplicates, transport loops, notifications.")


if __name__ == "__main__":
    main()
