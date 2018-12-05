//
//  phasorClass.h
//  DGTL_Generator
//
//  Created by Eduard Frigola on 28/07/16.
//
//

#ifndef switcher_h
#define switcher_h

#pragma once

#include "ofxOceanodeNodeModel.h"


class switcher : public ofxOceanodeNodeModel{
public:
    switcher();
    ~switcher(){};
    
    void changedSwitch(int &i);
    void changedInput();

private:
    
    ofEventListeners listeners;
    
    ofParameter<vector<float>>  input1;
    ofParameter<vector<float>>  input2;
    ofParameter<int>  switchSelector;
    ofParameter<vector<float>>  output;
};


#endif /* switcher_h */
