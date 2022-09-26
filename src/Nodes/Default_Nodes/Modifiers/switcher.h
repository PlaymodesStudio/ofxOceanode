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
    switcher() : ofxOceanodeNodeModel("Switcher Float"){};
    ~switcher(){};
    void setup() override;
	
	void update(ofEventArgs &a) override;
    
    void loadBeforeConnections(ofJson &json) override{
        deserializeParameter(json, numInputs);
    }

private:
    
    ofEventListeners listeners;
	
	ofParameter<int> numInputs;
    
    ofParameter<bool> normalizedSelector;
    vector<ofParameter<vector<float>>>  inputs;
    ofParameter<vector<float>>  switchSelector;
    ofParameter<vector<float>>  output;
};


#endif /* switcher_h */
