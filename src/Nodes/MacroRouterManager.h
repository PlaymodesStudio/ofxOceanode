//
//  MacroRouterManager.h
//  ofxOceanode
//
//  Extracted from ofxOceanodeNodeMacro — Phase 4 refactoring.
//  Owns the routerNodes map and dropdownRouterListeners, plus pure
//  query/update methods that don't need macro-level orchestration.
//

#ifndef MacroRouterManager_h
#define MacroRouterManager_h

#include <string>
#include <map>
#include <unordered_map>
#include "ofEvent.h"

// Forward declarations
class ofxOceanodeNode;
class ofxOceanodeContainer;
struct RouterInfo;

class MacroRouterManager {
public:
	MacroRouterManager();

	// Rebuild routerNodes by scanning the container for all router nodes
	void updateAllConnections(ofxOceanodeContainer* container);

	// Update a single router's info in the map
	void updateInfo(ofxOceanodeNode* node);

	// Determine if a router is an input router (no incoming connection on Value)
	bool checkIsInput(ofxOceanodeNode* node) const;

	// Router nodes map access
	const std::map<std::string, RouterInfo>& getRouterNodes() const { return routerNodes; }
	std::map<std::string, RouterInfo>& getRouterNodes() { return routerNodes; }
	void clearRouterNodes() { routerNodes.clear(); }

	// Dropdown listener management
	std::unordered_map<std::string, ofEventListener>& getDropdownListeners() { return dropdownRouterListeners; }
	const std::unordered_map<std::string, ofEventListener>& getDropdownListeners() const { return dropdownRouterListeners; }

	// Rename tracking helper (called when router name changes)
	void renameRouter(const std::string& oldName, const std::string& newName);

	// Remove a router from tracking
	void removeRouter(const std::string& name);

private:
	std::map<std::string, RouterInfo> routerNodes;
	std::unordered_map<std::string, ofEventListener> dropdownRouterListeners;
};

#endif /* MacroRouterManager_h */
