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

struct parameterInfo{
    bool isSavePreset;
    bool isSaveProject;
    bool acceptInConnection;
    bool acceptOutConnection;
};

class ofxOceanodeNodeModel {
public:
    ofxOceanodeNodeModel(string _name);
    ~ofxOceanodeNodeModel(){};
    
    //get parameterGroup
    ofParameterGroup* getParameterGroup(){return parameters;};
    
    //getters
    bool getIsDynamic(){return isDynamic;};
    string nodeName(){return nameIdentifier;};
    
    /// Function creates instances of a model stored in ofxOceanodeNodeRegistry
    virtual std::unique_ptr<ofxOceanodeNodeModel> clone() const = 0;
    
protected:
    ofParameterGroup* parameters;
//    std::map<ofAbstractParameter&, parameterInfo> parametersInfo; //information about interaction of parameter
    
private:
    bool isDynamic;
    string nameIdentifier;
};

#endif /* ofxOceanodeNodeModel_h */
