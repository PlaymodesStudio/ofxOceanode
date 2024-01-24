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
        myBuffer = nullptr;
        addParameter(bufferInput.set("Buffer Input", nullptr));
        addParameter(numCopies.set("Copies", 1, 1, INT_MAX));
        addParameter(offset.set("Offset", {1000/60}, {0.0}, {FLT_MAX}));
        
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
        listeners.push(numCopies.newListener([this](int &i){
            if(!calcOnUpdate) recalculate();
        }));
        listeners.push(offset.newListener([this](vector<float> &vf){
            if(!calcOnUpdate) recalculate();
        }));
        listeners.push(bufferInput.newListener([this](buffer<T1, T2>* &_buffer){
            if(!calcOnUpdate) recalculate();
        }));

        recalculate();
    }
    
    void update(ofEventArgs &a) override{
        if(calcOnUpdate){
            recalculate();
        }
    }
    
    void recalculate(){
        if(bufferInput.get() != nullptr && bufferInput.get()->getSize() > 0){
            if(offset->size() != numCopies){
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
            
            for(int i = numCopies-1; i >=0; i--)
            {
                if(multixDelaysInMs[numCopies-1-i] >= 0)
                {
                    if(offsetAsPosition){
                        temp.push_back(bufferInput.get()->getFrame((int)floor(multixDelaysInMs[numCopies-1-i]), true).getObjectRef());
                    }else{
                        temp.push_back(bufferInput.get()->getClosestFrameDelayMs(multixDelaysInMs[numCopies-1-i], true).getObjectRef());
                    }
                }
            }
            output = temp;
        }
    }
    
private:

    vector<float> multixDelaysInMs;

    ofParameter<buffer<T1, T2>*> bufferInput;
    ofParameter<int> numCopies;
    ofParameter<vector<float>> offset;
    ofParameter<std::vector<T1>> output;
    
    ofParameter<bool> calcOnUpdate;
    ofParameter<bool> offsetAsPosition;

    buffer<T1, T2>* myBuffer;

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
