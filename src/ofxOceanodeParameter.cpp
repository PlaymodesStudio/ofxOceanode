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

ofxOceanodeAbstractParameter::~ofxOceanodeAbstractParameter(){
    for(auto* peer : dropdownPeers){
        auto& peers = peer->dropdownPeers;
        peers.erase(std::remove(peers.begin(), peers.end(), this), peers.end());
    }
    if(!dropdownPeers.empty()) touchDropdownGraph();
}

void ofxOceanodeAbstractParameter::linkDropdownPeer(ofxOceanodeAbstractParameter& peer){
    if(&peer == this || std::find(dropdownPeers.begin(), dropdownPeers.end(), &peer) != dropdownPeers.end()) return;
    dropdownPeers.push_back(&peer);
    peer.dropdownPeers.push_back(this);
    touchDropdownGraph();
}

std::vector<ofxOceanodeAbstractParameter*> ofxOceanodeAbstractParameter::getConnectedDropdownSources(){
    std::vector<ofxOceanodeAbstractParameter*> pending{this}, visited, sources;
    while(!pending.empty()){
        auto* parameter = pending.back(); pending.pop_back();
        if(std::find(visited.begin(), visited.end(), parameter) != visited.end()) continue;
        visited.push_back(parameter);
        if(!parameter->dropdownRelay){
            if(parameter->dropdownSourceAvailable && !parameter->getDropdownOptions().empty()) sources.push_back(parameter);
            continue;
        }
        for(auto* connection : parameter->inConnections) pending.push_back(&connection->getSourceParameter());
        for(auto* connection : parameter->outConnections) pending.push_back(&connection->getSinkParameter());
        pending.insert(pending.end(), parameter->dropdownPeers.begin(), parameter->dropdownPeers.end());
    }
    return sources;
}

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
