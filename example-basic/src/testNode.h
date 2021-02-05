//
//  testNode.h
//  example-basic
//
//  Created by Eduard Frigola on 22/06/2017.
//
//

#ifndef testNode_h
#define testNode_h

#include "ofMain.h"
#include "ofxOceanodeNodeModel.h"

class testController;

class customClass{
public:
    customClass(){value = 1;};
    ~customClass(){};
    
    int value;
};

class testNode : public ofxOceanodeNodeModel {
public:
    testNode(shared_ptr<testController> tController);
    ~testNode(){};
    
    void update(ofEventArgs &a){};

    void setBpm(float _bpm) override {bpm = _bpm;};
    
private:
    ofEventListener listener;
    ofEventListener listener2;
    ofEventListener listener3;
    ofEventListener listener4;
    ofEventListener listener5;
    
    ofParameter<customClass*> intModParam;
    ofParameter<int> intParam;
    ofParameter<float>  floatParam;
    ofParameter<bool>   boolParam;
    ofParameter<void>   voidParam;
    ofParameter<string> textParam;
    ofParameter<char> labelParam;
    ofParameter<ofFloatColor>  colorParam;
    
    ofParameter<ofColor> inspectorColor;
    
    shared_ptr<testController> controller;
    
    float bpm;
};

#endif /* testNode_h */
