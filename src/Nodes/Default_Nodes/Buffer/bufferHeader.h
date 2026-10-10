//
//  bufferHeader.h
//  ofxOceanode
//
//  Created by Eduard Frigola on 21/12/23.
//

#ifndef bufferHeader_h
#define bufferHeader_h

#include "buffer.h"
#include "ofxOceanodeNodeModel.h"

template <typename T1, typename T2 = T1>
class bufferHeader: public ofxOceanodeNodeModel
{
public:
    bufferHeader(string typelabel, T1 val, bool minmax = false) : ofxOceanodeNodeModel("Header " + typelabel){
        description = "Reads samples from a connected Buffer and sends them as a vector.\n\n"
                      "Buffer Input -> Connect the Output of a Buffer of the same type.\n"
                      "Copies -> Number of samples to read.\n"
                      "Offset -> With one copy, the distance from the newest sample. With multiple copies and one Offset value, the spacing between samples: the reads start at 0, then 1, 2, and so on times that value. Supply exactly one Offset per copy to choose each distance separately.\n"
                      "Output -> Samples in Offset order.\n"
                      "Overflow -> True when a millisecond delay exceeds the available timestamp history, or a sample index exceeds the oldest sample. An out-of-range read returns the oldest available sample. Zero milliseconds reads the newest sample without overflow.\n\n"
                      "Inspector: Offset as Position -> Off: Offset is a delay in milliseconds from the newest stored timestamp; the nearest stored sample is used. On: Offset is a sample index back from the newest sample, with 0 being the newest and fractional positions rounded down.\n"
                      "Inspector: Calculate On Update -> On: refresh Output every update and when the Buffer records a sample. Off: refresh only when Copies, Offset, or Buffer Input changes. New samples in the same Buffer do not trigger a refresh in this mode.";
        myBuffer = nullptr;
        addParameter(bufferInput.set("Buffer Input", nullptr));
        addParameter(numCopies.set("Copies", 1, 1, INT_MAX));
        addParameter(offset.set("Offset", {1000.0/60.0}, {0.0}, {FLT_MAX}));
        addParameter(overflow.set("Overflow",false));
        
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
        addInspectorParameter(offsetAsPosition.set("Offset as Position", false));
    }
    
    ~bufferHeader(){}

    void setup() override{
        listeners.push(numCopies.newListener([this](int &){
            if(!calcOnUpdate) recalculate();
        }));
        listeners.push(offset.newListener([this](vector<float> &){
            if(!calcOnUpdate) recalculate();
        }));
        listeners.push(offsetAsPosition.newListener([this](bool &){
            if(!calcOnUpdate) recalculate();
        }));
        listeners.push(bufferInput.newListener([this](buffer<T1, T2>* &_buffer){
            subscribeToBuffer(_buffer);
            if(!calcOnUpdate) recalculate();
        }));

        subscribeToBuffer(bufferInput.get());
        recalculate();
    }
    
    void update(ofEventArgs &) override{
        if(calcOnUpdate){
            recalculate();
        }
    }
    
    void recalculate(){
        if(isRecalculating) return;
        if(bufferInput.get() != nullptr && bufferInput.get()->getSize() > 0){
            isRecalculating = true;
            if((offset->size() != static_cast<size_t>(numCopies.get()))&&(numCopies>0)){
                vector<float> vf(numCopies);
                for(int i = 0; i < numCopies; i++)
                {
                    if(offset->size() == 1){
                        vf[i] = (i * offset->at(0));
                    }
                }
                multixDelaysInMs = vf;
            }else{
                multixDelaysInMs = offset.get();
            }
            
            std::vector<T1> temp;
            
            overflow=false;
            for(int i = numCopies-1; i >=0; i--)
            {
                if(multixDelaysInMs[numCopies-1-i] >= 0)
                {
                    if(offsetAsPosition)
                    {
                        const float position = multixDelaysInMs[numCopies-1-i];
                        const bool outside = position >= bufferInput.get()->getSize();
                        const int frameToGet = outside
                            ? static_cast<int>(bufferInput.get()->getSize() - 1)
                            : static_cast<int>(floor(position));
                        temp.push_back(bufferInput.get()->getFrame(frameToGet, true).getObjectRef());
                        // check for buffer overflow
                        if(outside) overflow=true;
                    }
                    else
                    {
                        float frameDelayToGet = multixDelaysInMs[numCopies-1-i];
                        bool delayOverflow = false;
                        temp.push_back(bufferInput.get()->getClosestFrameDelayMs(frameDelayToGet, true, &delayOverflow).getObjectRef());
                        if(delayOverflow) overflow = true;
                            
                    }
                }
            }
            output = temp;
            isRecalculating = false;
        }
    }
    
private:

    void subscribeToBuffer(buffer<T1, T2>* source){
        frameAddedListener.unsubscribe();
        if(source != nullptr){
            frameAddedListener = source->onFrameAdded().newListener([this](){
                if(calcOnUpdate) recalculate();
            });
        }
    }

    vector<float> multixDelaysInMs;

    ofParameter<buffer<T1, T2>*> bufferInput;
    ofParameter<int> numCopies;
    ofParameter<vector<float>> offset;
    ofParameter<bool> overflow;
    ofParameter<std::vector<T1>> output;
    
    ofParameter<bool> calcOnUpdate;
    ofParameter<bool> offsetAsPosition;

    buffer<T1, T2>* myBuffer;
    ofEventListener frameAddedListener;
    bool isRecalculating = false;

    template <typename T>
    T getValueForPosition(const vector<T> &param, int index){
        if(param.size() == 1 || param.size() <= index){
            return param[0];
        }
        else{
            return param[index];
        }
    }

    ofEventListeners listeners;
};

#endif /* bufferHeader_h */
