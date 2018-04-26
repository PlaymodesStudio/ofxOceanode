//
//  ofxOceanodeNodeModelLocalPreset.h
//  VJYourself_OCEAN
//
//  Created by Eduard Frigola on 25/04/2018.
//

#ifndef ofxOceanodeNodeModelLocalPreset_h
#define ofxOceanodeNodeModelLocalPreset_h

#include "ofxOceanodeNodeModel.h"

class ofxOceanodeNodeModelLocalPreset : public ofxOceanodeNodeModel{
public:
    ofxOceanodeNodeModelLocalPreset(string name);
    ~ofxOceanodeNodeModelLocalPreset(){};

    
private:
    void presetListener(short int &preset);
    
    ofParameter<short int> presetControl;
    ofEventListener listener;
};

#endif /* ofxOceanodeLocalPreset_h */
