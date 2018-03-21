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
    
    virtual void update(ofEventArgs &e){};
    virtual void draw(ofEventArgs &e){};
    
    //get parameterGroup
    ofParameterGroup* getParameterGroup(){return parameters;};
    
    //getters
    bool getIsDynamic(){return isDynamic;};
    string nodeName(){return nameIdentifier;};
    uint getNumIdentifier(){return numIdentifier;};
    void setNumIdentifier(uint num);
    
    virtual ofxOceanodeAbstractConnection* createConnectionFromCustomType(ofxOceanodeContainer& c, ofAbstractParameter& source, ofAbstractParameter& sink){return nullptr;};
    
    ofEvent<string> parameterChangedMinMax;
    
    void registerLoop(shared_ptr<ofAppBaseWindow> w = nullptr);
    
    bool getAutoBPM(){return autoBPM;};
    virtual void setBpm(float _bpm){};
    
protected:
    ofParameterGroup* parameters;
//    std::map<ofAbstractParameter&, parameterInfo> parametersInfo; //information about interaction of parameter
    bool autoBPM;
private:
    vector<ofEventListener> eventListeners;
    bool isDynamic;
    string nameIdentifier;
    uint numIdentifier;
};

#endif /* ofxOceanodeNodeModel_h */
