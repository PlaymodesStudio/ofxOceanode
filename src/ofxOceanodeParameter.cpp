//
//  ofxOceanodeParameter.cpp
//  example-basic
//
//  Created by Eduard Frigola on 29/04/2020.
//

#include "ofxOceanodeParameter.h"
#include "ofMath.h"
#include "ofxOceanodeConnection.h"
#include "ofxOceanodeScope.h"
#include "ofxOceanodeNodeModel.h"

void ofxOceanodeAbstractParameter::setNodeModel(ofxOceanodeNodeModel* model){
    nodeModel = model;
    profilingRecord = model ? model->getProfilingRecord() : nullptr;
    if(profilingHook) profilingHook->setRecord(profilingRecord);
}

void ofxOceanodeAbstractParameter::removeAllConnections(){
    while(hasInConnection()) getInConnection()->deleteSelf();
    while(outConnections.size() != 0) outConnections.front()->deleteSelf();
    
    if(hasScope) ofxOceanodeScope::getInstance()->removeParameter(this);
};

void ofxOceanodeAbstractParameter::forwardToOutConnections(){
    setForwardTargets([this](){
        std::vector<ofxOceanodeAbstractParameter*> sinks;
        for(auto* connection : outConnections) sinks.push_back(&connection->getSinkParameter());
        return sinks;
    });
}

bool ofxOceanodeAbstractParameter::refreshCombinedInput(){
    const auto combine = inputCombiners().find(valueType());
    if(combine == inputCombiners().end()) return false;
    std::vector<ofxOceanodeAbstractParameter*> sources;
    for(auto* connection : inConnections){
        if(connection->getActive()) sources.push_back(&connection->getSourceParameter());
    }
    combine->second(*this, sources, combinedInputState);
    return true;
}
