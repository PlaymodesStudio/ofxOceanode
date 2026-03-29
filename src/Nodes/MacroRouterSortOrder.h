//
//  MacroRouterSortOrder.h
//  ofxOceanode
//
//  Extracted from ofxOceanodeNodeMacro — Phase 1 refactoring.
//  Pure data + helper class for router sort-order management (no GUI).
//

#ifndef MacroRouterSortOrder_h
#define MacroRouterSortOrder_h

#include <string>
#include <vector>
#include "ofColor.h"
#include "ofJson.h"

class MacroRouterSortOrder {
public:
	// Separator encoding/decoding
	bool isSortSeparatorEntry(const std::string& entry) const;
	std::string getSortSeparatorLabel(const std::string& entry) const;
	ofColor getSortSeparatorColor(const std::string& entry) const;
	std::string makeSortSeparatorEntry(const std::string& label, const ofColor& color) const;

	// Order queries
	bool isRouterInSortOrder(const std::string& routerName) const;

	// Persistence
	void loadRouterSortFromJson(const ofJson& json);

	// Accessors — non-const refs so the owning class can mutate directly
	std::vector<std::string>& getOrder() { return routerSortOrder; }
	const std::vector<std::string>& getOrder() const { return routerSortOrder; }
	std::vector<std::string>& getPendingOrder() { return pendingRouterSortOrder; }
	const std::vector<std::string>& getPendingOrder() const { return pendingRouterSortOrder; }

private:
	std::vector<std::string> routerSortOrder;         // active order, rebuilt each load
	std::vector<std::string> pendingRouterSortOrder;   // loaded from JSON, consumed by allNodesCreated()
};

#endif /* MacroRouterSortOrder_h */
