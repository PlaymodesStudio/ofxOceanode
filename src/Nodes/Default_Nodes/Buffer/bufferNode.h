//
//  bufferNode.h
//  ofxOceanode
//
//  Created by Eduard Frigola on 20/12/23.
//

#ifndef bufferNode_h
#define bufferNode_h

#include "buffer.h"
#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
#include "phasor.h"
#include <cmath>
#include <cstdint>

// Derives, from the frame-latched transport state, the transport time of a
// sample and the span it replaces in a transport-indexed Buffer. Continuous
// playback replaces what was stored since the previous sample; after a seek,
// stop or loop wrap only the frame at the new position is replaced.
struct bufferTransportClock {
    static int64_t toUs(double seconds){
        return static_cast<int64_t>(std::llround(seconds * 1000000.0));
    }

    void reset(){ hasRecorded = false; }

    // Once per frame, before recording: tracks the transport advance per frame.
    void observe(const ofxOceanodeFrameTransportState &state){
        const bool continuous = state.current.generation == state.previous.generation
            && state.current.loopCount == state.previous.loopCount;
        const int64_t delta = toUs(state.current.seconds) - toUs(state.previous.seconds);
        if(continuous && delta > 0) frameUs = delta;
    }

    int64_t getFrameUs() const { return frameUs; }

    // True while no sample has been recorded at this transport position.
    bool needsSample(const ofxOceanodeFrameTransportState &state) const {
        return !hasRecorded || toUs(state.current.seconds) != lastStampUs
            || state.current.generation != generation || state.current.loopCount != loopCount;
    }

    struct Sample {
        Timestamp stamp;
        Timestamp replaceFromExclusive;
    };
    Sample next(const ofxOceanodeFrameTransportState &state){
        const int64_t stampUs = toUs(state.current.seconds);
        const bool continuous = hasRecorded && state.current.generation == generation
            && state.current.loopCount == loopCount && stampUs >= lastStampUs;
        Sample sample{Timestamp(stampUs), Timestamp(continuous ? lastStampUs : stampUs - frameUs)};
        hasRecorded = true;
        lastStampUs = stampUs;
        generation = state.current.generation;
        loopCount = state.current.loopCount;
        return sample;
    }

private:
    bool hasRecorded = false;
    int64_t lastStampUs = 0;
    uint64_t generation = 0;
    uint64_t loopCount = 0;
    int64_t frameUs = 16667;
};

static const char *bufferSyncToTransportHelp =
    "Inspector: Sync To Transport -> Off: samples are stamped with the wall clock, or the Timestamp input, in recording order. "
    "On: the history is indexed by transport time. A sample is recorded every frame while the transport plays and whenever Input changes; the Timestamp input is ignored. "
    "Recording replaces what was stored over the time just played, so loops and scrubbing refresh samples in place and never grow the buffer. "
    "Header reads are anchored at the playhead: after a seek into material played before they return the samples recorded for that time, samples ahead of the playhead are ignored, "
    "and a read with no sample within two frames of its time sets Overflow. A full buffer discards samples ahead of the playhead first, then the oldest. Switching this option empties the history.";

template <typename T1, typename T2 = T1>
class bufferNode : public timeGenerator{
public:
    bufferNode(string typelabel, T1 val, bool minmax = false,
               std::function<void(T1&, T2&)> _assignFunction = [](T1 &data, T2 &container){container = data;},
               std::function<T1(T2&)> _returnFunction = [](T2 &container)->T1{return container;},
               std::function<bool(T1&)> _checkFunction = [](T1 &data)->bool{return true;}
               ) :  timeGenerator("Buffer " + typelabel), assignFunction(_assignFunction), returnFunction(_returnFunction), checkFunction(_checkFunction){
        description = "Stores a bounded history of Input values for a Header or HeaderPro of the same type. A new sample is recorded when Input changes while Rec is on. Input changes during preset loading are not recorded, so temporary initialization values do not enter the history.\n\n"
                      "Rec -> Enables recording.\n"
                      "Timestamp -> If connected, supplies each sample's timestamp; otherwise the current wall clock time is used.\n"
                      "Max Size -> Maximum number of stored samples. Older samples are discarded as new ones arrive.\n"
                      "Clear -> Empties the history.\n"
                      "Output -> The Buffer connection for a Header or HeaderPro; it is not the latest sample value.\n"
                      "Inspector: Last Frame Ms -> Available time span in milliseconds backward from the newest sample, updated when Input changes.\n"
                      + string(bufferSyncToTransportHelp);
        myBuffer = std::make_unique<buffer<T1, T2>>(assignFunction, returnFunction);
    }
    ~bufferNode(){}
    
    void setup() override{
        addParameter(input.set("Input", T1()));
        addParameter(rec.set("Rec", true));
		addParameter(clearBuffer.set("Clear"));

        addParameter(timestamp.set("Timestamp", Timestamp()));
        addParameter(maxSize.set("Max Size", 100, 1, INT_MAX));
        addOutputParameter(output.set("Output", myBuffer.get()));
        
        lastFrameMs.set("Last Frame Ms",0,0,FLT_MAX);
        lastFrameMs.setSerializable(false); // Runtime history measurement, not preset configuration.
        addInspectorParameter(lastFrameMs);
        addInspectorParameter(syncToTransport.set("Sync To Transport", false));
        
        listeners.push(input.newListener([this](T1 &object){
            // Restoring parameters and reactivating connections can send an
            // upstream node's default value before it has computed live output.
            if(!ofxOceanodeShared::isPresetLoading() && checkFunction(object) && rec){
                if(syncToTransport){
                    recordTransportSample(input.get());
                }else if(getOceanodeParameter(timestamp).hasInConnection()){
                    myBuffer->addFrame(input.get(), timestamp);
                }else{
                    myBuffer->addFrame(input.get());
                }
            }
            lastFrameMs = myBuffer->getAvailableHistoryMs();
            
        }));

        listeners.push(syncToTransport.newListener([this](bool &enabled){
            if(enabled == myBuffer->isTransportIndexed()) return;
            myBuffer->setTransportIndexed(enabled);
            transportClock.reset();
            lastFrameMs = myBuffer->getAvailableHistoryMs();
        }));

		listeners.push(clearBuffer.newListener([this](){
			myBuffer->clear();
			lastFrameMs = 0.0f;
		}));

        sizeListener = maxSize.newListener([this](int &_maxSize){
            myBuffer->setMaxSize(_maxSize);
            lastFrameMs = myBuffer->getAvailableHistoryMs();
        });
        
        myBuffer->setMaxSize(maxSize);
    }

    // Transport sync: follow the playhead and record on the frame grid while playing.
    void update(ofEventArgs &) override{
        if(!syncToTransport) return;
        const auto state = getFrameTransportState();
        transportClock.observe(state);
        myBuffer->setGapToleranceMs(static_cast<float>(transportClock.getFrameUs()) * 2.0f / 1000.0f);
        myBuffer->setPlayhead(Timestamp(bufferTransportClock::toUs(state.current.seconds)));
        if(state.current.isPlaying && rec && !ofxOceanodeShared::isPresetLoading()
           && transportClock.needsSample(state)){
            recordTransportSample(input.get());
        }
        lastFrameMs = myBuffer->getAvailableHistoryMs();
    }
    
private:
    void recordTransportSample(T1 value){
        if(!checkFunction(value)) return;
        const auto sample = transportClock.next(getFrameTransportState());
        myBuffer->addFrame(std::move(value), sample.stamp, sample.replaceFromExclusive);
    }

    std::function<void(T1&, T2&)> assignFunction;
    std::function<T1(T2&)> returnFunction;
    std::function<bool(T1&)> checkFunction;
    
    ofEventListeners listeners;
    ofEventListener sizeListener;
    
    ofParameter<T1> input;
    ofParameter<bool> rec;
    ofParameter<Timestamp> timestamp;
	ofParameter<void> clearBuffer;

    
    ofParameter<int> maxSize;
    
    ofParameter<buffer<T1, T2>*> output;
    
    ofParameter<float> lastFrameMs;
    ofParameter<bool> syncToTransport;
    bufferTransportClock transportClock;
    
    std::unique_ptr<buffer<T1, T2>> myBuffer;
};

template<typename T>
class bufferNode<std::vector<T>> : public ofxOceanodeNodeModel{
public:
    bufferNode(string typelabel, T val, bool minmax = false,
               std::function<void(std::vector<T>&, std::vector<T>&)> _assignFunction = [](std::vector<T> &data, std::vector<T> &container){container = data;},
               std::function<std::vector<T>(std::vector<T>&)> _returnFunction = [](std::vector<T> &container)->std::vector<T>{return container;},
               std::function<bool(std::vector<T>&)> _checkFunction = [](std::vector<T> &data)->bool{return true;}
               ) :  ofxOceanodeNodeModel("Buffer " + typelabel), assignFunction(_assignFunction), returnFunction(_returnFunction), checkFunction(_checkFunction){
                   description = "Stores a bounded history of Input vectors for a Header or HeaderPro of the same type. A new vector is recorded when Input changes while Rec is on. Input changes during preset loading are not recorded, so temporary initialization values do not enter the history.\n\n"
                                 "Rec -> Enables recording.\n"
                                 "Timestamp -> If connected, supplies each vector's timestamp; otherwise the current wall clock time is used.\n"
                                 "Max Size -> Maximum number of stored vectors. Older vectors are discarded as new ones arrive.\n"
                                 "Output -> The Buffer connection for a Header or HeaderPro; it is not the latest vector value.\n"
                                 + string(bufferSyncToTransportHelp);
                   myBuffer = std::make_unique<buffer<std::vector<T>, std::vector<T>>>(assignFunction, returnFunction);
                   if(minmax){
                       addParameter(input.set("Input", std::vector<T>(1, val),
                                              std::vector<T>(1, std::numeric_limits<T>::lowest()),
                                              std::vector<T>(1, std::numeric_limits<T>::max())));
                   }else{
                       addParameter(input.set("Input", std::vector<T>(1, val)));
                   }
               }
    ~bufferNode(){}
    
    void setup() override{
        addParameter(rec.set("Rec", true));
        addParameter(timestamp.set("Timestamp", Timestamp()));
        addParameter(maxSize.set("Max Size", 100, 1, INT_MAX));
        addOutputParameter(output.set("Output", myBuffer.get()));
        addInspectorParameter(syncToTransport.set("Sync To Transport", false));
        
        listeners.push(input.newListener([this](std::vector<T> &object){
            if(!ofxOceanodeShared::isPresetLoading() && checkFunction(object) && rec){
                if(syncToTransport){
                    recordTransportSample(input.get());
                }else if(getOceanodeParameter(timestamp).hasInConnection()){
                    myBuffer->addFrame(input.get(), timestamp);
                }else{
                    myBuffer->addFrame(input.get());
                }
            }
        }));

        listeners.push(syncToTransport.newListener([this](bool &enabled){
            if(enabled == myBuffer->isTransportIndexed()) return;
            myBuffer->setTransportIndexed(enabled);
            transportClock.reset();
        }));
        
		
        sizeListener = maxSize.newListener([this](int &_maxSize){
            myBuffer->setMaxSize(_maxSize);
        });
        
        myBuffer->setMaxSize(maxSize);
    }

    // Transport sync: follow the playhead and record on the frame grid while playing.
    void update(ofEventArgs &) override{
        if(!syncToTransport) return;
        const auto state = getFrameTransportState();
        transportClock.observe(state);
        myBuffer->setGapToleranceMs(static_cast<float>(transportClock.getFrameUs()) * 2.0f / 1000.0f);
        myBuffer->setPlayhead(Timestamp(bufferTransportClock::toUs(state.current.seconds)));
        if(state.current.isPlaying && rec && !ofxOceanodeShared::isPresetLoading()
           && transportClock.needsSample(state)){
            recordTransportSample(input.get());
        }
    }
    
	void clear()
	{
		myBuffer->clear();
	}
private:
    void recordTransportSample(std::vector<T> value){
        if(!checkFunction(value)) return;
        const auto sample = transportClock.next(getFrameTransportState());
        myBuffer->addFrame(std::move(value), sample.stamp, sample.replaceFromExclusive);
    }

    std::function<void(std::vector<T>&, std::vector<T>&)> assignFunction;
    std::function<std::vector<T>(std::vector<T>&)> returnFunction;
    std::function<bool(std::vector<T>&)> checkFunction;
    
    ofEventListeners listeners;
    ofEventListener sizeListener;
    
    ofParameter<std::vector<T>> input;
    ofParameter<bool> rec;
    ofParameter<Timestamp> timestamp;
    
    ofParameter<int> maxSize;
    
    ofParameter<buffer<std::vector<T>, std::vector<T>>*> output;
    ofParameter<bool> syncToTransport;
    bufferTransportClock transportClock;
    
    std::unique_ptr<buffer<std::vector<T>, std::vector<T>>> myBuffer;
};

#endif /* bufferNode_h */
