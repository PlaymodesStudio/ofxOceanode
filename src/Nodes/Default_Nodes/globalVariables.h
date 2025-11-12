//
//  globalVariables.h
//  ofxOceanode
//
//  Created by Eduard Frigola on 18/12/23.
//

#ifndef globalVariables_h
#define globalVariables_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeGlobalVariablesController.h"

class globalVariables : public ofxOceanodeNodeModel{
public:
    globalVariables(std::string _name, std::weak_ptr<globalVariablesGroup> _group) : ofxOceanodeNodeModel("Globals " + _name), name(_name), group(_group){
        description = "Contains all the variables stored globally for the " + name + " category";
    };
    ~globalVariables(){
        std::shared_ptr<globalVariablesGroup> sharedGroup = group.lock();
        if(sharedGroup){
            sharedGroup->removeNode(this);
        }
    };
    
    void setup() override{
        std::shared_ptr<globalVariablesGroup> sharedGroup = group.lock();
        if(sharedGroup){
            sharedGroup->addNode(this);
        }
    }
    
    void addFloatParameter(std::shared_ptr<ofParameter<float>> &p){
        auto createdParam = floatParameters.emplace_back(new ofParameter<float>(p->getName(), p->get(), p->getMin(), p->getMax()));
        addParameter(createdParam, ofxOceanodeParameterFlags_ReadOnly | ofxOceanodeParameterFlags_DisableSavePreset);
        listeners.push(p->newListener([this, createdParam](float &f){
            createdParam->set(f);
        }));
    }

    void addIntParameter(std::shared_ptr<ofParameter<int>> &p){
        auto createdParam = intParameters.emplace_back(new ofParameter<int>(p->getName(), p->get(), p->getMin(), p->getMax()));
        addParameter(createdParam, ofxOceanodeParameterFlags_ReadOnly | ofxOceanodeParameterFlags_DisableSavePreset);
        listeners.push(p->newListener([this, createdParam](int &i){
            createdParam->set(i);
        }));
    }

    void addBoolParameter(std::shared_ptr<ofParameter<bool>> &p){
        auto createdParam = boolParameters.emplace_back(new ofParameter<bool>(p->getName(), p->get(), p->getMin(), p->getMax()));
        addParameter(createdParam, ofxOceanodeParameterFlags_ReadOnly | ofxOceanodeParameterFlags_DisableSavePreset);
        listeners.push(p->newListener([this, createdParam](bool &b){
            createdParam->set(b);
        }));
        
    }

    void addStringParameter(std::shared_ptr<ofParameter<string>> &p){
        auto createdParam = stringParameters.emplace_back(new ofParameter<string>(p->getName(), p->get(), p->getMin(), p->getMax()));
        addParameter(createdParam, ofxOceanodeParameterFlags_ReadOnly | ofxOceanodeParameterFlags_DisableSavePreset);
        listeners.push(p->newListener([this, createdParam](string &s){
            createdParam->set(s);
        }));
        
    }

    void addOfColorParameter(std::shared_ptr<ofParameter<ofColor>> &p){
        auto createdParam = colorParameters.emplace_back(new ofParameter<ofColor>(p->getName(), p->get(), p->getMin(), p->getMax()));
        addParameter(createdParam, ofxOceanodeParameterFlags_ReadOnly | ofxOceanodeParameterFlags_DisableSavePreset);
        listeners.push(p->newListener([this, createdParam](ofColor &c){
            createdParam->set(c);
        }));
    }

    void addOfFloatColorParameter(std::shared_ptr<ofParameter<ofFloatColor>> &p){
        auto createdParam = fcolorParameters.emplace_back(new ofParameter<ofFloatColor>(p->getName(), p->get(), p->getMin(), p->getMax()));
        addParameter(createdParam, ofxOceanodeParameterFlags_ReadOnly | ofxOceanodeParameterFlags_DisableSavePreset);
        listeners.push(p->newListener([this, createdParam](ofFloatColor &fc){
            createdParam->set(fc);
        }));    }


    void removeParameter(std::string parameterName){
        ofxOceanodeNodeModel::removeParameter(parameterName);
        floatParameters.erase(std::remove_if(floatParameters.begin(), floatParameters.end(), [parameterName](auto &parameter){return parameter->getName() == parameterName;}), floatParameters.end());
        intParameters.erase(std::remove_if(intParameters.begin(), intParameters.end(), [parameterName](auto &parameter){return parameter->getName() == parameterName;}), intParameters.end());
        boolParameters.erase(std::remove_if(boolParameters.begin(), boolParameters.end(), [parameterName](auto &parameter){return parameter->getName() == parameterName;}), boolParameters.end());
        stringParameters.erase(std::remove_if(stringParameters.begin(), stringParameters.end(), [parameterName](auto &parameter){return parameter->getName() == parameterName;}), stringParameters.end());
        colorParameters.erase(std::remove_if(colorParameters.begin(), colorParameters.end(), [parameterName](auto &parameter){return parameter->getName() == parameterName;}), colorParameters.end());
        fcolorParameters.erase(std::remove_if(fcolorParameters.begin(), fcolorParameters.end(), [parameterName](auto &parameter){return parameter->getName() == parameterName;}), fcolorParameters.end());
    }
    
private:
    std::string name;
    std::weak_ptr<globalVariablesGroup> group;
    ofEventListeners listeners;
    
    std::vector<std::shared_ptr<ofParameter<float>>> floatParameters;
    std::vector<std::shared_ptr<ofParameter<int>>> intParameters;
    std::vector<std::shared_ptr<ofParameter<bool>>> boolParameters;
    std::vector<std::shared_ptr<ofParameter<string>>> stringParameters;
    std::vector<std::shared_ptr<ofParameter<ofColor>>> colorParameters;
    std::vector<std::shared_ptr<ofParameter<ofFloatColor>>> fcolorParameters;
};

#endif /* globalVariables_h */
