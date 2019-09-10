//
//  phasorClass.h
//  DGTL_Generator
//
//  Created by Eduard Frigola on 28/07/16.
//
//

#ifndef mapper_h
#define mapper_h

#pragma once

#include "ofxOceanodeNodeModel.h"


class mapper : public ofxOceanodeNodeModel{
public:
    mapper() : ofxOceanodeNodeModel("Mapper"){};
    ~mapper(){};
    void setup() override;
    
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


#endif /* mapper_h */
