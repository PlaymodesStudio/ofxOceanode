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
    mapper();
    ~mapper(){};
    
    void recalculate();

private:
    
    ofEventListeners listeners;
    
    ofParameter<vector<float>>  input;
    ofParameter<float>  minInput;
    ofParameter<float>  maxInput;
    ofParameter<float>  minOutput;
    ofParameter<float>  maxOutput;
    ofParameter<vector<float>>  output;
};


#endif /* mapper_h */
