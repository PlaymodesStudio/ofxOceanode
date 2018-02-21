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

class testNode : public ofxOceanodeNodeModel {
public:
    testNode();
    ~testNode(){};
    
    std::unique_ptr<ofxOceanodeNodeModel> clone() const override {return make_unique<testNode>();};
private:
    ofEventListener listener;
    
    ofParameter<int> intParam;
    ofParameter<float>  floatParam;
    ofParameter<bool>   boolParam;
    ofParameter<void>   voidParam;
    ofParameter<string> textParam;
    ofParameter<char> labelParam;
    ofParameter<ofColor>  colorParam;
};

#endif /* testNode_h */
