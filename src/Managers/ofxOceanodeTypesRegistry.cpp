//
//  ofxOceanodeTypesRegistry.cpp
//  example-basic
//
//  Created by Eduard Frigola on 19/04/2018.
//

#include "ofxOceanodeTypesRegistry.h"

ofxOceanodeTypesRegistry::ofxOceanodeTypesRegistry(){
    this->registerType<float>();
    this->registerType<int>();
    this->registerType<bool>();
    this->registerType<void>();
    this->registerType<string>();
    this->registerType<char>();
    this->registerType<vector<float>>();
    this->registerType<vector<int>>();
}

ofxOceanodeAbstractConnection* ofxOceanodeTypesRegistry::createCustomTypeConnection(ofxOceanodeContainer &container, ofAbstractParameter &source, ofAbstractParameter &sink, bool active){
    for(auto functionForType : registryColector){
        auto connection = functionForType(container, source, sink, active);
        if(connection != nullptr) return connection;
    }
    return nullptr;
}
