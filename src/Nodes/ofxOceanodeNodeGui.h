//
//  ofxOceanodeNodeGui.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 22/02/2018.
//

#ifndef ofxOceanodeNodeGui_h
#define ofxOceanodeNodeGui_h

#ifndef OFXOCEANODE_HEADLESS

#include "ofxDatGui.h"

class ofxOceanodeContainer;
class ofxOceanodeNode;
class ofxDatGui;

class ofxOceanodeNodeGui{
public:
    ofxOceanodeNodeGui(ofxOceanodeContainer &container, ofxOceanodeNode &node, shared_ptr<ofAppBaseWindow> window);
    ~ofxOceanodeNodeGui();
    
    void createGuiFromParameters(shared_ptr<ofAppBaseWindow> window);
    void updateGui();
    void updateGuiForParameter(string &parameterName);
    void updateDropdown(string &dropdownName);
    
    void setPosition(glm::vec2 position);
    
    shared_ptr<ofParameterGroup> getParameters();
    glm::vec2 getPosition();
    ofRectangle getRectangle();
    void collapse();
    void expand();
    
    void setWindow(shared_ptr<ofAppBaseWindow> window);
    
    void enable();
    void disable();
    
    glm::vec2 getSourceConnectionPositionFromParameter(ofAbstractParameter& parameter);
    glm::vec2 getSinkConnectionPositionFromParameter(ofAbstractParameter& parameter);
    void setTransformationMatrix(ofParameter<glm::mat4> *mat);
    
#ifdef OFXOCEANODE_USE_MIDI
    void setIsListeningMidi(bool b){isListeningMidi = b;};
#endif
    
    void keyPressed(ofKeyEventArgs &args);
    void keyReleased(ofKeyEventArgs &args);
    void mouseMoved(ofMouseEventArgs &args){};
    void mouseDragged(ofMouseEventArgs &args);
    void mousePressed(ofMouseEventArgs &args);
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
    
    ofEventListeners keyAndMouseListeners;
    ofEventListeners parameterChangedListeners;
    ofEventListener transformMatrixListener;
    
    ofxOceanodeContainer& container;
    
    ofxOceanodeNode& node;
    
    unique_ptr<ofxDatGui> gui;
    unique_ptr<ofxDatGuiTheme> theme;
    ofColor color;
    glm::vec2 position;
    
    bool guiToBeDestroyed;
    bool lastExpandedState;
    bool isGuiCreated;
    
#ifdef OFXOCEANODE_USE_MIDI
    bool isListeningMidi;
#endif
    
    ofParameter<glm::mat4> *transformationMatrix;
};

#endif

#endif /* ofxOceanodeNodeGui_h */
