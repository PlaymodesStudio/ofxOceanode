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
    
//    void onGuiDropdownEvent(ofxDatGuiDropdownEvent e);
//    void onGuiScrollViewEvent(ofxDatGuiScrollViewEvent e);
//    void onGuiTextInputEvent(ofxDatGuiTextInputEvent e);
    
//    void windowResized(ofResizeEventArgs &a);
private:
//    void changePresetLabelHighliht(ofxDatGuiButton *presetToHighlight);
    void loadBank();
    
    void loadPreset(string name, string bank);
    void savePreset(string name, string bank);
    
//    ofxDatGuiDropdown* bankSelect;
//    ofxDatGuiScrollView* presetsList;
//    ofxDatGuiButton*    oldPresetButton;
    
    map<string, vector<pair<int, string>>> bankPresets;
    map<string, pair<int, string>> currentPreset;
    
    vector<string> banks;
    int currentBank;

    
    int loadPresetInNextUpdate;
    
    ofEventListener presetListener;
    ofEventListener saveCurrentPresetListener;
};

#endif /* ofxOceanodePresetsController_h */
