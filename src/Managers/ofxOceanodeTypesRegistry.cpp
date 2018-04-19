//
//  ofxOceanodeTypesRegistry.cpp
//  example-basic
//
//  Created by Eduard Frigola on 19/04/2018.
//

#include "ofxOceanodeTypesRegistry.h"

ofxOceanodeAbstractConnection* ofxOceanodeTypesRegistry::createCustomTypeConnection(ofxOceanodeContainer &container, ofAbstractParameter &source, ofAbstractParameter &sink){
    for(auto functionForType : registryColector){
        auto connection = functionForType(container, source, sink);
        if(connection != nullptr) return connection;
    }
    return nullptr;
}
