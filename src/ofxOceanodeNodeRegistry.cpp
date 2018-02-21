//
//  ofxOceanodeNodeRegistry.cpp
//  example-basic
//
//  Created by Eduard Frigola on 22/06/2017.
//
//

#include "ofxOceanodeNodeRegistry.h"
#include "defaultNodes.h"

ofxOceanodeNodeRegistry::ofxOceanodeNodeRegistry(){
    this->registerModel<baseOscillator>();
    this->registerModel<phasorClass>();
}

std::unique_ptr<ofxOceanodeNodeModel> ofxOceanodeNodeRegistry::create(const string typeName){
    auto it = registeredModels.find(typeName);
    
    if (it != registeredModels.end())
    {
        return it->second->clone();
    }
    
    return nullptr;
}


ofxOceanodeNodeRegistry::registeredModelsMap const &ofxOceanodeNodeRegistry::getRegisteredModels(){
    return registeredModels;
}
