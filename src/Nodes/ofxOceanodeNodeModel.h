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
    parameterInfo(bool spres = true, bool sproj = true, bool inc = true, bool outc = true) : isSavePreset(spres), isSaveProject(sproj), acceptInConnection(inc), acceptOutConnection(outc){};
    void convertToProject(){
        isSavePreset = false;
        isSaveProject = true;
    }
};

class ofxOceanodeNodeModel {
public:
    ofxOceanodeNodeModel(string _name);
    virtual ~ofxOceanodeNodeModel(){};
    
    virtual void setup(){};
    virtual void update(ofEventArgs &e){};
    virtual void draw(ofEventArgs &e){};
    
    //get parameterGroup
    shared_ptr<ofParameterGroup> getParameterGroup(){return parameters;};
    
    //getters
    string nodeName(){return nameIdentifier;};
    unsigned int getNumIdentifier(){return numIdentifier;};
    void setNumIdentifier(unsigned int num);
        
    ofEvent<string> parameterChangedMinMax;
    ofEvent<string> dropdownChanged;
    ofEvent<void> parameterGroupChanged;
    ofEvent<string> disconnectConnectionsForParameter;
    
    void registerLoop(shared_ptr<ofAppBaseWindow> w = nullptr);
    
    bool getAutoBPM(){return autoBPM;};
    virtual void setBpm(float _bpm){};
    virtual void setPhase(float _phase){};
    
    virtual void presetSave(ofJson &json){};
    virtual void presetRecallBeforeSettingParameters(ofJson &json){};
    virtual void presetRecallAfterSettingParameters(ofJson &json){};
    virtual void loadCustomPersistent(ofJson &json){};
    
    virtual void presetWillBeLoaded(){};
    virtual void presetHasLoaded(){};
    virtual void loadBeforeConnections(ofJson &json){};
    
    ofColor getColor(){return color;};
    
    parameterInfo& addParameterToGroupAndInfo(ofAbstractParameter& p);
    parameterInfo& addOutputParameterToGroupAndInfo(ofAbstractParameter& p);
    const parameterInfo getParameterInfo(ofAbstractParameter& p);
    const parameterInfo getParameterInfo(string parameterName);
    
    ofAbstractParameter& createDropdownAbstractParameter(string name, vector<string> options, ofParameter<int> &dropdownSelector){
        dropdownGroups.emplace_back();
        dropdownGroups.back().setName(name + " Selector");
        string  tempStr;
        ofParameter<string> tempStrParam("Options");
        for(auto opt : options)
            tempStr += opt + "-|-";
        
        tempStr.erase(tempStr.end()-3, tempStr.end());
        tempStrParam.set(tempStr);
        
        dropdownGroups.back().add(tempStrParam);
        dropdownGroups.back().add(dropdownSelector.set(name, 0, 0, options.size()-1));
        return dropdownGroups.back();
    }
    
protected:
    shared_ptr<ofParameterGroup> parameters;
    std::map<string, parameterInfo> parametersInfo; //information about interaction of parameter
    bool autoBPM;
    ofColor color;
    string nameIdentifier;
    unsigned int numIdentifier;
    vector<ofParameterGroup> dropdownGroups;
    
private:
    ofEventListeners eventListeners;
};

#endif /* ofxOceanodeNodeModel_h */
