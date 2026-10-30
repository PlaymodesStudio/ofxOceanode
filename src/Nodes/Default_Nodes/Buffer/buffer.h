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
//
// In transport-indexed mode (setTransportIndexed) the same slots hold a sparse
// timeline instead of a recording order: samples are keyed by transport time,
// recording replaces whatever was stored over the span just played, reads are
// anchored at the playhead and ignore samples ahead of it, and a full buffer
// evicts samples ahead of the playhead first, then the oldest. Replaying a
// loop or scrubbing back over played material therefore refreshes slots in
// place and never grows the buffer.
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
        if(transportIndexed){
            const size_t count = viewEnd();
            if(position >= 0 && static_cast<size_t>(position) < count){
                return frames[order[fromTail ? count - 1 - static_cast<size_t>(position)
                                             : static_cast<size_t>(position)]];
            }
            return frames[order.empty() ? 0 : order[0]];
        }
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
        if(transportIndexed){
            if(order.empty()) return frames[0];
            return frames[order[nearestInOrder(order.size(), targetUs)]];
        }
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
        if(transportIndexed) return closestTransportDelay(ms, overflow);
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
        if(transportIndexed) return frames[order[viewEnd() - 1]].getTimestamp();
        return frameAt(getSize()-1).getTimestamp();
    }

    Timestamp   getLastFrameTimestamp(){
        if(getSize() == 0) return Timestamp();
        if(transportIndexed) return frames[order[0]].getTimestamp();
        return frameAt(0).getTimestamp();
    }

    void addFrame(T1 _object){
        Timestamp now;
        addFrame(_object, now);
    }

    void addFrame(T1 _object, Timestamp _timestamp){
        const int64_t stampUs = _timestamp.epochMicrosecondsSigned();
        if(transportIndexed){
            addTransportFrame(_object, stampUs, stampUs - 1);
            return;
        }
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

    // Transport-indexed recording: stores the sample at its transport time and
    // replaces every sample stored in (replaceFromExclusive, timestamp], plus
    // any at exactly that time. Continuous playback passes the previous
    // sample's time; after a seek, stop or loop wrap the caller passes a time
    // one frame earlier so only the frame at the new position is replaced.
    // Outside transport-indexed mode the span is ignored.
    void addFrame(T1 _object, Timestamp _timestamp, Timestamp replaceFromExclusive){
        if(!transportIndexed){
            addFrame(_object, _timestamp);
            return;
        }
        addTransportFrame(_object, _timestamp.epochMicrosecondsSigned(),
                          replaceFromExclusive.epochMicrosecondsSigned());
    }

    void setMaxSize(int _maxSize){
        const size_t newMaxSize = static_cast<size_t>(std::max(1, _maxSize));
        if(newMaxSize == maxSize) return;
        if(transportIndexed){
            const int64_t anchorUs = hasPlayhead ? playheadUs
                : (order.empty() ? 0 : stampsUs[order.back()]);
            while(order.size() > newMaxSize) freeSlots.push_back(evictTransportSlot(anchorUs));
            if(frames.size() > newMaxSize) compactTransport();
            maxSize = newMaxSize;
            ++revisionCount;
            return;
        }
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

    // Transport-indexed: the samples at or before the playhead.
    size_t getSize(){
        if(transportIndexed) return viewEnd();
        return frames.size();
    }

    float getAvailableHistoryMs(){
        if(transportIndexed) return transportHistoryMs();
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
        order.clear();
        freeSlots.clear();
        head = 0;
        inversions = 0;
        ++revisionCount;
    }

    // ---- Transport-indexed mode ----

    // Switching modes empties the history: wall-clock and transport stamps
    // do not share a time base.
    void setTransportIndexed(bool enabled){
        if(enabled == transportIndexed) return;
        clear();
        transportIndexed = enabled;
        hasPlayhead = false;
    }
    bool isTransportIndexed() const { return transportIndexed; }

    // The transport position reads are anchored at. Samples ahead of it are
    // hidden from getSize, getFrame and the delay reads.
    void setPlayhead(Timestamp playhead){
        const int64_t us = playhead.epochMicrosecondsSigned();
        if(hasPlayhead && us == playheadUs) return;
        hasPlayhead = true;
        playheadUs = us;
        ++revisionCount;
    }

    // Largest distance between a requested transport time and the nearest
    // stored sample that still counts as a hit; beyond it a read overflows,
    // and a larger gap between consecutive samples ends the available history.
    void setGapToleranceMs(float ms){
        const int64_t us = static_cast<int64_t>(std::round(std::max(0.0, static_cast<double>(ms)) * 1000.0));
        if(us == gapToleranceUs) return;
        gapToleranceUs = us;
        ++revisionCount;
    }

    // Changes whenever the stored history changes. Readers compare it to skip
    // recomputing an unchanged result.
    uint64_t revision() const { return revisionCount; }

    // True while stored timestamps never decrease in recording order.
    bool isOrdered() const { return transportIndexed || inversions == 0; }

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

    // ---- Transport-indexed helpers. `order` lists occupied physical slots
    // sorted by stamp; equal stamps keep the later-recorded one last.

    size_t firstSlotAbove(int64_t value) const { // first order index with stamp > value
        return static_cast<size_t>(std::upper_bound(order.begin(), order.end(), value,
            [this](int64_t v, uint32_t slot){ return v < stampsUs[slot]; }) - order.begin());
    }
    size_t firstSlotAtLeast(int64_t value) const { // first order index with stamp >= value
        return static_cast<size_t>(std::lower_bound(order.begin(), order.end(), value,
            [this](uint32_t slot, int64_t v){ return stampsUs[slot] < v; }) - order.begin());
    }
    // Number of ordered samples at or before the playhead.
    size_t viewEnd() const {
        if(!hasPlayhead) return order.size();
        return firstSlotAbove(playheadUs);
    }
    // Order index of the sample nearest to targetUs among the first `count`;
    // equal distances prefer the later sample. count must be > 0.
    size_t nearestInOrder(size_t count, int64_t targetUs) const {
        const size_t above = std::min(count, firstSlotAbove(targetUs));
        if(above == 0) return 0;
        if(above == count) return count - 1;
        const int64_t belowDistance = targetUs - stampsUs[order[above - 1]];
        const int64_t aboveDistance = stampsUs[order[above]] - targetUs;
        return aboveDistance <= belowDistance ? above : above - 1;
    }
    // Frees one occupied slot for a sample at anchorUs: a sample ahead of the
    // anchor if any (the farthest first), otherwise the oldest.
    uint32_t evictTransportSlot(int64_t anchorUs){
        const bool ahead = stampsUs[order.back()] > anchorUs;
        const uint32_t slot = ahead ? order.back() : order.front();
        order.erase(ahead ? order.end() - 1 : order.begin());
        return slot;
    }
    void addTransportFrame(T1 &_object, int64_t stampUs, int64_t fromExclusiveUs){
        // Replace what the transport just played over, and anything at this time.
        const int64_t lowerExclusiveUs = std::min(fromExclusiveUs, stampUs - 1);
        const size_t first = firstSlotAbove(lowerExclusiveUs);
        const size_t last = firstSlotAbove(stampUs);
        for(size_t i = first; i < last; ++i) freeSlots.push_back(order[i]);
        order.erase(order.begin() + first, order.begin() + last);

        const Timestamp timestamp(stampUs);
        uint32_t slot;
        if(!freeSlots.empty()){
            slot = freeSlots.back();
            freeSlots.pop_back();
            frames[slot].update(_object, timestamp);
        }else if(frames.size() < maxSize){
            slot = static_cast<uint32_t>(frames.size());
            frames.emplace_back(functions.get(), _object, timestamp);
            stampsUs.push_back(stampUs);
        }else{
            slot = evictTransportSlot(stampUs);
            frames[slot].update(_object, timestamp);
        }
        stampsUs[slot] = stampUs;
        order.insert(order.begin() + firstSlotAbove(stampUs), slot);
        ++revisionCount;
        ofNotifyEvent(frameAdded);
    }
    // Drops free slots so the physical storage matches the occupied samples.
    void compactTransport(){
        std::deque<Frame> kept;
        std::vector<int64_t> keptStamps;
        keptStamps.reserve(order.size());
        for(uint32_t slot : order){
            kept.emplace_back(std::move(frames[slot]));
            keptStamps.push_back(stampsUs[slot]);
        }
        frames.swap(kept);
        stampsUs.swap(keptStamps);
        for(size_t i = 0; i < order.size(); ++i) order[i] = static_cast<uint32_t>(i);
        freeSlots.clear();
    }
    Frame &closestTransportDelay(float ms, bool* overflow){
        const size_t count = viewEnd();
        if(count == 0){
            if(overflow) *overflow = true;
            return frames[order.empty() ? 0 : order[0]];
        }
        const int64_t anchorUs = hasPlayhead ? playheadUs : stampsUs[order[count - 1]];
        // Timestamps have microsecond resolution; NaN and negative delays read
        // the playhead.
        long double delayUs = std::round(static_cast<long double>(ms) * 1000.0L);
        if(!(delayUs > 0.0L)) delayUs = 0.0L;
        const long double oldestUs = static_cast<long double>(stampsUs[order[0]]);
        const long double targetUs = static_cast<long double>(anchorUs) - delayUs;
        size_t index;
        if(targetUs < oldestUs){
            index = 0;
        }else{
            index = nearestInOrder(count, static_cast<int64_t>(targetUs));
        }
        const long double distance = std::fabs(static_cast<long double>(stampsUs[order[index]]) - targetUs);
        if(overflow) *overflow = distance > static_cast<long double>(gapToleranceUs);
        return frames[order[index]];
    }
    // Span, in ms, from the playhead back over consecutive samples no farther
    // apart than the gap tolerance. Zero when nothing is stored near the playhead.
    float transportHistoryMs(){
        if(historyCacheRevision == revisionCount) return historyCacheMs;
        historyCacheRevision = revisionCount;
        historyCacheMs = 0.0f;
        const size_t count = viewEnd();
        if(count == 0) return historyCacheMs;
        const int64_t anchorUs = hasPlayhead ? playheadUs : stampsUs[order[count - 1]];
        if(anchorUs - stampsUs[order[count - 1]] > gapToleranceUs) return historyCacheMs;
        size_t oldest = count - 1;
        while(oldest > 0 && stampsUs[order[oldest]] - stampsUs[order[oldest - 1]] <= gapToleranceUs) --oldest;
        historyCacheMs = static_cast<float>(static_cast<long double>(anchorUs - stampsUs[order[oldest]]) / 1000.0L);
        return historyCacheMs;
    }

    size_t maxSize;
    size_t head = 0;           // Physical index of the oldest frame.
    size_t inversions = 0;     // Adjacent pairs recorded out of timestamp order.
    uint64_t revisionCount = 0;
    ofEvent<void> frameAdded;

    std::unique_ptr<bufferFunctions<T1, T2>> functions;

    std::deque<Frame> frames;      // Stable addresses: frames never move on insert.
    std::vector<int64_t> stampsUs; // Same physical layout as frames.

    // Transport-indexed mode only.
    bool transportIndexed = false;
    bool hasPlayhead = false;
    int64_t playheadUs = 0;
    int64_t gapToleranceUs = 100000;
    std::vector<uint32_t> order;     // Occupied slots sorted by stamp.
    std::vector<uint32_t> freeSlots; // Slots whose sample was replaced.
    uint64_t historyCacheRevision = std::numeric_limits<uint64_t>::max();
    float historyCacheMs = 0.0f;
};


#endif /* buffer_h */
