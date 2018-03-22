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
    this->registerModel<oscillator>();
    this->registerModel<phasorClass>();
    this->registerModel<mapper>();
    this->registerModel<ranger>();
    this->registerModel<oscillatorBank>();
    this->registerModel<reindexer>();
    this->registerModel<smoother>();
}

std::unique_ptr<ofxOceanodeNodeModel> ofxOceanodeNodeRegistry::create(const string typeName){
    auto it = registeredModelCreators.find(typeName);
    
    if (it != registeredModelCreators.end())
    {
        return it->second();
    }
    
    return nullptr;
}


ofxOceanodeNodeRegistry::registeredModelsMap const &ofxOceanodeNodeRegistry::getRegisteredModels(){
    return registeredModelCreators;
}
