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

    // Each node owns its own copy of a group variable, which follows the
    // group's value; sharing one parameter between nodes breaks connection loading.
    void addVariable(const std::shared_ptr<ofAbstractParameter>& groupParameter){
        Variable variable;
        variable.groupParameter = groupParameter;
        if(!createLocal(variable)) return;
        variables.push_back(std::move(variable));
    }

    void removeVariable(const ofAbstractParameter* groupParameter){
        auto it = findVariable(groupParameter);
        if(it == variables.end()) return;
        removeLocal(*it);
        variables.erase(it);
        parameterGroupChanged.notify();
    }

    void renameVariable(const ofAbstractParameter* groupParameter, const std::string& newName){
        auto it = findVariable(groupParameter);
        if(it == variables.end()) return;
        it->localParameter->setName(newName);
        parameterGroupChanged.notify();
    }

    // Swaps a variable for one of another type in the same row, reconnecting
    // whatever the new type can still feed.
    void replaceVariable(const ofAbstractParameter* oldGroupParameter, const std::shared_ptr<ofAbstractParameter>& newGroupParameter);

    // Puts the pins in the group's row order.
    void sortVariables(const std::vector<std::shared_ptr<ofAbstractParameter>>& groupParameters);

private:
    struct Variable {
        std::shared_ptr<ofAbstractParameter> groupParameter;
        std::shared_ptr<ofAbstractParameter> localParameter;
        std::unique_ptr<of::priv::AbstractEventToken> listener;
    };

    std::vector<Variable>::iterator findVariable(const ofAbstractParameter* groupParameter){
        return std::find_if(variables.begin(), variables.end(), [groupParameter](const Variable& variable){
            return variable.groupParameter.get() == groupParameter;
        });
    }

    template<typename T>
    void createLocalOfType(Variable& variable){
        auto source = std::dynamic_pointer_cast<ofParameter<T>>(variable.groupParameter);
        auto createdParam = std::make_shared<ofParameter<T>>(source->getName(), source->get(), source->getMin(), source->getMax());
        addOutputParameter(createdParam, ofxOceanodeParameterFlags_ReadOnly | ofxOceanodeParameterFlags_DisableSavePreset);
        variable.listener = source->newListener([createdParam](T &value){
            createdParam->set(value);
        });
        variable.localParameter = createdParam;
    }

    bool createLocal(Variable& variable){
        switch(globalVariableTypeOf(*variable.groupParameter)){
            case globalVariableType_Float: createLocalOfType<float>(variable); return true;
            case globalVariableType_Int: createLocalOfType<int>(variable); return true;
            case globalVariableType_Bool: createLocalOfType<bool>(variable); return true;
            case globalVariableType_String: createLocalOfType<std::string>(variable); return true;
            case globalVariableType_Color: createLocalOfType<ofColor>(variable); return true;
            case globalVariableType_FloatColor: createLocalOfType<ofFloatColor>(variable); return true;
        }
        return false;
    }

    void removeLocal(Variable& variable){
        variable.listener.reset();
        const std::string parameterName = variable.localParameter->getName();
        if(getParameterGroup().contains(parameterName)){
            static_cast<ofxOceanodeAbstractParameter&>(getParameterGroup().get(parameterName)).removeAllConnections();
            ofxOceanodeNodeModel::removeParameter(parameterName);
        }
        variable.localParameter.reset();
    }

    std::string name;
    std::weak_ptr<globalVariablesGroup> group;
    std::vector<Variable> variables;
};

#endif /* globalVariables_h */
