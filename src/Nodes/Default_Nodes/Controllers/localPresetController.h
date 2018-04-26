//
//  localPresetController.h
//  VJYourself_OCEAN
//
//  Created by Eduard Frigola on 25/04/2018.
//

#ifndef localPresetController_h
#define localPresetController_h

#include "ofxOceanodeNodeModel.h"

class localPresetController : public ofxOceanodeNodeModel{
public:
    localPresetController();
    ~localPresetController(){};
    
private:
    ofParameter<pair<int, bool>> matrix;
    ofParameter<bool> on;
    ofParameter<short int> preset;
    
    ofEventListener listener;
};

#endif /* localPresetController_h */
