//
//  buffer.h
//  ofxOceanode
//
//  Created by Eduard Frigola on 20/12/23.
//

#ifndef buffer_h
#define buffer_h

#include "frame.h"

template <typename T1, typename T2 = T1>
class buffer {
public:
    buffer(std::function<void(T1&, T2&)> _assignFunction, std::function<T1(T2&)> _returnFunction) : assignFunction(_assignFunction), returnFunction(_returnFunction){
        maxSize = 1;
    }
    ~buffer(){}
    
    bufferFrame<T1, T2> &getFrame(int position, bool fromTail = false){
        if(position < getSize()){
            if(fromTail){
                return frames[getSize()-1-position];
            }else{
                return frames[position];
            }
        }
        return frames[0];
    }
    
    bufferFrame<T1, T2> &getFrame(float pct){
        return frames[pct / getSize()];
    }
    
    bufferFrame<T1, T2> &getClosestFrame(Timestamp timestamp){
        int left = 0;
        int right = frames.size() - 1;
        
        while (left <= right) {
            int mid = left + (right - left) / 2;
            auto& midFrame = frames[mid];
            
            if (midFrame.getTimestamp() == timestamp) {
                // Found the frame with the exact timestamp
                return midFrame;
            }
            
            if (midFrame.getTimestamp() < timestamp) {
                left = mid + 1;
            } else {
                right = mid - 1;
            }
        }
        
        // At this point, 'left' and 'right' are the closest indices, one on each side of 'ts'.
        // Determine which one is the closest to 'ts'.
        if (right < 0) {
            return frames[0];
        }
        if (left >= frames.size()) {
            return frames.back();
        }
        
        auto& leftFrame = frames[left];
        auto& rightFrame = frames[right];
        long long leftDiff = std::abs((long long)timestamp.epochMicroseconds() - (long long)leftFrame.getTimestamp().epochMicroseconds());
        long long rightDiff = std::abs((long long)timestamp.epochMicroseconds() - (long long)rightFrame.getTimestamp().epochMicroseconds());
        
        return leftDiff < rightDiff ? leftFrame : rightFrame;
    }
    
    bufferFrame<T1, T2> &getClosestFrameDelayMs(float ms, bool relative){
        Timestamp timestampToCompare;
        timestampToCompare.update(); //Get now;
        if(relative){
            timestampToCompare = getFirstFrameTimestamp();
        }
        timestampToCompare.substractMs(ms);
        return getClosestFrame(timestampToCompare);
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
    }
    
    void setMaxSize(int _maxSize){
        while(frames.size() > _maxSize){
            frames.pop_front();
        }
        maxSize = _maxSize;
    }
    
    size_t getSize(){
        return frames.size();
    }
	
	void clear(){
		frames.clear();
	}
    
private:
    size_t maxSize;
    
    std::function<void(T1&, T2&)> assignFunction;
    std::function<T1(T2&)> returnFunction;
    
    std::deque<bufferFrame<T1, T2>> frames;
};


#endif /* buffer_h */
