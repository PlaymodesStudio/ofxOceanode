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
    ranger();
    ~ranger(){};
    void setup(int index = 0);
//    float getRange();    
//    void resetRange();
    
    void recalculate();

private:
    
    vector<ofEventListener> listeners;
    
    ofParameter<vector<float>>  Input;
    ofParameter<float>  MinInput;
    ofParameter<float>  MaxInput;
    ofParameter<float>  MinOutput;
    ofParameter<float>  MaxOutput;
    ofParameter<vector<float>>  Output;
};


#endif /* ranger_h */
