//
//  ofxOceanodeNode.h
//  example-basic
//
//  Created by Eduard Frigola on 22/06/2017.
//
//

#ifndef ofxOceanodeNode_h
#define ofxOceanodeNode_h

#include "ofxOceanodeNodeModel.h"
#include "ofxDatGui.h"

class ofxOceanodeNode {
public:
    ofxOceanodeNode(unique_ptr<ofxOceanodeNodeModel> && _nodeModel);
    ~ofxOceanodeNode(){};
    
    void createGuiFromParameters();
    bool updateGuiFromParameters();
    
    void setPosition(glm::vec2 position);
    
    ofParameterGroup* getParameters(){return nodeModel->getParameterGroup();};
    
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
    
    
    void parameterListener(ofAbstractParameter &parameter);
    vector<ofEventListener> parameterChangedListeners;
    
    unique_ptr<ofxOceanodeNodeModel> nodeModel;
    
    shared_ptr<ofxDatGui> gui;
    ofColor color;
    ofPoint position;
};

#endif /* ofxOceanodeNode_h */
