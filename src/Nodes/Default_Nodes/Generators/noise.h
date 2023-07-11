//
//  noise.h
//  ofxOceanode
//
//  Created by Eduard Frigola Bagué on 16/08/2021.
//

#ifndef noise_h
#define noise_h

#include "ofxOceanodeNodeModel.h"

class noise : public ofxOceanodeNodeModel{
public:
    noise() : ofxOceanodeNodeModel("Noise"){};
    void setup(){
        color = ofColor(0, 200, 255);
        description = "Creates a Perlin noise based on a always changing input";
        addParameter(input.set("Input", {0}, {0}, {FLT_MAX}));
        addParameter(index.set("Index", {0}, {0}, {1}));
        addParameter(scale.set("Scale", 1, 0, FLT_MAX));
        addParameter(speed.set("Speed", {1}, {0}, {FLT_MAX}));
        addParameter(seed.set("Seed", {0}, {-FLT_MAX}, {FLT_MAX}));
        addOutputParameter(output.set("Output", {0}, {0}, {1}));
        
        addInspectorParameter(crossfadePercentage.set("Crossfade Pct", 0, 0, 1));
    }
	
    void update(ofEventArgs &a){
        int maxSize = max({index->size(), seed->size(), input->size()});
        vector<float> tempOut(maxSize, 0);
        //Start Values
        float inputAtStart = getValueForPosition(input.get(), 0);
        float speedAtStart = getValueForPosition(speed.get(), 0);
        float indexAtStart = getValueForPosition(index.get(), 0);
        float seedAtStart = getValueForPosition(seed.get(), 0);
        //End Values
        float inputAtEnd = getValueForPosition(input.get(), maxSize-1);
        float speedAtEnd = getValueForPosition(speed.get(), maxSize-1);
        float indexAtEnd = getValueForPosition(index.get(), maxSize-1);
        float seedAtEnd = getValueForPosition(seed.get(), maxSize-1);
        //
        int crossfadeSize = crossfadePercentage * maxSize;
        
        for(int i = 0; i < maxSize; i++){
            float inputAtIndex = getValueForPosition(input.get(), i);
            float speedAtIndex = getValueForPosition(speed.get(), i);
            float indexAtIndex = getValueForPosition(index.get(), i);
            float seedAtIndex = getValueForPosition(seed.get(), i);
            
            if(maxSize - i - 1 < crossfadeSize){
                float interpol = float(i - (maxSize - 1 - crossfadeSize)) / float(crossfadeSize);
                float inputAtIndexLooped = inputAtIndex - inputAtEnd + inputAtStart;
                float speedAtIndexLooped = speedAtIndex - speedAtEnd + speedAtStart;
                float indexAtIndexLooped = indexAtIndex - indexAtEnd + indexAtStart;
                float seedAtIndexLooped = seedAtIndex - seedAtEnd + seedAtStart;
                
                tempOut[i] = smoothinterpolate(ofNoise((inputAtIndex * speedAtIndex) + (indexAtIndex * scale), seedAtIndex * scale),
                                    ofNoise((inputAtIndexLooped * speedAtIndexLooped) + (indexAtIndexLooped * scale), seedAtIndexLooped * scale),
                                    interpol);
            }else{
                tempOut[i] = ofNoise((inputAtIndex * speedAtIndex) + (indexAtIndex * scale), seedAtIndex * scale);
            }
            
        }
        output = tempOut;
    }
    
private:
    template <typename T>
    auto getValueForPosition(const vector<T> &param, int index) -> T{
        if(param.size() == 1 || param.size() <= index){
            return param[0];
        }
        else{
            return param[index];
        }
    };
    
    float smoothinterpolate(float start, float end, float pos){
        float oldRandom = start;
        float pastRandom = start;
        float newRandom = end;
        float futureRandom  = end;
        float L0 = (newRandom - pastRandom) * 0.5;
        float L1 = L0 + (oldRandom-newRandom);
        float L2 = L1 + ((futureRandom - oldRandom)*0.5) + (oldRandom - newRandom);
        return oldRandom + (pos * (L0 + (pos * ((pos * L2) - (L1 + L2)))));
        
    }
    
    ofParameter<vector<float>> input;
    ofParameter<vector<float>>  index;
	ofParameter<float>	scale;
    ofParameter<vector<float>> speed;
    ofParameter<vector<float>> seed;
    ofParameter<vector<float>>  output;
    ofParameter<float> crossfadePercentage;
    ofParameter<int> crossfadeSize;
};

#endif /* noise_h */
