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
    this->registerModel<oscillator>("Generators");
    this->registerModel<chaoticOscillator>("Generators");
    this->registerModel<phasor>("Generators");
    this->registerModel<simpleNumberGenerator>("Generators");
    this->registerModel<mapper>("Modifiers");
    this->registerModel<ranger>("Modifiers");
    this->registerModel<indexer>("Generators");
    this->registerModel<reindexer>("Modifiers");
    this->registerModel<smoother>("Modifiers");
    this->registerModel<switcher>("Modifiers");
	this->registerModel<curve>("Modifiers");
    this->registerModel<ofxOceanodeNodeMacro>("MACRO");
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

ofxOceanodeNodeRegistry::registeredModelsCategoryMap const &ofxOceanodeNodeRegistry::getRegisteredModelsCategoryAssociation(){
    return registeredModelsCategory;
}


ofxOceanodeNodeRegistry::categoriesSet const &ofxOceanodeNodeRegistry::getCategories(){
    return categories;
}
