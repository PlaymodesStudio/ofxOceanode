//
//  MacroRouterSortOrder.cpp
//  ofxOceanode
//
//  Extracted from ofxOceanodeNodeMacro — Phase 1 refactoring.
//

#include "MacroRouterSortOrder.h"
#include "ofUtils.h"

// ─────────────────────────────────────────────────────────────────────────────
// Separator encoding / decoding
// ─────────────────────────────────────────────────────────────────────────────

bool MacroRouterSortOrder::isSortSeparatorEntry(const std::string& e) const {
	return e.size() > 7 && e.substr(0, 7) == "__SEP__";
}

std::string MacroRouterSortOrder::getSortSeparatorLabel(const std::string& e) const {
	// format: "__SEP__:label"  or  "__SEP__:label:r,g,b,a"
	size_t first = e.find(':');
	if(first == std::string::npos) return "";
	size_t second = e.find(':', first + 1);
	return (second == std::string::npos)
		? e.substr(first + 1)
		: e.substr(first + 1, second - first - 1);
}

ofColor MacroRouterSortOrder::getSortSeparatorColor(const std::string& e) const {
	size_t first = e.find(':');
	if(first == std::string::npos) return ofColor(200, 200, 200, 255);
	size_t second = e.find(':', first + 1);
	if(second == std::string::npos) return ofColor(200, 200, 200, 255);
	auto parts = ofSplitString(e.substr(second + 1), ",");
	if(parts.size() < 4) return ofColor(200, 200, 200, 255);
	return ofColor(ofToInt(parts[0]), ofToInt(parts[1]), ofToInt(parts[2]), ofToInt(parts[3]));
}

std::string MacroRouterSortOrder::makeSortSeparatorEntry(const std::string& label, const ofColor& c) const {
	return "__SEP__:" + label + ":" +
		ofToString((int)c.r) + "," + ofToString((int)c.g) + "," +
		ofToString((int)c.b) + "," + ofToString((int)c.a);
}

// ─────────────────────────────────────────────────────────────────────────────
// Order queries
// ─────────────────────────────────────────────────────────────────────────────

bool MacroRouterSortOrder::isRouterInSortOrder(const std::string& name) const {
	for(const auto& e : routerSortOrder)
		if(!isSortSeparatorEntry(e) && e == name) return true;
	return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Persistence
// ─────────────────────────────────────────────────────────────────────────────

void MacroRouterSortOrder::loadRouterSortFromJson(const ofJson& json) {
	// Load into the PENDING buffer, not into the active routerSortOrder.
	// The active routerSortOrder is cleared and rebuilt by allNodesCreated() once
	// all router nodes have been created and their nameParams pre-loaded.
	pendingRouterSortOrder.clear();
	if(json.contains("RouterSortOrder") && json["RouterSortOrder"].is_array()){
		for(const auto& item : json["RouterSortOrder"])
			pendingRouterSortOrder.push_back(item.get<std::string>());
	}
}
