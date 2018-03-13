//
//  ofxOceanodeBPMController.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 13/03/2018.
//

#ifndef ofxOceanodeBPMController_h
#define ofxOceanodeBPMController_h

#include "ofxOceanodeBaseController.h"
#include "ofxDatGui.h"

class ofxOceanodeBPMController: public ofxOceanodeBaseController{
public:
    ofxOceanodeBPMController();
    ~ofxOceanodeBPMController(){};
    
    void onGuiDropdownEvent(ofxDatGuiDropdownEvent e);
    void onGuiScrollViewEvent(ofxDatGuiScrollViewEvent e);
    void onGuiTextInputEvent(ofxDatGuiTextInputEvent e);
    
    void activate();
    void deactivate();
    
    void windowResized(ofResizeEventArgs &a);
private:
    ofxDatGui* gui;
    ofxDatGuiTheme* mainGuiTheme;
    
    ofParameter<float> bpm;
};


#endif /* ofxOceanodeBPMController_h */
