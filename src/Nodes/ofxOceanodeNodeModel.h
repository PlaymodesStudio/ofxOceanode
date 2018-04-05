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
    parameterInfo() : isSavePreset(true), isSaveProject(true), acceptInConnection(true), acceptOutConnection(true){};
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
    
    virtual void presetSave(ofJson &json){};
    virtual void presetRecallBeforeSettingParameters(ofJson &json){};
    virtual void presetRecallAfterSettingParameters(ofJson &json){};
    
    ofColor getColor(){return color;};
    
    parameterInfo& addParameterToGroupAndInfo(ofAbstractParameter& p);
    const parameterInfo getParameterInfo(ofAbstractParameter& p);
    const parameterInfo getParameterInfo(string parameterName);
    
protected:
    ofParameterGroup* parameters;
    std::map<string, parameterInfo> parametersInfo; //information about interaction of parameter
    bool autoBPM;
    ofColor color;
    bool isDynamic;
    string nameIdentifier;
    uint numIdentifier;
    
private:
    vector<ofEventListener> eventListeners;
};

#endif /* ofxOceanodeNodeModel_h */
