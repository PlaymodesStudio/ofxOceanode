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
        addParameter(input.set("Input", {0}, {0}, {FLT_MAX}));
        addParameter(index.set("Index", {0}, {0}, {1}));
        addParameter(scale.set("Scale", 1, 0, FLT_MAX));
        addParameter(seed.set("Seed", {0}, {-FLT_MAX}, {FLT_MAX}));
        addOutputParameter(output.set("Output", {0}, {0}, {1}));
    }
	
    void update(ofEventArgs &a){
        int maxSize = max({index->size(), seed->size(), input->size()});
        vector<float> tempOut(maxSize, 0);
        for(int i = 0; i < maxSize; i++){
            tempOut[i] = ofNoise(getValueForPosition(input.get(), i) + (getValueForPosition(index.get(), i)*scale), getValueForPosition(seed.get(), i)*scale);
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
    
    ofParameter<vector<float>> input;
    ofParameter<vector<float>>  index;
	ofParameter<float>	scale;
    ofParameter<vector<float>> seed;
    ofParameter<vector<float>>  output;
};

#endif /* noise_h */
