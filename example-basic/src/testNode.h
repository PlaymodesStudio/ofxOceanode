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

class customClass{
public:
    customClass(){value = 1;};
    ~customClass(){};
    
    int value;
};

class testNode : public ofxOceanodeNodeModel {
public:
    testNode();
    ~testNode(){};
        
    ofxOceanodeAbstractConnection* createConnectionFromCustomType(ofxOceanodeContainer& c, ofAbstractParameter& source, ofAbstractParameter& sink) override;

    void setBpm(float _bpm) override {bpm = _bpm;};
    
private:
    ofEventListener listener;
    ofEventListener listener2;
    
    ofParameter<customClass*> intModParam;
    ofParameter<int> intParam;
    ofParameter<float>  floatParam;
    ofParameter<bool>   boolParam;
    ofParameter<void>   voidParam;
    ofParameter<string> textParam;
    ofParameter<char> labelParam;
    ofParameter<ofColor>  colorParam;
    
    float bpm;
};

#endif /* testNode_h */
