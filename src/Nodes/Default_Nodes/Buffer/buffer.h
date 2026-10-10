//
//  buffer.h
//  ofxOceanode
//
//  Created by Eduard Frigola on 20/12/23.
//

#ifndef buffer_h
#define buffer_h

#include "frame.h"
#include <cmath>
#include <limits>

template <typename T1, typename T2 = T1>
class buffer {
public:
    buffer(std::function<void(T1&, T2&)> _assignFunction, std::function<T1(T2&)> _returnFunction) : assignFunction(_assignFunction), returnFunction(_returnFunction){
        maxSize = 1;
    }
    ~buffer(){}
    
    bufferFrame<T1, T2> &getFrame(int position, bool fromTail = false){
        if(position >= 0 && static_cast<size_t>(position) < getSize()){
            if(fromTail){
                return frames[getSize()-1-static_cast<size_t>(position)];
            }else{
                return frames[static_cast<size_t>(position)];
            }
        }
        return frames[0];
    }
    
    bufferFrame<T1, T2> &getFrame(float pct){
        return frames[pct / getSize()];
    }
    
    bufferFrame<T1, T2> &getClosestFrame(Timestamp timestamp){
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
    
    bufferFrame<T1, T2> &getClosestFrameDelayMs(float ms, bool relative, bool* overflow = nullptr){
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
    
    std::deque<bufferFrame<T1, T2>> frames;
};


#endif /* buffer_h */
