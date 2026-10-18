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
    if(inConnection != nullptr) inConnection->deleteSelf();
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
