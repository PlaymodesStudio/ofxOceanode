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

class ofxOceanodePresetsController: public ofxOceanodeBaseController{
public:
    ofxOceanodePresetsController();
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
    
    ofxDatGui* gui;
    ofxDatGuiTheme* mainGuiTheme;
    ofxDatGuiDropdown* bankSelect;
    ofxDatGuiScrollView* presetsList;
    ofxDatGuiButton*    oldPresetButton;
};

#endif /* ofxOceanodePresetsController_h */
