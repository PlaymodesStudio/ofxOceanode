//
//  ofxOceanodeNodeMacro.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 20/06/2019.
//  Snapshot management added by Santi Vilanova on January 2025
//
//  Refactored in phases 1-8 (March 2025): extracted helper classes for
//  router management, snapshot system, morph engine, preset management,
//  router sort-order, and value dispatch.  GUI rendering lives in
//  ofxOceanodeNodeMacroGui.cpp.

#ifndef ofxOceanodeNodeMacro_h
#define ofxOceanodeNodeMacro_h

#include "ofxOceanode.h"
#include "ofxOceanodeShared.h"
#include "router.h"

#include "MacroRouterSortOrder.h"
#include "MacroRouterValueDispatch.h"
#include "MacroSnapshotSystem.h"
#include "MacroMorphEngine.h"
#include "MacroRouterManager.h"
#include "MacroPresetManager.h"

#ifdef OFXOCEANODE_USE_OSC
class ofxOscMessage;
#endif

// ─── Shared data types ────────────────────────────────────────────────────────
// RouterInfo is used by both the router manager and the snapshot system.
// RouterSnapshot and SnapshotData are defined in MacroSnapshotSystem.h.

struct RouterInfo {
	bool isInput;
	std::string routerName;
	std::string parameterType;
	ofxOceanodeNode* node;
};

// ─── GUI scratch state ────────────────────────────────────────────────────────
// Mutable buffers used by the router-sort inspector (renderRouterSortInterface).

struct MacroGuiState {
	char routerSortSepNameBuf[128] = {};
	int  routerSortEditingIndex = -1;
	bool routerSortEditNeedsFocus = false;
	char routerSortEditBuf[256] = {};
	bool showClearAllSnapshotsPopup = false;
};

// ═══════════════════════════════════════════════════════════════════════════════
// ofxOceanodeNodeMacro — encapsulates a sub-graph (a "macro")
// ═══════════════════════════════════════════════════════════════════════════════

class ofxOceanodeNodeMacro : public ofxOceanodeNodeModel{
public:
	ofxOceanodeNodeMacro();
	~ofxOceanodeNodeMacro(){};
	
	// ─── Lifecycle ────────────────────────────────────────────────────────
	void setup(){setup("");};
	void setup(string additionalInfo);
	void update(ofEventArgs &a);
	void draw(ofEventArgs &a);
	
	void setContainer(ofxOceanodeContainer* container);
	
	// ─── Preset save/load ─────────────────────────────────────────────────
	void macroSave(ofJson &json, string path);
	void macroLoad(ofJson &json, string path);
	
	void loadBeforeConnections(ofJson &json) override;
	void presetRecallBeforeSettingParameters(ofJson &json) override;
	void presetRecallAfterSettingParameters(ofJson &json) override;
	void presetWillBeLoaded() override;
	void presetHasLoaded() override;
	
	void activateConnections() override;
	void deactivateConnections() override;
	
	// ─── BPM / Phase ──────────────────────────────────────────────────────
	void setBpm(float bpm){container->setBpm(bpm);};
	void resetPhase(){container->resetPhase();};
	
	// ─── Snapshot sync ────────────────────────────────────────────────────
	void syncSnapshotsFromDisk();

#ifdef OFXOCEANODE_USE_OSC
	bool receiveOscMessage(ofxOscMessage &m) override{
		container->receiveOscMessage(m);
		return true;};
#endif
	
	// ─── Accessors ────────────────────────────────────────────────────────
	shared_ptr<ofxOceanodeContainer> getContainer() {return container;};
	ofxOceanodeCanvas* getCanvas() {return &canvas;};
	
	string getCurrentMacroName(){
		return presetManager.getCurrentMacro();
	}
	
	bool isLocal(){
		return presetManager.isLocal();
	}

	bool isActive() const {
		return active.get();
	}
	
	// ─── Activation ───────────────────────────────────────────────────────
	void activate(){
		if(active){
			container->activate();
			if(resetPhaseOnActive) container->resetPhase();
			if(retriggerSnapshotOnActive && snapshotSystem.getCurrentSlot() >= 0) {
				loadRouterSnapshot(snapshotSystem.getCurrentSlot());
			}
		}
	}
	
	void deactivate(){
		if(active){
			container->deactivate();
		}
	}
	
	void activateWindow(){
		showWindow = true;
		canvas.requestFocus();
	}
	
private:
	// ─── Node management ──────────────────────────────────────────────────
	void newNodeCreated(ofxOceanodeNode* &node);
	void allNodesCreated();
	
	// ─── Router & snapshot logic ──────────────────────────────────────────
	void processRouterNode(ofxOceanodeNode* node);
	void changeRouterType(const std::string& routerName, const std::string& newTypeName);
	void storeRouterSnapshot(int slot);
	void loadRouterSnapshot(int slot);
	void updateMacroDirectoryStructure();
	
	// ─── Initialization helpers (called from setup) ───────────────────────
	void initializeContainer(const string& additionalInfo);
	void initializeParameters();
	void initializeEventListeners();
	void setupPresetControl();
	void initializeSnapshotSystem();

	// ─── ImGui layout persistence (per-macro) ────────────────────────────
	void saveMacroLayout(const string& folderPath);
	void loadMacroLayout(const string& folderPath);

	// ─── GUI rendering (implemented in ofxOceanodeNodeMacroGui.cpp) ──────
	void renderPresetControlGui();
	void renderPresetNamingGui();
	void renderMinimizedView(ImVec2 size);
	void renderSnapshotMatrix();
	void renderInspectorInterface();
	void renderRouterSortInterface();
	void syncParameterGroupToSortOrder();

	// ─── Router sort-order inline helpers (delegate to sortOrder) ─────────
	bool isSortSeparatorEntry(const std::string& entry) const { return sortOrder.isSortSeparatorEntry(entry); }
	std::string getSortSeparatorLabel(const std::string& entry) const { return sortOrder.getSortSeparatorLabel(entry); }
	ofColor getSortSeparatorColor(const std::string& entry) const { return sortOrder.getSortSeparatorColor(entry); }
	std::string makeSortSeparatorEntry(const std::string& label, const ofColor& color) const { return sortOrder.makeSortSeparatorEntry(label, color); }
	bool isRouterInSortOrder(const std::string& routerName) const { return sortOrder.isRouterInSortOrder(routerName); }
	void loadRouterSortFromJson(const ofJson& json) { sortOrder.loadRouterSortFromJson(json); }

	// ─── Interpolation inline helper ──────────────────────────────────────
	bool shouldInterpolateType(const std::string& type) const { return MacroRouterValueDispatch::shouldInterpolate(type); }

	// ─── Event handlers ───────────────────────────────────────────────────
	void onMatrixSizeChanged(int& value);

	// ═════════════════════════════════════════════════════════════════════
	// Member variables
	// ═════════════════════════════════════════════════════════════════════

	// ─── Sub-systems ──────────────────────────────────────────────────────
	MacroRouterSortOrder sortOrder;
	MacroSnapshotSystem  snapshotSystem;
	MacroMorphEngine     morphEngine;
	MacroRouterManager   routerManager;
	MacroPresetManager   presetManager;

	// ─── GUI state ────────────────────────────────────────────────────────
	MacroGuiState guiState;
	std::function<void(ImVec2)> minimizedViewCallback;

	// ─── Core containers ──────────────────────────────────────────────────
#ifndef OFXOCEANODE_HEADLESS
	ofxOceanodeCanvas canvas;
#endif
	shared_ptr<ofxOceanodeContainer> container;
	shared_ptr<ofxOceanodeNodeRegistry> registry;
	shared_ptr<ofxOceanodeTypesRegistry> typesRegistry;
	
	// ─── Event listeners ──────────────────────────────────────────────────
	ofEventListener newNodeListener;
	ofEventListener macroUpdatedListener;
	ofEventListener activeListener;
	ofEventListener colorListener;
	ofEventListener addSnapshotListener;
	ofEventListener clearSnapshotsListener;
	ofEventListener matrixSizeListener;
	ofEventListener activeSnapshotSlotListener;
	ofEventListener snapshotUpdatedListener;
	ofEventListener showSnapshotMatrixListener;
	ofEventListener allNodesCreatedListener;
	ofEventListeners deleteListeners;
	ofEventListeners presetActionsListeners;
	std::unordered_map<string, ofEventListeners> inoutListeners;
	
	// ─── Basic state ──────────────────────────────────────────────────────
	bool showWindow;
	bool lastActiveState;
	bool isLoadingPreset;
	string canvasParentID;
	std::vector<ofxOceanodeNode*> toCreateRouters;

	// ─── Parameters ───────────────────────────────────────────────────────
	ofParameter<bool> active;
	ofParameter<ofColor> colorParam;
	ofParameter<bool> resetPhaseOnActive;
//	ofParameter<bool> clearContainerOnLoad;
	ofParameter<string> localName;
	
	ofParameter<std::function<void()>> presetControl;
	ofParameter<std::function<void()>> presetNaming;
	
	// ─── Snapshot parameters ──────────────────────────────────────────────
	ofParameter<std::function<void()>> snapshotInspector;
	ofParameter<int> matrixRows;
	ofParameter<int> matrixCols;
	ofParameter<bool> showSnapshotNames;
	ofParameter<void> addSnapshotButton;
	ofParameter<void> clearSnapshotsButton;
	ofParameter<int> activeSnapshotSlot;
	shared_ptr<ofxOceanodeAbstractParameter> activeSnapshotSlotParam;
	ofParameter<float> buttonSize;
	ofParameter<bool> retriggerSnapshotOnActive;
	ofParameter<bool> showSnapshotMatrix;

	// ─── Router sort-order inspector ──────────────────────────────────────
	ofParameter<std::function<void()>> routerSortInspector;
};

#endif /* ofxOceanodeNodeMacro_h */
