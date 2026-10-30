//
//  buffer.h
//  ofxOceanode
//
//  Created by Eduard Frigola on 20/12/23.
//

#ifndef buffer_h
#define buffer_h

#include "frame.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <vector>

// A bounded history of samples. Frames live in a ring: the oldest slot is
// overwritten when the buffer is full, so adding a sample never moves the
// other frames and a frame's container (an fbo, a vector) is reused in place.
// Timestamps are mirrored in a contiguous microsecond array for the lookups,
// and the buffer tracks whether they are in order so a lookup can binary
// search; after a transport seek they may not be, and the original linear scan
// is used instead with identical results.
template <typename T1, typename T2 = T1>
class buffer {
public:
    using Frame = bufferFrame<T1, T2>;

    buffer(std::function<void(T1&, T2&)> _assignFunction, std::function<T1(T2&)> _returnFunction)
    : functions(std::make_unique<bufferFunctions<T1, T2>>(bufferFunctions<T1, T2>{std::move(_assignFunction), std::move(_returnFunction)})){
        maxSize = 1;
    }
    ~buffer(){}

    buffer(const buffer&) = delete;
    buffer &operator=(const buffer&) = delete;
    buffer(buffer&&) = default;
    buffer &operator=(buffer&&) = default;

    // Logical position 0 is the oldest frame; fromTail counts from the newest.
    Frame &getFrame(int position, bool fromTail = false){
        if(position >= 0 && static_cast<size_t>(position) < getSize()){
            if(fromTail){
                return frameAt(getSize()-1-static_cast<size_t>(position));
            }else{
                return frameAt(static_cast<size_t>(position));
            }
        }
        return frameAt(0);
    }

    Frame &getFrame(float pct){
        return getFrame(static_cast<int>(pct / getSize()));
    }

    Frame &getClosestFrame(Timestamp timestamp){
        const int64_t targetUs = timestamp.epochMicrosecondsSigned();
        if(isOrdered()) return frameAt(nearestOrdered(targetUs));
        // Recording order and timestamp order can differ after a transport seek.
        // Search newest first so identical timestamps prefer the latest sample.
        size_t closest = getSize() - 1;
        int64_t bestDistance = std::numeric_limits<int64_t>::max();
        for(size_t i = getSize(); i-- > 0;){
            const int64_t distance = absoluteDifference(stampAt(i), targetUs);
            if(distance < bestDistance){
                bestDistance = distance;
                closest = i;
            }
        }
        return frameAt(closest);
    }

    Frame &getClosestFrameDelayMs(float ms, bool relative, bool* overflow = nullptr){
        if(overflow) *overflow = false;
        if(relative){
            const size_t newest = getSize() - 1;
            if(ms <= 0.0f) return frameAt(newest);

            const int64_t newestUs = stampAt(newest);
            // Timestamps have microsecond resolution. Round the float input to
            // that resolution so a request at the oldest sample stays in range.
            const long double delayUs = std::round(static_cast<long double>(ms) * 1000.0L);

            if(isOrdered()){
                const int64_t maximumAgeUs = newestUs - stampAt(0);
                const bool beyondHistory = delayUs > static_cast<long double>(maximumAgeUs);
                if(overflow) *overflow = beyondHistory;
                // Among frames sharing the oldest timestamp prefer the newest one.
                if(beyondHistory) return frameAt(upperBoundOrdered(stampAt(0)) - 1);
                if(!(delayUs >= 0.0L)) return frameAt(newest); // NaN delay reads the newest sample.
                return frameAt(nearestOrdered(newestUs - static_cast<int64_t>(delayUs)));
            }

            size_t closest = newest;
            size_t oldestByTime = newest;
            long double bestDistance = std::numeric_limits<long double>::infinity();
            int64_t maximumAgeUs = 0;
            for(size_t i = getSize(); i-- > 0;){
                const int64_t ageUs = newestUs - stampAt(i);
                if(ageUs < 0) continue; // A prior transport cycle may have a later timestamp.
                if(ageUs > maximumAgeUs){
                    maximumAgeUs = ageUs;
                    oldestByTime = i;
                }
                const long double distance = std::fabs(static_cast<long double>(ageUs) - delayUs);
                if(distance < bestDistance){
                    bestDistance = distance;
                    closest = i;
                }
            }
            const bool beyondHistory = delayUs > static_cast<long double>(maximumAgeUs);
            if(overflow) *overflow = beyondHistory;
            return frameAt(beyondHistory ? oldestByTime : closest);
        }

        Timestamp now;
        const long double targetUs = static_cast<long double>(now.epochMicrosecondsSigned())
            - static_cast<long double>(ms) * 1000.0L;
        size_t closest = getSize() - 1;
        long double bestDistance = std::numeric_limits<long double>::infinity();
        for(size_t i = getSize(); i-- > 0;){
            const long double distance = std::fabs(static_cast<long double>(stampAt(i)) - targetUs);
            if(distance < bestDistance){
                bestDistance = distance;
                closest = i;
            }
        }
        return frameAt(closest);
    }

    Timestamp   getFirstFrameTimestamp(){
        if(getSize() == 0) return Timestamp();
        return frameAt(getSize()-1).getTimestamp();
    }

    Timestamp   getLastFrameTimestamp(){
        if(getSize() == 0) return Timestamp();
        return frameAt(0).getTimestamp();
    }

    void addFrame(T1 _object){
        Timestamp now;
        addFrame(_object, now);
    }

    void addFrame(T1 _object, Timestamp _timestamp){
        const int64_t stampUs = _timestamp.epochMicrosecondsSigned();
        const size_t size = frames.size();
        if(size >= maxSize){
            // Full: the oldest slot becomes the newest sample.
            if(size >= 2 && stampAt(0) > stampAt(1)) --inversions;
            const int64_t previousNewestUs = stampAt(size - 1);
            frames[head].update(_object, _timestamp);
            stampsUs[head] = stampUs;
            head = head + 1 == size ? 0 : head + 1;
            if(size >= 2 && stampUs < previousNewestUs) ++inversions;
        }else{
            if(size >= 1 && stampUs < stampAt(size - 1)) ++inversions;
            frames.emplace_back(functions.get(), _object, _timestamp);
            stampsUs.push_back(stampUs);
        }
        ++revisionCount;
        ofNotifyEvent(frameAdded);
    }

    void setMaxSize(int _maxSize){
        const size_t newMaxSize = static_cast<size_t>(std::max(1, _maxSize));
        if(newMaxSize == maxSize) return;
        linearize();
        if(frames.size() > newMaxSize){
            const size_t drop = frames.size() - newMaxSize;
            frames.erase(frames.begin(), frames.begin() + drop);
            stampsUs.erase(stampsUs.begin(), stampsUs.begin() + drop);
            recountInversions();
        }
        maxSize = newMaxSize;
        ++revisionCount;
    }

    size_t getSize(){
        return frames.size();
    }

    float getAvailableHistoryMs(){
        if(frames.empty()) return 0.0f;
        const int64_t newestUs = stampAt(getSize() - 1);
        int64_t maximumAgeUs = 0;
        if(isOrdered()){
            maximumAgeUs = newestUs - stampAt(0);
        }else{
            for(size_t i = 0; i < getSize(); ++i){
                const int64_t ageUs = newestUs - stampAt(i);
                if(ageUs > maximumAgeUs) maximumAgeUs = ageUs;
            }
        }
        return static_cast<float>(static_cast<long double>(maximumAgeUs) / 1000.0L);
    }

    ofEvent<void>& onFrameAdded(){
        return frameAdded;
    }

    void clear(){
        frames.clear();
        stampsUs.clear();
        head = 0;
        inversions = 0;
        ++revisionCount;
    }

    // Changes whenever the stored history changes. Readers compare it to skip
    // recomputing an unchanged result.
    uint64_t revision() const { return revisionCount; }

    // True while stored timestamps never decrease in recording order.
    bool isOrdered() const { return inversions == 0; }

private:
    size_t physical(size_t logical) const {
        const size_t index = head + logical;
        return index >= frames.size() ? index - frames.size() : index;
    }
    Frame &frameAt(size_t logical){ return frames[physical(logical)]; }
    int64_t stampAt(size_t logical) const { return stampsUs[physical(logical)]; }

    static int64_t absoluteDifference(int64_t a, int64_t b){
        return a > b ? a - b : b - a;
    }

    // Rotates the ring so logical and physical order coincide.
    void linearize(){
        if(head == 0) return;
        std::rotate(frames.begin(), frames.begin() + head, frames.end());
        std::rotate(stampsUs.begin(), stampsUs.begin() + head, stampsUs.end());
        head = 0;
    }

    void recountInversions(){
        inversions = 0;
        for(size_t i = 1; i < stampsUs.size(); ++i){
            if(stampAt(i) < stampAt(i - 1)) ++inversions;
        }
    }

    // First logical index whose timestamp is greater than value (ordered only).
    size_t upperBoundOrdered(int64_t value) const {
        size_t low = 0;
        size_t count = stampsUs.size();
        while(count > 0){
            const size_t step = count / 2;
            if(!(value < stampAt(low + step))){
                low += step + 1;
                count -= step + 1;
            }else{
                count = step;
            }
        }
        return low;
    }

    // Logical index of the frame nearest to targetUs, matching the newest-first
    // linear scan: equal distances and equal timestamps prefer the newer frame.
    size_t nearestOrdered(int64_t targetUs) const {
        const size_t size = stampsUs.size();
        const size_t firstAbove = upperBoundOrdered(targetUs);
        if(firstAbove == 0) return upperBoundOrdered(stampAt(0)) - 1;
        if(firstAbove == size) return size - 1;
        const size_t below = firstAbove - 1;
        const size_t above = upperBoundOrdered(stampAt(firstAbove)) - 1;
        const int64_t belowDistance = targetUs - stampAt(below);
        const int64_t aboveDistance = stampAt(above) - targetUs;
        return aboveDistance <= belowDistance ? above : below;
    }

    size_t maxSize;
    size_t head = 0;           // Physical index of the oldest frame.
    size_t inversions = 0;     // Adjacent pairs recorded out of timestamp order.
    uint64_t revisionCount = 0;
    ofEvent<void> frameAdded;

    std::unique_ptr<bufferFunctions<T1, T2>> functions;

    std::deque<Frame> frames;      // Stable addresses: frames never move on insert.
    std::vector<int64_t> stampsUs; // Same physical layout as frames.
};


#endif /* buffer_h */
