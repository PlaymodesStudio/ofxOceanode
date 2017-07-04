//
//  ofxOceanodeNodeRegistry.cpp
//  example-basic
//
//  Created by Eduard Frigola on 22/06/2017.
//
//

#include "ofxOceanodeNodeRegistry.h"

std::unique_ptr<ofxOceanodeNodeModel> ofxOceanodeNodeRegistry::create(const string typeName){
    auto it = registeredTypes.find(typeName);
    
    if (it != registeredTypes.end())
    {
        return it->second->clone();
    }
    
    return nullptr;
}


ofxOceanodeNodeRegistry::registeredTypesMap const &ofxOceanodeNodeRegistry::getRegisteredTypes(){
    return registeredTypes;
}
