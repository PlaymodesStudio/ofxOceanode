//
//  staticTestModule.h
//  example-persistentNodes
//
//  Created by Eduard Frigola Bagué on 16/08/2018.
//

#ifndef staticTestModule_h
#define staticTestModule_h

#include "ofxOceanodeNodeModel.h"

class staticTestModule : public ofxOceanodeNodeModel{
public:
    staticTestModule() : ofxOceanodeNodeModel("Static Test Module"){};
    ~staticTestModule(){};
    
    void setup() override{
        parameters->add(intParam.set("Int", 5, 0, 24));
        parameters->add(floatParam.set("Float", 0, 0, 1));
        addParameterToGroupAndInfo(stringParam.set("String", "I'm a Static")).isSavePreset = false;
    }
    
private:
    ofParameter<int> intParam;
    ofParameter<float> floatParam;
    ofParameter<string> stringParam;
};


#endif /* staticTestModule_h */
