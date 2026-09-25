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
    
    void draw() override;
    void drawPopups() override;
    void update() override;
    bool isMenuController() const override { return true; }
    
    void loadPresetFromNumber(int num);
    
private:
    void drawPresetList();
    void createPreset(string name);
    
    void loadPreset(string name, string bank);
    void savePreset(string name, string bank);
    void deletePreset(string name, string bank);
    
    map<string, vector<string>> bankPresets;
    map<string, string> currentPreset;
    vector<string> banks;
    int currentBank;

    enum class PopupRequest { None, NewBank, SaveAs, Delete };
    PopupRequest popupRequest = PopupRequest::None;
    char bankNameBuffer[256] = {};
    char presetNameBuffer[256] = {};
    int saveAsBank = 0;
    string deleteBankName;
    string deletePresetName;

    bool newPresetCreated;
    int loadPresetInNextUpdate;
    
    ofEventListener presetListener;
    ofEventListener presetNumListener;
    ofEventListener saveCurrentPresetListener;
    
    shared_ptr<ofxOceanodeContainer> container;
};

#endif /* ofxOceanodePresetsController_h */
