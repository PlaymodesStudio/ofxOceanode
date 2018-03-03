//
//  ofxOceanodeNodeGui.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 22/02/2018.
//

#ifndef ofxOceanodeNodeGui_h
#define ofxOceanodeNodeGui_h

#include "ofxDatGui.h"

class ofxOceanodeContainer;
class ofxOceanodeNode;

class ofxOceanodeNodeGui{
public:
    ofxOceanodeNodeGui(ofxOceanodeContainer &container, ofxOceanodeNode &node);
    ~ofxOceanodeNodeGui();
    
    void createGuiFromParameters();
    void updateGuiForParameter(string &parameterName);
    
    void setPosition(glm::vec2 position);
    
    ofParameterGroup* getParameters();
    
    glm::vec2 getSourceConnectionPositionFromParameter(ofAbstractParameter& parameter);
    glm::vec2 getSinkConnectionPositionFromParameter(ofAbstractParameter& parameter);
    
    void mouseMoved(ofMouseEventArgs &args){};
    void mouseDragged(ofMouseEventArgs &args);
    void mousePressed(ofMouseEventArgs &args){};
    void mouseReleased(ofMouseEventArgs &args);
    void mouseScrolled(ofMouseEventArgs &args){};
    void mouseEntered(ofMouseEventArgs &args){};
    void mouseExited(ofMouseEventArgs &args){};
    
private:
    void onGuiButtonEvent(ofxDatGuiButtonEvent e);
    void onGuiToggleEvent(ofxDatGuiToggleEvent e);
    void onGuiDropdownEvent(ofxDatGuiDropdownEvent e);
    void onGuiMatrixEvent(ofxDatGuiMatrixEvent e);
    void onGuiSliderEvent(ofxDatGuiSliderEvent e);
    void onGuiTextInputEvent(ofxDatGuiTextInputEvent e);
    void onGuiColorPickerEvent(ofxDatGuiColorPickerEvent e);
    void onGuiRightClickEvent(ofxDatGuiRightClickEvent e);
    void onGuiScrollViewEvent(ofxDatGuiScrollViewEvent e);
    
    void newModuleListener(ofxDatGuiDropdownEvent e);
    void newPresetListener(ofxDatGuiTextInputEvent e);
    
    vector<ofEventListener> parameterChangedListeners;
    
    ofxOceanodeContainer& container;
    
    ofxOceanodeNode& node;
    
    unique_ptr<ofxDatGui> gui;
    ofColor color;
    glm::vec2 position;
    
    bool isDraggingGui;
};

#endif /* ofxOceanodeNodeGui_h */
