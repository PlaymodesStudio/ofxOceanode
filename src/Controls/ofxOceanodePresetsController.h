//
//  ofxOceanodePresetsController.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 12/03/2018.
//

#ifndef ofxOceanodePresetsController_h
#define ofxOceanodePresetsController_h

#include "ofxOceanodeBaseController.h"
#include "ofxDatGui.h"

class ofxOceanodeContainer;

class ofxOceanodePresetsController: public ofxOceanodeBaseController{
public:
    ofxOceanodePresetsController(shared_ptr<ofxOceanodeContainer> _container);
    ~ofxOceanodePresetsController(){};
    
    void onGuiDropdownEvent(ofxDatGuiDropdownEvent e);
    void onGuiScrollViewEvent(ofxDatGuiScrollViewEvent e);
    void onGuiTextInputEvent(ofxDatGuiTextInputEvent e);

    void activate();
    void deactivate();
    
    void windowResized(ofResizeEventArgs &a);
private:
    void changePresetLabelHighliht(ofxDatGuiButton *presetToHighlight);
    void loadBank();
    
    void loadPreset(string name, string bank);
    void savePreset(string name, string bank);
    
    ofxDatGui* gui;
    ofxDatGuiTheme* mainGuiTheme;
    ofxDatGuiDropdown* bankSelect;
    ofxDatGuiScrollView* presetsList;
    ofxDatGuiButton*    oldPresetButton;
    
    shared_ptr<ofxOceanodeContainer> container;
};

#endif /* ofxOceanodePresetsController_h */
