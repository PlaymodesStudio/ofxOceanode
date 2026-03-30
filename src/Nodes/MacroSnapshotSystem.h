//
//  MacroSnapshotSystem.h
//  ofxOceanode
//
//  Extracted from ofxOceanodeNodeMacro — Phase 2 refactoring.
//  Manages snapshot CRUD, JSON serialisation, and disk persistence.
//

#ifndef MacroSnapshotSystem_h
#define MacroSnapshotSystem_h

#include <string>
#include <map>
#include "ofJson.h"

// Forward declaration — full definition lives in ofxOceanodeNodeMacro.h
struct RouterInfo;

// ── Snapshot data structures ────────────────────────────────────────────────

struct RouterSnapshot {
	std::string type;
	ofJson value;
};

struct SnapshotData {
	std::string name;
	std::map<std::string, RouterSnapshot> routerValues;
	float morphTimeMs = 0.f;
	float morphBiPow = 0.f;
};

// ── MacroSnapshotSystem ─────────────────────────────────────────────────────

class MacroSnapshotSystem {
public:
	MacroSnapshotSystem();

	// Core snapshot operations
	void storeSnapshot(int slot, const std::map<std::string, RouterInfo>& routerNodes,
	                   float morphTimeMs, float morphBiPow);
	void clearSnapshot(int slot);
	void clearAll();
	void renameSnapshot(int slot, const std::string& newName);

	// Persistence
	void save(bool isLocal, const std::string& macroPath, const std::string& presetPath,
	          const std::string& nodeName, int numIdentifier);
	void loadFromPath(const std::string& path);
	void loadFromJson(const ofJson& json);  // backward compat for embedded snapshots in preset JSON
	void syncFromDisk(const std::string& macroPath);
	std::string calculateHash() const;

	// JSON helpers
	static ofJson routerValuesToJson(const std::map<std::string, RouterSnapshot>& values);
	static std::map<std::string, RouterSnapshot> jsonToRouterValues(const ofJson& json);
	static ofJson routerSnapshotToJson(const RouterSnapshot& snapshot);
	static RouterSnapshot jsonToRouterSnapshot(const ofJson& json);

	// Accessors
	const std::map<int, SnapshotData>& getSnapshots() const { return snapshots; }
	std::map<int, SnapshotData>& getSnapshots() { return snapshots; }
	int getCurrentSlot() const { return currentSlot; }
	void setCurrentSlot(int slot) { currentSlot = slot; }
	bool hasSnapshot(int slot) const { return snapshots.count(slot) > 0; }
	bool isEmpty() const { return snapshots.empty(); }
	int count() const { return (int)snapshots.size(); }

	// Get snapshot values for loading (used by macro for interpolation setup)
	const SnapshotData* getSnapshot(int slot) const;

private:
	void loadSnapshotFromJson(SnapshotData& snapshot, const ofJson& json);

	std::map<int, SnapshotData> snapshots;
	int currentSlot;
	std::string lastHash;
};

#endif /* MacroSnapshotSystem_h */
