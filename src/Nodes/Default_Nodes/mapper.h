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
    void setup(int index = 0);
    float getRange();    
    void resetRange();
    
    void recalculate(float& f);

    std::unique_ptr<ofxOceanodeNodeModel> clone() const override {return make_unique<mapper>();};
private:
    
    ofParameter<float>  Input;
    ofParameter<float>  MinInput;
    ofParameter<float>  MaxInput;
    ofParameter<float>  MinOutput;
    ofParameter<float>  MaxOutput;
    ofParameter<float>  Output;
};


#endif /* mapper_h */
