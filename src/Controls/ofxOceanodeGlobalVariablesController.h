//
//  ofxOceanodeGlobalVariablesController.hpp
//  ofxOceanode
//
//  Created by Eduard Frigola on 18/12/23.
//

#ifndef ofxOceanodeGlobalVariablesController_h
#define ofxOceanodeGlobalVariablesController_h

#include "ofxOceanodeBaseController.h"

class globalVariables;

struct globalVariablesGroup : std::enable_shared_from_this<globalVariablesGroup> {
    globalVariablesGroup();
    globalVariablesGroup(string _name, shared_ptr<ofxOceanodeContainer> _container);
    
    ~globalVariablesGroup();
    
    void registerModule();
    
    void addNode(globalVariables* node);
    void removeNode(globalVariables* node);
    void addFloatParameter(std::string parameterName, float value = 0);
    void addIntParameter(std::string parameterName, int value = 0);
    void addBoolParameter(std::string parameterName, bool value = false);
    void addStringParameter(std::string parameterName, std::string value = "");
    void addOfColorParameter(std::string parameterName, ofColor value = ofColor::black);
    void addOfFloatColorParameter(std::string parameterName, ofFloatColor value = ofFloatColor::black);
    void removeParameter(std::string parameterName);
    
    string name;
    std::vector<std::shared_ptr<ofAbstractParameter>> parameters;
    std::vector<std::shared_ptr<ofParameter<float>>> floatParameters;
    std::vector<std::shared_ptr<ofParameter<int>>> intParameters;
    std::vector<std::shared_ptr<ofParameter<bool>>> boolParameters;
    std::vector<std::shared_ptr<ofParameter<string>>> stringParameters;
    std::vector<std::shared_ptr<ofParameter<ofColor>>> colorParameters;
    std::vector<std::shared_ptr<ofParameter<ofFloatColor>>> fcolorParameters;
    vector<globalVariables*> nodes;
    
    shared_ptr<ofxOceanodeContainer> container;
};

class ofxOceanodeGlobalVariablesController: public ofxOceanodeBaseController{
public:
    ofxOceanodeGlobalVariablesController(shared_ptr<ofxOceanodeContainer> _container);
    ~ofxOceanodeGlobalVariablesController(){};
    
    void draw();
    
    void save();
    void load();
    
private:
    shared_ptr<ofxOceanodeContainer> container;

    std::vector<std::shared_ptr<globalVariablesGroup>> groups;
};

#endif /* ofxOceanodeGlobalVariablesController_h */
