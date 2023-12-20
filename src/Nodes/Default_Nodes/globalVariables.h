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
    globalVariables(std::string _name, std::weak_ptr<globalVariablesGroup> _group) : ofxOceanodeNodeModel("Info " + _name), name(_name), group(_group){
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
    
private:
    std::string name;
    std::weak_ptr<globalVariablesGroup> group;
};

#endif /* globalVariables_h */
