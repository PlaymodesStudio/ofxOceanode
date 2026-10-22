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
#include "GlobalMacroSaveReview.h"

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
	float routerSortSepColor[4] = {0.78f, 0.78f, 0.78f, 1.0f}; // Default 200,200,200,255
	int  routerSortEditingIndex = -1;
	bool routerSortEditNeedsFocus = false;
	char routerSortEditBuf[256] = {};
	bool showClearAllSnapshotsPopup = false;
	bool openLoadPopup = false;
	bool loadSelectionIsPreset = false;
	std::string loadSelectionPath;
	std::string loadSelectionName;
	bool openSavePopup = false;
	bool closeSavePopup = false;
	bool saveToLibrary = false;
	bool confirmOverwrite = false;
	bool creatingSaveFolder = false;
	bool revealSaveFolder = false;
	char newFolderName[256] = {};
	char saveName[256] = {};
	std::vector<char> saveDescription;
	std::string saveError;
	std::string hoverMacroPath;
	std::string hoverMacroDescription;
	// Library Macro deletion, confirmed from the Load and Save As dialogs.
	bool openDeletePopup = false;
	bool deleteKeepsLocalCopy = false;
	std::string deleteMacroPath;
	std::string deleteMacroName;
	// Unsaved library edits, reviewed before Save, or before Load replaces them.
	bool openUnsavedPopup = false;
	bool unsavedForLoad = false;
	bool unsavedLoadIsPreset = false;
	std::string unsavedLoadPath;
	std::string unsavedLoadName;
	GlobalMacroSaveReview unsavedReview;
};

// ═══════════════════════════════════════════════════════════════════════════════
// ofxOceanodeNodeMacro — encapsulates a sub-graph (a "macro")
// ═══════════════════════════════════════════════════════════════════════════════

class ofxOceanodeNodeMacro : public ofxOceanodeNodeModel{
public:
	ofxOceanodeNodeMacro();
	~ofxOceanodeNodeMacro();
	bool drawNodeParameter(ofxOceanodeAbstractParameter& parameter, float labelWidth, float widgetWidth) override;
	std::string getNodeTitleSuffix() const override {
		return presetManager.isLocal() ? localName.get() : presetManager.getCurrentMacro();
	}
	
	// ─── Lifecycle ────────────────────────────────────────────────────────
	void setup(){setup("");};
	void setup(string additionalInfo);
	// Load a standalone Macro folder using native lifecycle, routers and snapshots.
	// The in-memory instance is local; it does not subscribe to library replacement.
	bool loadMacroSource(const std::string& folder);
	void update(ofEventArgs &a);
	void draw(ofEventArgs &a);
	
	void setContainer(ofxOceanodeContainer* container);
	
	// ─── Preset save/load ─────────────────────────────────────────────────
	void macroSave(ofJson &json, string path);
    // Capture the current contents, including unsaved library-macro edits.
    void saveCanvasUndoState(ofJson& json, const string& path);
    std::map<string, ofxOceanodeAbstractParameter*> getExposedRouterParameters() const;
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
	const string& getCurrentMacroPath() const { return presetManager.getCurrentMacroPath(); }
	const std::deque<string>& getCurrentMacroCategory() const { return presetManager.getCurrentCategory(); }
	const std::vector<string>& getRouterSortOrder() const { return sortOrder.getOrder(); }
	bool wasRouterOrderUserEdited() const { return routerOrderUserEdited; }
	void markMacroReferenceUserEdited();
	bool wasMacroReferenceUserEdited() const;
	void clearUserEditMarkers() override;
	string getLocalMacroName() const { return localName.get(); }
	string getMacroDescription() const { return macroDescription.get(); }
	bool saveGlobalDefinition(bool notifyOtherInstances = true);

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
		canvas.forceDockWindow();
		canvas.requestFocus();
	}
	bool isWindowVisible() const { return showWindow; }
	void setWindowVisible(bool visible, bool focus = true){
        if(!focus) { showWindow = visible; return; }
		if(visible) activateWindow();
		else showWindow = false;
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
	bool saveGlobalDescription(const string& folderPath) const;
	void loadGlobalDescription(const string& folderPath);
	bool saveLocalDefinition();
	bool saveLibraryDefinition(const string& path);
	bool importPresetContents(const string& path);
	// Keeps the current contents as a local Macro, detached from the library.
	bool detachLocalCopy();
	bool usesLibraryMacro(const string& path) const;
	bool deleteLibraryMacro(const string& path);
	// Deferred to update(), outside the ImGui pass.
	void requestMacroLoad(const string& path, bool isPreset);
	void requestMacroSave();
	const string& getLocalWorkingDirectory();

	// ─── GUI rendering (implemented in ofxOceanodeNodeMacroGui.cpp) ──────
	void renderPresetControlGui();
	void renderMacroLoadDialog();
	void openMacroSaveDialog(bool toLibrary);
	void renderMacroSaveDialog();
	void renderMacroSaveLibraryTree();
	bool createMacroLibraryFolder(const string& name);
	void openMacroDeleteDialog(const string& path, const string& name);
	void renderMacroDeleteDialog();
	bool reviewUnsavedChanges(bool forLoad, const string& loadPath = "", const string& loadName = "", bool loadIsPreset = false);
	void renderMacroUnsavedChangesDialog();
	void renderPresetNamingGui();
	void renderMinimizedView(ImVec2 size);
	void renderSnapshotMatrix();
	void renderInspectorInterface();
	void renderRouterSortInterface();
	void setSortSeparator(int index, const std::string& label, const ofColor& color);
	void syncParameterGroupToSortOrder();

	// ─── Router sort-order inline helpers (delegate to sortOrder) ─────────
	bool isSortSeparatorEntry(const std::string& entry) const { return sortOrder.isSortSeparatorEntry(entry); }
	std::string getSortSeparatorLabel(const std::string& entry) const { return sortOrder.getSortSeparatorLabel(entry); }
	ofColor getSortSeparatorColor(const std::string& entry) const { return sortOrder.getSortSeparatorColor(entry); }
	std::string makeSortSeparatorEntry(const std::string& label, const ofColor& color) const { return sortOrder.makeSortSeparatorEntry(label, color); }
	bool isRouterInSortOrder(const std::string& routerName) const { return sortOrder.isRouterInSortOrder(routerName); }
	void loadRouterSortFromJson(const ofJson& json) { sortOrder.loadRouterSortFromJson(json); routerOrderUserEdited = false; }

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
	std::function<void()> pendingGuiAction;
	string localWorkingDirectory;
	std::function<void(ImVec2)> minimizedViewCallback;

	// ─── Core containers ──────────────────────────────────────────────────
	ofxOceanodeCanvas canvas;
	shared_ptr<ofxOceanodeContainer> container;
	ofxOceanodeContainer* parentContainer = nullptr;
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
	bool routerOrderUserEdited = false;
	bool macroReferenceUserEdited = false;
	bool pendingMacroReferenceUserEdited = false;
	bool editedReferenceIsLocal = true;
	string editedReferenceName;
	std::deque<string> editedReferenceCategory;
	
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
	ofParameter<string> macroDescription;
	
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
	ofParameter<bool> retriggerSnapshotOnActive;
	ofParameter<bool> showSnapshotMatrix;

	// ─── Router sort-order inspector ──────────────────────────────────────
	ofParameter<std::function<void()>> routerSortInspector;
};

#endif /* ofxOceanodeNodeMacro_h */
