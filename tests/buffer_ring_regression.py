#!/usr/bin/env python3
"""Compare the ring Buffer with the previous deque implementation on random histories.

The reference below is the Buffer before it became a ring with ordered-timestamp
binary search: a deque rotated on every insert, with newest-first linear scans.
Every lookup must give the same frame for the same history and query, whether
the timestamps are in order or not.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

REFERENCE = r"""
template <typename T1, typename T2 = T1>
class referenceBuffer {
public:
    referenceBuffer(std::function<void(T1&, T2&)> _assignFunction, std::function<T1(T2&)> _returnFunction) : assignFunction(_assignFunction), returnFunction(_returnFunction){
        maxSize = 1;
    }
    ~referenceBuffer(){}
    
    referenceFrame<T1, T2> &getFrame(int position, bool fromTail = false){
        if(position >= 0 && static_cast<size_t>(position) < getSize()){
            if(fromTail){
                return frames[getSize()-1-static_cast<size_t>(position)];
            }else{
                return frames[static_cast<size_t>(position)];
            }
        }
        return frames[0];
    }
    
    referenceFrame<T1, T2> &getFrame(float pct){
        return frames[pct / getSize()];
    }
    
    referenceFrame<T1, T2> &getClosestFrame(Timestamp timestamp){
        const long double targetUs = timestamp.epochMicrosecondsSigned();
        size_t closest = frames.size() - 1;
        long double bestDistance = std::numeric_limits<long double>::infinity();
        // Recording order and timestamp order can differ after a transport seek.
        // Search newest first so identical timestamps prefer the latest sample.
        for(size_t i = frames.size(); i-- > 0;){
            const long double stampUs = frames[i].getTimestamp().epochMicrosecondsSigned();
            const long double distance = std::fabs(stampUs - targetUs);
            if(distance < bestDistance){
                bestDistance = distance;
                closest = i;
            }
        }
        return frames[closest];
    }
    
    referenceFrame<T1, T2> &getClosestFrameDelayMs(float ms, bool relative, bool* overflow = nullptr){
        if(overflow) *overflow = false;
        if(relative){
            const size_t newest = frames.size() - 1;
            if(ms <= 0.0f) return frames[newest];

            const long double newestUs = frames[newest].getTimestamp().epochMicrosecondsSigned();
            // Timestamps have microsecond resolution. Round the float input to
            // that resolution so a request at the oldest sample stays in range.
            const long double delayUs = std::round(static_cast<long double>(ms) * 1000.0L);
            size_t closest = newest;
            size_t oldestByTime = newest;
            long double bestDistance = std::numeric_limits<long double>::infinity();
            long double maximumAgeUs = 0;
            for(size_t i = frames.size(); i-- > 0;){
                const long double stampUs = frames[i].getTimestamp().epochMicrosecondsSigned();
                const long double ageUs = newestUs - stampUs;
                if(ageUs < 0) continue; // A prior transport cycle may have a later timestamp.
                if(ageUs > maximumAgeUs){
                    maximumAgeUs = ageUs;
                    oldestByTime = i;
                }
                const long double distance = std::fabs(ageUs - delayUs);
                if(distance < bestDistance){
                    bestDistance = distance;
                    closest = i;
                }
            }
            const bool beyondHistory = delayUs > maximumAgeUs;
            if(overflow) *overflow = beyondHistory;
            return frames[beyondHistory ? oldestByTime : closest];
        }

        Timestamp now;
        const long double targetUs = static_cast<long double>(now.epochMicrosecondsSigned())
            - static_cast<long double>(ms) * 1000.0L;
        size_t closest = frames.size() - 1;
        long double bestDistance = std::numeric_limits<long double>::infinity();
        for(size_t i = frames.size(); i-- > 0;){
            const long double stampUs = frames[i].getTimestamp().epochMicrosecondsSigned();
            const long double distance = std::fabs(stampUs - targetUs);
            if(distance < bestDistance){
                bestDistance = distance;
                closest = i;
            }
        }
        return frames[closest];
    }
    
    Timestamp   getFirstFrameTimestamp(){
        if(frames.size() == 0) return Timestamp();
        return frames[frames.size()-1].getTimestamp();
    }
    
    Timestamp   getLastFrameTimestamp(){
        if(frames.size() == 0) return Timestamp();
        return frames[0].getTimestamp();
    }
    
    void addFrame(T1 _object){
        Timestamp now;
        addFrame(_object, now);
    }
    
    void addFrame(T1 _object, Timestamp _timestamp){
        if(frames.size() >= maxSize){
            std::rotate(frames.begin(), frames.begin() + 1, frames.end());
            frames.back().update(_object, _timestamp);
        }else{
            frames.emplace_back(_object, _timestamp, assignFunction, returnFunction);
        }
        
        if(frames.size() > maxSize){
            frames.pop_front();
        }
        ofNotifyEvent(frameAdded);
    }
    
    void setMaxSize(int _maxSize){
        const size_t newMaxSize = static_cast<size_t>(std::max(1, _maxSize));
        while(frames.size() > newMaxSize){
            frames.pop_front();
        }
        maxSize = newMaxSize;
    }
    
    size_t getSize(){
        return frames.size();
    }

    float getAvailableHistoryMs(){
        if(frames.empty()) return 0.0f;
        const long double newestUs = frames.back().getTimestamp().epochMicrosecondsSigned();
        long double maximumAgeUs = 0;
        for(auto &frame : frames){
            const long double ageUs = newestUs - frame.getTimestamp().epochMicrosecondsSigned();
            if(ageUs > maximumAgeUs) maximumAgeUs = ageUs;
        }
        return static_cast<float>(maximumAgeUs / 1000.0L);
    }

    ofEvent<void>& onFrameAdded(){
        return frameAdded;
    }
	
	void clear(){
		frames.clear();
	}
    
private:
    size_t maxSize;
    ofEvent<void> frameAdded;
    
    std::function<void(T1&, T2&)> assignFunction;
    std::function<T1(T2&)> returnFunction;
    
    std::deque<referenceFrame<T1, T2>> frames;
};


"""

SOURCE = r"""
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <random>
#include <set>
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
template<> struct ofEvent<void> { int notifications = 0; };
void ofNotifyEvent(ofEvent<void> &event) { ++event.notifications; }
#include "buffer.h"

template<typename T1, typename T2 = T1> struct referenceFrame {
    T1 value;
    Timestamp timestamp;
    using Assign = function<void(T1&, T2&)>;
    using Return = function<T1(T2&)>;
    referenceFrame(T1 item, Timestamp time, Assign, Return) : value(item), timestamp(time) {}
    void update(T1 item, Timestamp time) { value = item; timestamp = time; }
    T1 getObjectRef() { return value; }
    Timestamp getTimestamp() { return timestamp; }
};
REFERENCE_IMPLEMENTATION

// Each stored sample remembers which slot held it, so slot reuse is observable.
struct Slot { int value = 0; int writes = 0; };

int main() {
    mt19937 rng(1234);
    auto assign = [](int &value, int &stored) { stored = value; };
    auto fetch = [](int &stored) { return stored; };
    for(int round = 0; round < 300; ++round) {
        buffer<int> ring(assign, fetch);
        referenceBuffer<int> reference(assign, fetch);
        const int maxSize = 1 + rng() % (round % 3 == 0 ? 3 : 40);
        ring.setMaxSize(maxSize); reference.setMaxSize(maxSize);
        int64_t clock = 1000000;
        int value = 0;
        const bool monotonicRound = round % 2 == 0;
        for(int step = 0; step < 200; ++step) {
            const int action = rng() % 100;
            if(action < 70) {
                // Record: mostly forward in time, with duplicates and seeks.
                const int kind = rng() % 20;
                if(kind == 0 && !monotonicRound) clock -= 1 + rng() % 50000;   // transport seek backwards
                else if(kind == 1) clock += 0;                                 // identical timestamp
                else clock += rng() % 20000;
                ring.addFrame(++value, Timestamp(clock)); reference.addFrame(value, Timestamp(clock));
            } else if(action < 78) {
                const int next = 1 + rng() % 40;
                ring.setMaxSize(next); reference.setMaxSize(next);
            } else if(action < 80) {
                ring.clear(); reference.clear();
            }
            assert(ring.getSize() == reference.getSize());
            if(monotonicRound) assert(ring.isOrdered());
            if(ring.getSize() == 0) continue;
            const size_t size = ring.getSize();
            for(size_t i = 0; i < size + 2; ++i) {
                assert(ring.getFrame(int(i), false).getObjectRef() == reference.getFrame(int(i), false).getObjectRef());
                assert(ring.getFrame(int(i), true).getObjectRef() == reference.getFrame(int(i), true).getObjectRef());
                assert(ring.getFrame(int(i), true).getTimestamp().us == reference.getFrame(int(i), true).getTimestamp().us);
            }
            assert(ring.getFrame(-1, true).getObjectRef() == reference.getFrame(-1, true).getObjectRef());
            assert(ring.getAvailableHistoryMs() == reference.getAvailableHistoryMs());
            assert(ring.getFirstFrameTimestamp().us == reference.getFirstFrameTimestamp().us);
            assert(ring.getLastFrameTimestamp().us == reference.getLastFrameTimestamp().us);
            const int64_t newest = ring.getFrame(0, true).getTimestamp().us;
            const int64_t oldest = ring.getFrame(0, false).getTimestamp().us;
            vector<float> delays = {0.f, -1.f, 0.0004f, 0.0006f, 1e30f, numeric_limits<float>::infinity(),
                                    numeric_limits<float>::quiet_NaN(), ring.getAvailableHistoryMs(),
                                    ring.getAvailableHistoryMs() + 0.001f, ring.getAvailableHistoryMs() - 0.001f};
            vector<int64_t> targets = {newest, oldest, newest + 1, oldest - 1, 0};
            for(int q = 0; q < 12; ++q) {
                delays.push_back(float(rng() % 60000) / 1000.f);
                const int64_t stamp = ring.getFrame(int(rng() % size), true).getTimestamp().us;
                delays.push_back(float(newest - stamp) / 1000.f);
                targets.push_back(stamp + int64_t(rng() % 7) - 3);
                targets.push_back(stamp);
            }
            for(float ms : delays) {
                bool ringOverflow = true, referenceOverflow = false;
                auto &a = ring.getClosestFrameDelayMs(ms, true, &ringOverflow);
                auto &b = reference.getClosestFrameDelayMs(ms, true, &referenceOverflow);
                if(a.getObjectRef() != b.getObjectRef() || ringOverflow != referenceOverflow) {
                    fprintf(stderr, "round %d step %d delay %g: ring %d/%d reference %d/%d\n", round, step, ms,
                            a.getObjectRef(), ringOverflow, b.getObjectRef(), referenceOverflow);
                    abort();
                }
            }
            for(int64_t target : targets) {
                assert(ring.getClosestFrame(Timestamp(target)).getObjectRef() ==
                       reference.getClosestFrame(Timestamp(target)).getObjectRef());
            }
        }
    }

    // Frames never move once the ring is full, and the oldest slot's container
    // is rewritten in place for the new sample.
    buffer<int, Slot> slots([](int &value, Slot &slot) { slot.value = value; ++slot.writes; },
                            [](Slot &slot) { return slot.value; });
    slots.setMaxSize(4);
    for(int i = 0; i < 4; ++i) slots.addFrame(i, Timestamp(i));
    set<const void*> addresses;
    for(int i = 0; i < 4; ++i) addresses.insert(&slots.getFrame(i));
    for(int i = 4; i < 40; ++i) {
        slots.addFrame(i, Timestamp(i));
        for(int j = 0; j < 4; ++j) assert(addresses.count(&slots.getFrame(j)));
    }
    int writes = 0;
    for(int i = 0; i < 4; ++i) writes += slots.getFrame(i).getContainer().writes;
    assert(writes == 40);
    assert(slots.getFrame(0, true).getObjectRef() == 39 && slots.getFrame(3, true).getObjectRef() == 36);
    assert(slots.onFrameAdded().notifications == 40);

    // The revision changes with the history, not with reads.
    buffer<float> history([](float &value, float &stored) { stored = value; }, [](float &stored) { return stored; });
    history.setMaxSize(2);
    const uint64_t initial = history.revision();
    history.addFrame(1.f, Timestamp(0));
    assert(history.revision() != initial);
    uint64_t before = history.revision();
    history.getFrame(0, true); history.getAvailableHistoryMs(); history.getClosestFrameDelayMs(0, true);
    assert(history.revision() == before);
    history.clear();
    assert(history.revision() != before);
    before = history.revision();
    history.setMaxSize(3);
    assert(history.revision() != before);
    before = history.revision();
    history.setMaxSize(3); // Unchanged size leaves the history as it is.
    assert(history.revision() == before);
    return 0;
}
"""


def main():
    source_text = SOURCE.replace("REFERENCE_IMPLEMENTATION", REFERENCE)
    with tempfile.TemporaryDirectory(prefix="buffer-ring-") as directory:
        folder = Path(directory)
        source = folder / "buffer_ring.cpp"
        binary = folder / "buffer_ring"
        source.write_text(source_text)
        subprocess.run([
            "clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
            "-I" + str(ROOT / "src/Nodes/Default_Nodes/Buffer"),
            "-I" + str(ROOT / "src"),
            str(source), "-o", str(binary),
        ], check=True)
        subprocess.run([str(binary)], check=True)
    print("Buffer ring regression passed: lookups match the previous deque implementation for ordered and seeked histories, slots are reused in place, revision tracks the history.")


if __name__ == "__main__":
    main()
