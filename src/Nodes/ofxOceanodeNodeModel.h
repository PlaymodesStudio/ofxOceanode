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
#include "ofxOceanodeParameter.h"

class ofxOceanodeContainer;
class ofxOceanodeAbstractConnection;
class ofxOscMessage;

//template <typename T>
class parameterInfo{
public:
    parameterInfo(bool spres = true, bool sproj = true, bool inc = true, bool outc = true) : isSavePreset(spres), isSaveProject(sproj), acceptInConnection(inc), acceptOutConnection(outc){};
    
    void convertToProject(){
        isSavePreset = false;
        isSaveProject = true;
    }
    
    bool isSavePreset;
    bool isSaveProject;
    bool acceptInConnection;
    bool acceptOutConnection;
    
    vector<string> dropdownOptions;
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
    
    virtual void setBpm(float _bpm){};
    virtual void resetPhase(){};
    
    virtual void presetSave(ofJson &json){};
    virtual void presetRecallBeforeSettingParameters(ofJson &json){};
    virtual void presetRecallAfterSettingParameters(ofJson &json){};
    virtual void loadCustomPersistent(ofJson &json){};
    
    virtual void presetWillBeLoaded(){};
    virtual void presetHasLoaded(){};
    virtual void loadBeforeConnections(ofJson &json){};
    
    void deserializeParameter(ofJson &json, ofAbstractParameter &p){
        auto pair = std::make_pair(json, p.getName());
        deserializeParameterEvent.notify(pair);
    }
    
    ofColor getColor(){return color;};
    
    //For Macro
    virtual bool receiveOscMessage(ofxOscMessage &m){return false;};
    virtual void setContainer(ofxOceanodeContainer* container){};
	
	template<typename ParameterType>
	shared_ptr<ofxOceanodeParameter<ParameterType>> addParameter(ofParameter<ParameterType>& p, ofxOceanodeParameterFlags flags = 0){
		//TODO: Review if we loose the data?
		auto oceaParam = make_shared<ofxOceanodeParameter<ParameterType>>();
		oceaParam->bindParameter(p);
		oceaParam->setFlags(flags);
		parameters->add(*oceaParam);
		return dynamic_pointer_cast<ofxOceanodeParameter<ParameterType>>(*(parameters->end()-1));
	}
	
	template<typename ParameterType>
	shared_ptr<ofxOceanodeParameter<ParameterType>> addOutputParameter(ofParameter<ParameterType>& p, ofxOceanodeParameterFlags flags = 0){
		auto oceaParam = addParameter(p, flags | ofxOceanodeParameterFlags_DisableInConnection);
		return oceaParam;
	}
    
    shared_ptr<ofxOceanodeParameter<int>> addParameterDropdown(ofParameter<int> &dropdownSelector, string name, int defaultPos, vector<string> options, ofxOceanodeParameterFlags flags = 0){
        dropdownSelector.set(name, defaultPos, 0, options.size()-1);
		auto op = addParameter(dropdownSelector, flags);
		op->setDropdownOptions(options);
        return op;
    }
    ofEvent<std::pair<ofJson, string>> deserializeParameterEvent;
    
protected:
    ofColor color;
    string nameIdentifier;
    unsigned int numIdentifier;
    
private:
	shared_ptr<ofParameterGroup> parameters;
    ofEventListeners eventListeners;
};

#endif /* ofxOceanodeNodeModel_h */
