// HeaderPro reads a Buffer by sample index, elapsed milliseconds, or normalized position.
#ifndef bufferHeaderPro_h
#define bufferHeaderPro_h

#include "buffer.h"
#include "ofxOceanodeNodeModel.h"
#include <algorithm>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdint>
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
                      "Overflow -> True when a Frames or Normalized position exceeds the oldest sample, or a requested Ms delay exceeds the available timestamp history, including when Fit Available History compresses the reads. Without fitting, an out-of-range read returns the oldest available sample. Zero Ms reads the newest sample without overflow.\n\n"
                      "Inspector: Fit Available History -> Off by default. In Ms mode, proportionally compress all requested delays when the largest exceeds the recorded time span. Their relative spacing is preserved as history grows; original delays are used once enough history exists. An empty or disconnected Buffer produces an empty Output and Overflow is true. With one sample, all copies read that sample. Frames and Normalized modes are unchanged.\n"
                      "Inspector: Calculate On Update -> On: refresh Output every update and when the Buffer records a sample. Off: refresh only when Copies, Offset, Offset Mode, Fit Available History, or Buffer Input changes. New samples in the same Buffer do not trigger a refresh in this mode.";

        addParameter(bufferInput.set("Buffer Input", nullptr));
        addParameter(numCopies.set("Copies", 1, 1, INT_MAX));
        addParameterDropdown(offsetMode, "Offset Mode", Frames, {"Frames", "Ms", "Normalized"});
        addParameter(offset.set("Offset", {0.0f}, {0.0f}, {FLT_MAX}));
        addOutputParameter(overflow.set("Overflow", false));

        if(minmax){
            addOutputParameter(output.set("Output", std::vector<T1>(1, val),
                std::vector<T1>(1, std::numeric_limits<T1>::lowest()),
                std::vector<T1>(1, std::numeric_limits<T1>::max())),
                ofxOceanodeParameterFlags_DisplayMinimized);
        }else{
            addOutputParameter(output.set("Output", std::vector<T1>(1, val)),
                ofxOceanodeParameterFlags_DisplayMinimized);
        }
        addInspectorParameter(calcOnUpdate.set("Calculate On Update", true));
        addInspectorParameter(fitAvailableHistory.set("Fit Available History", false));
    }

    void setup() override{
        listeners.push(numCopies.newListener([this](int &){ parametersChanged(); }));
        listeners.push(offsetMode.newListener([this](int &){ parametersChanged(); }));
        listeners.push(offset.newListener([this](vector<float> &){ parametersChanged(); }));
        listeners.push(fitAvailableHistory.newListener([this](bool &){ parametersChanged(); }));
        listeners.push(bufferInput.newListener([this](buffer<T1, T2>* &source){
            subscribeToBuffer(source);
            parametersChanged();
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

    void parametersChanged(){
        dirty = true;
        if(!calcOnUpdate) recalculate();
    }

    // Publishes the samples for the current Buffer and parameters. The reads
    // are recomputed only when a parameter or the Buffer history changed, so
    // the per-update refresh and the Buffer's frame notification in the same
    // tick cost one computation; the Output is still sent each time.
    void recalculate(){
        if(isRecalculating) return;
        auto *source = bufferInput.get();
        const uint64_t revision = source != nullptr ? source->revision() : 0;
        if(dirty || source != cachedSource || revision != cachedRevision){
            computeSamples(source);
            dirty = false;
            cachedSource = source;
            cachedRevision = revision;
        }
        if(!hasResult) return;
        isRecalculating = true;
        overflow = hasOverflow;
        output = samples;
        isRecalculating = false;
    }

    void computeSamples(buffer<T1, T2> *source){
        hasResult = false;
        const bool fitMs = fitAvailableHistory && offsetMode == Ms;
        if(source == nullptr || source->getSize() == 0){
            if(fitMs){
                samples.clear();
                hasOverflow = true;
                hasResult = true;
            }
            return;
        }

        const size_t size = source->getSize();
        const auto &offsets = offset.get();
        if(offsets.empty()) return;

        // A single value is spacing; otherwise each copy has its own delay.
        const auto positionForCopy = [&](int i){
            return offsets.size() == 1 && numCopies.get() > 1
                ? i * offsets.front()
                : offsets[std::min(static_cast<size_t>(i), offsets.size() - 1)];
        };
        double delayScale = 1.0;
        float availableMs = 0.0f;
        bool requestedOverflow = false;
        if(fitMs){
            availableMs = source->getAvailableHistoryMs();
            float maximumDelay = 0.0f;
            for(int i = 0; i < numCopies.get(); ++i){
                const float position = positionForCopy(i);
                if(std::isfinite(position)) maximumDelay = std::max(maximumDelay, position);
            }
            requestedOverflow = maximumDelay > availableMs;
            if(requestedOverflow) delayScale = static_cast<double>(availableMs) / maximumDelay;
        }

        samples.clear();
        samples.reserve(numCopies.get());
        hasOverflow = requestedOverflow;
        for(int i = 0; i < numCopies.get(); ++i){
            const float position = positionForCopy(i);
            if(!(position >= 0.0f)) continue;

            if(offsetMode == Ms){
                bool delayOverflow = false;
                const float delay = fitMs && std::isfinite(position)
                    ? std::min(availableMs, static_cast<float>(position * delayScale))
                    : position;
                samples.push_back(source->getClosestFrameDelayMs(delay, true, &delayOverflow).getObjectRef());
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
        hasResult = true;
    }

    ofParameter<buffer<T1, T2>*> bufferInput;
    ofParameter<int> numCopies;
    ofParameter<int> offsetMode;
    ofParameter<vector<float>> offset;
    ofParameter<bool> overflow;
    ofParameter<std::vector<T1>> output;
    ofParameter<bool> calcOnUpdate;
    ofParameter<bool> fitAvailableHistory;
    ofEventListener frameAddedListener;
    bool isRecalculating = false;
    bool dirty = true;
    buffer<T1, T2> *cachedSource = nullptr;
    uint64_t cachedRevision = 0;
    std::vector<T1> samples;
    bool hasOverflow = false;
    bool hasResult = false;
    ofEventListeners listeners;
};

#endif /* bufferHeaderPro_h */
