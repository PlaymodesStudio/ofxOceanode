// HeaderPro reads a Buffer by sample index, elapsed milliseconds, or normalized position.
#ifndef bufferHeaderPro_h
#define bufferHeaderPro_h

#include "buffer.h"
#include "ofxOceanodeNodeModel.h"
#include <algorithm>
#include <cfloat>
#include <climits>
#include <cmath>
#include <limits>

template <typename T1, typename T2 = T1>
class bufferHeaderPro : public ofxOceanodeNodeModel
{
public:
    bufferHeaderPro(string typelabel, T1 val, bool minmax = false)
    : ofxOceanodeNodeModel("HeaderPro " + typelabel){
        description = "Reads samples from a connected Buffer and sends them as a vector.\n\n"
                      "Buffer Input -> Connect the Output of a Buffer of the same type.\n"
                      "Copies -> Number of samples to read.\n"
                      "Offset Mode -> Frames: sample index back from newest (0 is newest; fractions round down). Ms: milliseconds back from the newest timestamp, using the nearest sample. Normalized: position across the stored samples (0 is newest, 1 is oldest).\n"
                      "Offset -> With one copy, the requested position in the selected mode. With multiple copies and one Offset value, reads at 0, 1, 2, and so on times that value. Supply one Offset per copy for individual positions.\n"
                      "Output -> Samples in Offset order.\n"
                      "Overflow -> True when a Frames or Normalized position exceeds the oldest sample, or an Ms delay exceeds the available timestamp history. An out-of-range read returns the oldest available sample. Zero Ms reads the newest sample without overflow.\n\n"
                      "Inspector: Calculate On Update -> On: refresh Output every update and when the Buffer records a sample. Off: refresh only when Copies, Offset, Offset Mode, or Buffer Input changes. New samples in the same Buffer do not trigger a refresh in this mode.";

        addParameter(bufferInput.set("Buffer Input", nullptr));
        addParameter(numCopies.set("Copies", 1, 1, INT_MAX));
        addParameterDropdown(offsetMode, "Offset Mode", Frames, {"Frames", "Ms", "Normalized"});
        addParameter(offset.set("Offset", {0.0f}, {0.0f}, {FLT_MAX}));
        addParameter(overflow.set("Overflow", false));

        if(minmax){
            addParameter(output.set("Output", std::vector<T1>(1, val),
                std::vector<T1>(1, std::numeric_limits<T1>::lowest()),
                std::vector<T1>(1, std::numeric_limits<T1>::max())),
                ofxOceanodeParameterFlags_DisplayMinimized);
        }else{
            addParameter(output.set("Output", std::vector<T1>(1, val)),
                ofxOceanodeParameterFlags_DisplayMinimized);
        }
        addInspectorParameter(calcOnUpdate.set("Calculate On Update", true));
    }

    void setup() override{
        listeners.push(numCopies.newListener([this](int &){ if(!calcOnUpdate) recalculate(); }));
        listeners.push(offsetMode.newListener([this](int &){ if(!calcOnUpdate) recalculate(); }));
        listeners.push(offset.newListener([this](vector<float> &){ if(!calcOnUpdate) recalculate(); }));
        listeners.push(bufferInput.newListener([this](buffer<T1, T2>* &source){
            subscribeToBuffer(source);
            if(!calcOnUpdate) recalculate();
        }));
        subscribeToBuffer(bufferInput.get());
        recalculate();
    }

    void update(ofEventArgs &) override{
        if(calcOnUpdate) recalculate();
    }

private:
    enum OffsetMode { Frames, Ms, Normalized };

    void subscribeToBuffer(buffer<T1, T2>* source){
        frameAddedListener.unsubscribe();
        if(source != nullptr){
            frameAddedListener = source->onFrameAdded().newListener([this](){
                if(calcOnUpdate) recalculate();
            });
        }
    }

    void recalculate(){
        if(isRecalculating) return;
        auto *source = bufferInput.get();
        if(source == nullptr || source->getSize() == 0) return;

        const size_t size = source->getSize();
        const auto &offsets = offset.get();
        if(offsets.empty()) return;

        isRecalculating = true;

        std::vector<T1> samples;
        samples.reserve(numCopies.get());
        bool hasOverflow = false;
        for(int i = 0; i < numCopies.get(); ++i){
            // One value is a step; a vector supplies an independent position per copy.
            const float position = offsets.size() == 1 && numCopies.get() > 1
                ? i * offsets.front()
                : offsets[std::min(static_cast<size_t>(i), offsets.size() - 1)];
            if(!(position >= 0.0f)) continue;

            if(offsetMode == Ms){
                bool delayOverflow = false;
                samples.push_back(source->getClosestFrameDelayMs(position, true, &delayOverflow).getObjectRef());
                if(delayOverflow) hasOverflow = true;
            }else{
                const double framePosition = offsetMode == Normalized
                    ? static_cast<double>(position) * (size - 1)
                    : static_cast<double>(position);
                const bool outside = offsetMode == Normalized
                    ? position > 1.0f
                    : framePosition >= size;
                const size_t index = outside ? size - 1 : static_cast<size_t>(std::floor(framePosition));
                samples.push_back(source->getFrame(static_cast<int>(index), true).getObjectRef());
                if(outside) hasOverflow = true;
            }
        }
        overflow = hasOverflow;
        output = samples;
        isRecalculating = false;
    }

    ofParameter<buffer<T1, T2>*> bufferInput;
    ofParameter<int> numCopies;
    ofParameter<int> offsetMode;
    ofParameter<vector<float>> offset;
    ofParameter<bool> overflow;
    ofParameter<std::vector<T1>> output;
    ofParameter<bool> calcOnUpdate;
    ofEventListener frameAddedListener;
    bool isRecalculating = false;
    ofEventListeners listeners;
};

#endif /* bufferHeaderPro_h */
