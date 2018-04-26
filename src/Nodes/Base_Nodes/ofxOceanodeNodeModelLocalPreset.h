//
//  ofxOceanodeLocalPreset.h
//  VJYourself_OCEAN
//
//  Created by Eduard Frigola on 25/04/2018.
//

#ifndef ofxOceanodeLocalPreset_h
#define ofxOceanodeLocalPreset_h

#include "ofxOceanodeNodeModel.h"

class ofxOceanodeNodeModelLocalPreset : public ofxOceanodeNodeModel{
public:
    ofxOceanodeNodeModelLocalPreset(string name);
    ofxOceanodeNodeModelLocalPreset(){};

    
private:
    void presetListener(short int &preset);
    
    ofParameter<short int> presetControl;
    ofEventListener listener;
};

#endif /* ofxOceanodeLocalPreset_h */
