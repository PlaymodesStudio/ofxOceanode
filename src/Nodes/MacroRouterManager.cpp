//
//  MacroRouterManager.cpp
//  ofxOceanode
//
//  Extracted from ofxOceanodeNodeMacro — Phase 4 refactoring.
//  Implementations for router-map management methods.
//

#include "MacroRouterManager.h"
#include "ofxOceanodeNodeMacro.h"   // RouterInfo definition
#include "ofxOceanodeContainer.h"   // container->getAllModules()
#include "ofxOceanodeNode.h"
#include "router.h"                 // abstractRouter

MacroRouterManager::MacroRouterManager() {
}

void MacroRouterManager::updateAllConnections(ofxOceanodeContainer* container) {
	routerNodes.clear();

	auto nodes = container->getAllModules();
	if(nodes.empty()) return;

	for(auto* node : nodes) {
		if(!node) continue;

		if(dynamic_cast<abstractRouter*>(&node->getNodeModel()) != nullptr) {
			updateInfo(node);
		}
	}
}

void MacroRouterManager::updateInfo(ofxOceanodeNode* node) {
	auto& params = node->getParameters();

	// Typed routers expose "Value"; void routers expose "Val".
	// Both should be registered in routerNodes so rename/sort works for all.
	string valueParamName = "";
	if(params.contains("Value"))     valueParamName = "Value";
	else if(params.contains("Val"))  valueParamName = "Val";

	RouterInfo info;
	info.node       = node;
	info.routerName = static_cast<abstractRouter&>(node->getNodeModel()).getNameParam().get();
	info.isInput    = checkIsInput(node);

	if(!valueParamName.empty()){
		auto valueParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&params.get(valueParamName));
		if(valueParam) info.parameterType = valueParam->valueType();
	}

	routerNodes[info.routerName] = info;
}

bool MacroRouterManager::checkIsInput(ofxOceanodeNode* node) const {
	auto& params = node->getParameters();
	// Typed routers use "Value"; void routers use "Val".
	string valName = params.contains("Value") ? "Value" :
	                 (params.contains("Val")   ? "Val"   : "");
	if(valName.empty()) return false;
	auto valueParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&params.get(valName));
	if(!valueParam) return false;
	return !valueParam->hasInConnection();
}

void MacroRouterManager::renameRouter(const std::string& oldName, const std::string& newName) {
	auto nodeIt = routerNodes.find(oldName);
	if(nodeIt != routerNodes.end()){
		RouterInfo info = nodeIt->second;
		info.routerName = newName;
		routerNodes.erase(nodeIt);
		routerNodes[newName] = info;
	}
}

void MacroRouterManager::removeRouter(const std::string& name) {
	routerNodes.erase(name);
}
