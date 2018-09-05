//
//  ofxOceanodeRandomSeedController.h
//  example-seedRandom
//
//  Created by Eduard Frigola on 04/09/2018.
//

#ifndef ofxOceanodeRandomSeedController_h
#define ofxOceanodeRandomSeedController_h

#ifdef OFXOCEANODE_USE_RANDOMSEED

#include "ofxOceanodeBaseController.h"

class ofxOceanodeRandomSeedController : public ofxOceanodeBaseController{
public:
    ofxOceanodeRandomSeedController(shared_ptr<ofxOceanodeContainer> _container);
    ~ofxOceanodeRandomSeedController(){};
    
private:
    ofParameter<int> randomSeedNum;
    ofEventListener listener;
    ofxDatGuiTextInput* display;
};

#endif

#endif /* ofxOceanodeRandomSeedController_h */
