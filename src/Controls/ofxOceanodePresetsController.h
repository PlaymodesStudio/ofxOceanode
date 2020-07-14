//
//  ofxOceanodePresetsController.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 12/03/2018.
//

#ifndef ofxOceanodePresetsController_h
#define ofxOceanodePresetsController_h

#include "ofxOceanodeBaseController.h"

class ofxOceanodePresetsController: public ofxOceanodeBaseController{
public:
    ofxOceanodePresetsController(shared_ptr<ofxOceanodeContainer> _container);
    ~ofxOceanodePresetsController(){};
    
    void draw();
    void update();
    
    void loadPresetFromNumber(int num);
    
private:
    void createPreset(string name);
    
    void loadPreset(string name, string bank);
    void savePreset(string name, string bank);
    void deletePreset(string name, string bank);
    
    map<string, vector<string>> bankPresets;
    map<string, string> currentPreset;
    vector<string> banks;
    int currentBank;

    bool newPresetCreated;
    int loadPresetInNextUpdate;
    
    ofEventListener presetListener;
    ofEventListener presetNumListener;
    ofEventListener saveCurrentPresetListener;
    
    shared_ptr<ofxOceanodeContainer> container;
};

#endif /* ofxOceanodePresetsController_h */
