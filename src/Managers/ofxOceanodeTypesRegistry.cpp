//
//  ofxOceanodeTypesRegistry.cpp
//  example-basic
//
//  Created by Eduard Frigola on 19/04/2018.
//

#include "ofxOceanodeTypesRegistry.h"

ofxOceanodeTypesRegistry::ofxOceanodeTypesRegistry(){
    
}

ofxOceanodeAbstractConnection* ofxOceanodeTypesRegistry::createCustomTypeConnection(ofxOceanodeContainer &container, ofxOceanodeAbstractParameter &source, ofxOceanodeAbstractParameter &sink, bool active){
    for(auto functionForType : registryColector){
        auto connection = functionForType(container, source, sink, active);
        if(connection != nullptr) return connection;
    }
    return nullptr;
}

shared_ptr<ofAbstractParameter> ofxOceanodeTypesRegistry::createRouterFromType(ofxOceanodeNode* routerNode){
    for(auto functionForType : routerColector){
        auto param = functionForType(routerNode);
        if(param != nullptr) return param;
    }
    return nullptr;
}

shared_ptr<ofxOceanodeAbstractParameter> ofxOceanodeTypesRegistry::createOceanodeAbstractFromAbstract(ofAbstractParameter &p){
    for(auto functionForType : absParamColector){
        auto param = functionForType(p);
        if(param != nullptr) return param;
    }
    return nullptr;
}
