//
//  ofxOceanodeNodeModel.hpp
//  example-basic
//
//  Created by Eduard Frigola on 19/06/2017.
//
//

#ifndef ofxOceanodeNodeModel_h
#define ofxOceanodeNodeModel_h

#include "ofMain.h"

class ofxOceanodeContainer;
class ofxOceanodeAbstractConnection;

struct parameterInfo{
    bool isSavePreset;
    bool isSaveProject;
    bool acceptInConnection;
    bool acceptOutConnection;
};

class ofxOceanodeNodeModel {
public:
    ofxOceanodeNodeModel(string _name);
    virtual ~ofxOceanodeNodeModel(){};
    
    //get parameterGroup
    ofParameterGroup* getParameterGroup(){return parameters;};
    
    //getters
    bool getIsDynamic(){return isDynamic;};
    string nodeName(){return nameIdentifier;};
    
    virtual ofxOceanodeAbstractConnection* createConnectionFromCustomType(ofxOceanodeContainer& c, ofAbstractParameter& source, ofAbstractParameter& sink){return nullptr;};
    
    ofEvent<string> parameterChangedMinMax;
    
protected:
    ofParameterGroup* parameters;
//    std::map<ofAbstractParameter&, parameterInfo> parametersInfo; //information about interaction of parameter
    
private:
    bool isDynamic;
    string nameIdentifier;
};

#endif /* ofxOceanodeNodeModel_h */
