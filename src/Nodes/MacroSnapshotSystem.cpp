//
//  MacroSnapshotSystem.cpp
//  ofxOceanode
//
//  Extracted from ofxOceanodeNodeMacro — Phase 2 refactoring.
//  Manages snapshot CRUD, JSON serialisation, and disk persistence.
//

#include "MacroSnapshotSystem.h"
#include "MacroRouterValueDispatch.h"
#include "ofxOceanodeShared.h"
#include "router.h"

// RouterInfo is defined in ofxOceanodeNodeMacro.h; we need the full definition
// for storeSnapshot() to iterate over router nodes.
#include "ofxOceanodeNodeMacro.h"

MacroSnapshotSystem::MacroSnapshotSystem()
	: currentSlot(-1)
{
}

// ─────────────────────────────────────────────────────────────────────────────
// Core snapshot operations
// ─────────────────────────────────────────────────────────────────────────────

void MacroSnapshotSystem::storeSnapshot(int slot,
                                         const std::map<std::string, RouterInfo>& routerNodes,
                                         float morphTimeMs, float morphBiPow)
{
	SnapshotData snapshotData;

	// Preserve existing name if the slot already exists
	auto existingSnapshot = snapshots.find(slot);
	if(existingSnapshot != snapshots.end()) {
		snapshotData.name = existingSnapshot->second.name;
	} else {
		snapshotData.name = ofToString(slot);
	}

	for(auto& routerPair : routerNodes) {
		auto& router = routerPair.second;
		if(!router.isInput) continue;

		auto& params = router.node->getParameters();
		if(!params.contains("Value")) continue; // void routers have no capturable value
		auto valueParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&params.get("Value"));
		if(!valueParam) continue;
		abstractRouter* absRouter = dynamic_cast<abstractRouter*>(&router.node->getNodeModel());
		if(absRouter == nullptr) continue;
		if(absRouter->isExcludeFromSnapshot()) continue;

		try {
			RouterSnapshot snapshot = MacroRouterValueDispatch::captureValue(valueParam);
			snapshotData.routerValues[router.routerName] = snapshot;
		} catch(const std::exception& e) {
			ofLogError("Snapshot") << "Error storing value: " << e.what();
		}
	}
	snapshotData.morphTimeMs = morphTimeMs;
	snapshotData.morphBiPow = morphBiPow;
	snapshots[slot] = snapshotData;
	currentSlot = slot;
}

void MacroSnapshotSystem::clearSnapshot(int slot) {
	auto it = snapshots.find(slot);
	if(it != snapshots.end()) {
		ofLogNotice("SnapshotSync") << "Clearing snapshot slot: " << slot;
		snapshots.erase(it);
		if(currentSlot == slot) {
			currentSlot = -1;
		}
		ofLogNotice("SnapshotSync") << "Snapshot " << slot << " cleared";
	}
}

void MacroSnapshotSystem::clearAll() {
	ofLogNotice("SnapshotSync") << "Clearing all snapshots";
	snapshots.clear();
	currentSlot = -1;
	ofLogNotice("SnapshotSync") << "All snapshots cleared";
}

void MacroSnapshotSystem::renameSnapshot(int slot, const std::string& newName) {
	auto it = snapshots.find(slot);
	if(it != snapshots.end()) {
		it->second.name = newName;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Persistence
// ─────────────────────────────────────────────────────────────────────────────

void MacroSnapshotSystem::save(bool isLocal, const std::string& macroPath,
                                const std::string& presetPath,
                                const std::string& nodeName, int numIdentifier)
{
	ofLogNotice("SnapshotSync") << "save called, snapshot count: " << snapshots.size();

	// For global macros
	if(!isLocal && !macroPath.empty()) {
		string filename = ofFilePath::removeTrailingSlash(macroPath) + "/snapshots.json";
		ofLogNotice("SnapshotSync") << "Global macro snapshot file: " << filename;

		if(snapshots.empty()) {
			// Delete the snapshots file if no snapshots exist
			if(ofFile::doesFileExist(filename)) {
				ofFile::removeFile(filename);
				ofLogNotice("SnapshotSync") << "Deleted empty snapshots file: " << filename;
			} else {
				ofLogNotice("SnapshotSync") << "No snapshots file to delete";
			}
		} else {
			// Create JSON with snapshots and save
			ofJson snapshotsJson;
			for(const auto& pair : snapshots) {
				ofJson slotJson;
				slotJson["name"] = pair.second.name;
				slotJson["morphTimeMs"] = pair.second.morphTimeMs;
				slotJson["morphBiPow"] = pair.second.morphBiPow;
				slotJson["routerValues"] = routerValuesToJson(pair.second.routerValues);
				snapshotsJson[ofToString(pair.first)] = slotJson;
			}

			ofSavePrettyJson(filename, snapshotsJson);
			ofLogNotice("SnapshotSync") << "Saved " << snapshots.size() << " snapshots to: " << filename;
		}

		// Broadcast update regardless of whether we saved or deleted
		ofxOceanodeShared::snapshotUpdated(macroPath);
	}
	// For local macros
	else if(isLocal) {
		string localPath;

		// Try to use the same path that was used to load the preset
		if(presetPath != "") {
			localPath = presetPath;
		}
		// If no existing path, use a temp folder to avoid creating a stray folder
		// (e.g. Presets/Canvas/) that would confuse the presets controller on next launch.
		else {
			localPath = "temp/" + nodeName + "_" + ofToString(numIdentifier);
		}

		string snapshotsFilePath = localPath + "/snapshots.json";
		ofLogNotice("SnapshotSync") << "Local macro snapshot file: " << snapshotsFilePath;

		if(snapshots.empty()) {
			// Delete the snapshots file if no snapshots exist
			if(ofFile::doesFileExist(snapshotsFilePath)) {
				ofFile::removeFile(snapshotsFilePath);
				ofLogNotice("SnapshotSync") << "Deleted empty local snapshots file: " << snapshotsFilePath;
			} else {
				ofLogNotice("SnapshotSync") << "No local snapshots file to delete";
			}
		} else {
			// Create JSON with snapshots and save
			ofDirectory::createDirectory(localPath, true, true);
			ofJson snapshotsJson;
			for(const auto& pair : snapshots) {
				ofJson slotJson;
				slotJson["name"] = pair.second.name;
				slotJson["morphTimeMs"] = pair.second.morphTimeMs;
				slotJson["morphBiPow"] = pair.second.morphBiPow;
				slotJson["routerValues"] = routerValuesToJson(pair.second.routerValues);
				snapshotsJson[ofToString(pair.first)] = slotJson;
			}

			ofSavePrettyJson(snapshotsFilePath, snapshotsJson);
			ofLogNotice("SnapshotSync") << "Saved " << snapshots.size() << " local snapshots to: " << snapshotsFilePath;
		}
	}
}

void MacroSnapshotSystem::loadFromPath(const std::string& path) {
	// Check if the path already ends with snapshots.json
	string snapshotsFile;
	if(ofFilePath::getFileName(path) == "snapshots.json") {
		snapshotsFile = path;
	} else {
		snapshotsFile = ofFilePath::removeTrailingSlash(path) + "/snapshots.json";
	}

	ofLogNotice("SnapshotSync") << "Loading snapshots from: " << snapshotsFile;

	if(ofFile::doesFileExist(snapshotsFile)) {
		try {
			ofJson snapshotsJson = ofLoadJson(snapshotsFile);
			snapshots.clear();
			for(const auto& item : snapshotsJson.items()) {
				int slot = ofToInt(item.key());
				SnapshotData snapshot;
				loadSnapshotFromJson(snapshot, item.value());
				snapshots[slot] = snapshot;
			}
			ofLogNotice("SnapshotSync") << "Loaded " << snapshots.size() << " snapshots";

			lastHash = calculateHash();

		} catch(const std::exception& e) {
			ofLogError("Macro") << "Error loading snapshots: " << e.what();
		}
	} else {
		ofLogWarning("SnapshotSync") << "Snapshots file does not exist: " << snapshotsFile;
	}
}

void MacroSnapshotSystem::loadFromJson(const ofJson& json) {
	snapshots.clear();
	for(const auto& item : json.items()) {
		int slot = ofToInt(item.key());
		SnapshotData snapshot;
		loadSnapshotFromJson(snapshot, item.value());
		snapshots[slot] = snapshot;
	}
}

void MacroSnapshotSystem::syncFromDisk(const std::string& macroPath) {
	if(!macroPath.empty()) {
		ofLogNotice("SnapshotSync") << "Manual sync requested for: " << macroPath;
		loadFromPath(macroPath);
	}
}

std::string MacroSnapshotSystem::calculateHash() const {
	if(snapshots.empty()) return "";

	string hashInput;
	for(const auto& pair : snapshots) {
		hashInput += ofToString(pair.first) + pair.second.name;
		// Add some representation of the router values
		for(const auto& routerPair : pair.second.routerValues) {
			hashInput += routerPair.first + routerPair.second.type;
		}
	}

	// Simple hash
	return ofToString(std::hash<string>{}(hashInput));
}

// ─────────────────────────────────────────────────────────────────────────────
// JSON helpers
// ─────────────────────────────────────────────────────────────────────────────

ofJson MacroSnapshotSystem::routerValuesToJson(const std::map<std::string, RouterSnapshot>& values) {
	ofJson json;
	for(const auto& pair : values) {
		json[pair.first] = routerSnapshotToJson(pair.second);
	}
	return json;
}

std::map<std::string, RouterSnapshot> MacroSnapshotSystem::jsonToRouterValues(const ofJson& json) {
	std::map<std::string, RouterSnapshot> values;
	for(auto it = json.begin(); it != json.end(); ++it) {
		values[it.key()] = jsonToRouterSnapshot(it.value());
	}
	return values;
}

ofJson MacroSnapshotSystem::routerSnapshotToJson(const RouterSnapshot& snapshot) {
	ofJson json;
	json["type"] = snapshot.type;
	json["value"] = snapshot.value;
	return json;
}

RouterSnapshot MacroSnapshotSystem::jsonToRouterSnapshot(const ofJson& json) {
	RouterSnapshot snapshot;
	snapshot.type = json["type"].get<string>();
	snapshot.value = json["value"];
	return snapshot;
}

// ─────────────────────────────────────────────────────────────────────────────
// Accessors
// ─────────────────────────────────────────────────────────────────────────────

const SnapshotData* MacroSnapshotSystem::getSnapshot(int slot) const {
	auto it = snapshots.find(slot);
	if(it == snapshots.end()) return nullptr;
	return &it->second;
}

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────

void MacroSnapshotSystem::loadSnapshotFromJson(SnapshotData& snapshot, const ofJson& json) {
	try {
		if(json.contains("name") && !json["name"].is_null()) {
			snapshot.name = json["name"].get<string>();
		} else {
			snapshot.name = "Snapshot";
		}

		if(json.contains("morphTimeMs") && json["morphTimeMs"].is_number()) {
			snapshot.morphTimeMs = json["morphTimeMs"].get<float>();
		} else {
			snapshot.morphTimeMs = 0.f;
		}

		if(json.contains("morphBiPow") && json["morphBiPow"].is_number()) {
			snapshot.morphBiPow = json["morphBiPow"].get<float>();
		} else {
			snapshot.morphBiPow = 0.f;
		}

		if(json.contains("routerValues") && !json["routerValues"].is_null()) {
			snapshot.routerValues = jsonToRouterValues(json["routerValues"]);
		}
	} catch(const std::exception& e) {
		ofLogError("Snapshot") << "Error parsing snapshot: " << e.what();
		throw;
	}
}
