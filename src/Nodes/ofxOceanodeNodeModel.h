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
class ofxOscMessage;

class ofxOceanodeNodeModel {
public:
    using customGuiRegion = ofParameter<std::function<void()>>;
    
    ofxOceanodeNodeModel(string _name);
    virtual ~ofxOceanodeNodeModel(){};
    
    virtual void setup(){};
    virtual void update(ofEventArgs &e){};
    virtual void draw(ofEventArgs &e){};
    
    //get parameterGroup
    ofParameterGroup &getParameterGroup(){return parameters;};
    
    //getters
    string nodeName(){return nameIdentifier;};
    unsigned int getNumIdentifier(){return numIdentifier;};
    void setNumIdentifier(unsigned int num);
        
    ofEvent<string> parameterChangedMinMax;
    ofEvent<string> dropdownChanged;
    ofEvent<void> parameterGroupChanged;
    
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
    
    shared_ptr<ofxOceanodeAbstractParameter> addParameter(ofAbstractParameter& p, ofxOceanodeParameterFlags flags = 0);
	
	template<typename ParameterType>
	shared_ptr<ofxOceanodeParameter<ParameterType>> addParameter(ofParameter<ParameterType>& p, ofxOceanodeParameterFlags flags = 0){
		//TODO: Review if we loose the data?
		auto oceaParam = make_shared<ofxOceanodeParameter<ParameterType>>();
		oceaParam->bindParameter(p);
		oceaParam->setFlags(flags);
		parameters.add(*oceaParam);
		return dynamic_pointer_cast<ofxOceanodeParameter<ParameterType>>(*(parameters.end()-1));
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
    
    shared_ptr<ofxOceanodeParameter<std::function<void()>>> addCustomRegion(customGuiRegion &p, std::function<void()> func){
        return addParameter(p.set(func), ofxOceanodeParameterFlags_DisableInConnection | ofxOceanodeParameterFlags_DisableOutConnection | ofxOceanodeParameterFlags_DisableSavePreset | ofxOceanodeParameterFlags_DisableSaveProject);
    }
    
    template<typename ParameterType>
    ofParameter<ParameterType>& getParameter(string name){
        return static_cast<ofxOceanodeAbstractParameter&>(parameters.get(name)).cast<ParameterType>().getParameter();
    }
    
    ofEvent<std::pair<ofJson, string>> deserializeParameterEvent;
    
protected:
    ofColor color;
    
private:
    string nameIdentifier;
    unsigned int numIdentifier;
	ofParameterGroup parameters;
    ofEventListeners eventListeners;
};

#endif /* ofxOceanodeNodeModel_h */
