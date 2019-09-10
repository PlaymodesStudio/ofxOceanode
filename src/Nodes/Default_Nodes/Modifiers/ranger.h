//
//  phasorClass.h
//  DGTL_Generator
//
//  Created by Eduard Frigola on 28/07/16.
//
//

#ifndef ranger_h
#define ranger_h

#pragma once

#include "ofxOceanodeNodeModel.h"


class ranger : public ofxOceanodeNodeModel{
public:
    ranger() : ofxOceanodeNodeModel("Ranger"){};
    ~ranger(){};
    void setup() override;
//    float getRange();    
//    void resetRange();
    
    void recalculate();

private:
    
    ofEventListeners listeners;
    
    ofParameter<vector<float>>  input;
    ofParameter<vector<float>>  minInput;
    ofParameter<vector<float>>  maxInput;
    ofParameter<vector<float>>  minOutput;
    ofParameter<vector<float>>  maxOutput;
    ofParameter<vector<float>>  output;
};


#endif /* ranger_h */
