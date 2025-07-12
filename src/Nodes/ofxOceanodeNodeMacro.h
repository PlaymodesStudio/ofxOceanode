//
//  ofxOceanodeNodeMacro.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 20/06/2019.
//	Snapshot management added by Santi Vilanova on January 2025

#ifndef ofxOceanodeNodeMacro_h
#define ofxOceanodeNodeMacro_h

#include "ofxOceanode.h"
#include "ofxOceanodeShared.h"
#include "router.h"

#ifdef OFXOCEANODE_USE_OSC
class ofxOscMessage;
#endif

// Data Structures for Snapshots
struct RouterInfo {
	bool isInput;
	std::string routerName;
	std::string parameterType;
	ofxOceanodeNode* node;
};

struct RouterSnapshot {
	std::string type;
	ofJson value;
};

struct SnapshotData {
	std::string name;
	std::map<string, RouterSnapshot> routerValues;
};

class ofxOceanodeNodeMacro : public ofxOceanodeNodeModel{
public:
	ofxOceanodeNodeMacro();
	~ofxOceanodeNodeMacro(){};
	
	void setup(){setup("");};
	void setup(string additionalInfo);
	void update(ofEventArgs &a);
	void draw(ofEventArgs &a);
	
	void setContainer(ofxOceanodeContainer* container);
	
	void macroSave(ofJson &json, string path);
	void macroLoad(ofJson &json, string path);
	
	void loadBeforeConnections(ofJson &json) override;
	void presetRecallBeforeSettingParameters(ofJson &json) override;
	void presetRecallAfterSettingParameters(ofJson &json) override;
	void presetWillBeLoaded() override;
	void presetHasLoaded() override;
	
	void activateConnections() override;
	void deactivateConnections() override;
	
	void setBpm(float bpm){container->setBpm(bpm);};
	void resetPhase(){container->resetPhase();};
	
	void syncSnapshotsFromDisk();

#ifdef OFXOCEANODE_USE_OSC
	bool receiveOscMessage(ofxOscMessage &m) override{
		container->receiveOscMessage(m);
		return true;};
#endif
	
	shared_ptr<ofxOceanodeContainer> getContainer() {return container;};
	ofxOceanodeCanvas* getCanvas() {return &canvas;};
	
	void activate(){
		active = true;
	}
	
	void deactivate(){
		active = false;
	}
	
	void activateWindow(){
		showWindow = true;
	}
	
	string getCurrentMacroName(){
		return currentMacro;
	}
	
	bool isLocal(){
		return localPreset;
	}
	
private:
	// Node Management
	void newNodeCreated(ofxOceanodeNode* &node);
    void allNodesCreated();
	void loadMacroInsideCategory(int newPresetIndex);
	void updateCurrentCategoryFromPath(string path);
	
	// Initialization Methods
	void initializeContainer(const string& additionalInfo);
	void initializeParameters();
	void initializeEventListeners();
	void setupPresetControl();
	void initializeSnapshotSystem();

	// GUI Rendering Methods
	void renderMacroControls();
	void renderSnapshotsSection();
	void renderMacroSelection(bool& addBank);
	void renderBankCreationModal();
	void renderSaveControls(bool& firstSaveAsOpen);
	void renderSaveAsModal(bool firstSaveAsOpen);
	void renderSaveAsPopups(bool openNameAlreadyExistsPopup, char* cString);
	void renderChooseCategoryPopup();
	void renderSnapshotMatrix();
	void renderInspectorInterface();
	void renderSnapshotListItem(int slot, SnapshotData& snapshot);

	// Router & Snapshot Management
	void updateRouterConnections();
	void updateRouterInfo(ofxOceanodeNode* node);
	bool checkIsInputRouter(ofxOceanodeNode* node);
	void storeRouterSnapshot(int slot);
	void loadRouterSnapshot(int slot);
	void clearSnapshot(int slot);
	void renameSnapshot(int slot, const string& newName);
	void saveSnapshots();
	void loadSnapshotsFromPath(const string& path);
	void loadSnapshotFromJson(SnapshotData& snapshot, const ofJson& json);
	void updateMacroDirectoryStructure();
	void clearAllSnapshots();
	void setupSnapshotInspectorParameters();
	string calculateSnapshotHash();

	

	// Router Value Management
	std::map<string, RouterSnapshot> captureRouterValues();
	void applyRouterValues(const std::map<string, RouterSnapshot>& values);
	ofJson routerValuesToJson(const std::map<string, RouterSnapshot>& values);
	std::map<string, RouterSnapshot> jsonToRouterValues(const ofJson& json);
	ofJson routerSnapshotToJson(const RouterSnapshot& snapshot);
	RouterSnapshot jsonToRouterSnapshot(const ofJson& json);

	// Event Handlers
	void onMatrixSizeChanged(int& value);

#ifndef OFXOCEANODE_HEADLESS
	ofxOceanodeCanvas canvas;
#endif
	shared_ptr<ofxOceanodeContainer> container;
	shared_ptr<ofxOceanodeNodeRegistry> registry;
	shared_ptr<ofxOceanodeTypesRegistry> typesRegistry;
	
	// Event Listeners
	ofEventListener newNodeListener;
	ofEventListener macroUpdatedListener;
	ofEventListener activeListener;
	ofEventListener colorListener;
	ofEventListener addSnapshotListener;
	ofEventListener matrixSizeListener;
	ofEventListeners deleteListeners;
	ofEventListeners presetActionsListeners;
	ofEventListener activeSnapshotSlotListener;
	ofEventListener snapshotUpdatedListener;
		
	std::unordered_map<string, ofEventListeners> inoutListeners;
	
	// Basic Parameters
	bool showWindow;
	string presetPath;
	ofParameter<int> bank;
	int previousBank;
	shared_ptr<ofxOceanodeParameter<int>> bankDropdown;
	vector<string> bankNames;
	ofParameter<int> preset;
	shared_ptr<ofxOceanodeParameter<int>> presetDropdown;
	int currentPreset;
	ofParameter<string> savePresetField;
	vector<string> presetsInBank;
	ofParameter<string> presetName;
	ofParameter<bool> savePreset;
	
	ofParameter<ofColor> colorParam;
	ofParameter<bool> resetPhaseOnActive;
    
    ofParameter<bool> clearContainerOnLoad;
	
	bool localPreset;
	bool lastActiveState;
	string nextPresetPath;
	string currentMacro;
	string currentMacroPath;
	string canvasParentID;
	deque<string> currentCategory;
	shared_ptr<macroCategory> currentCategoryMacro;
	deque<string> saveAsTempCategory;
	ofParameter<string> localName;
    
    ofEventListener allNodesCreatedListener;
    bool isLoadingPreset;
    std::vector<ofxOceanodeNode*> toCreateRouters;
	
	ofParameter<bool> active;
	ofParameter<std::function<void()>> presetControl;
	ofParameter<std::function<void()>> presetNaming;

	// Snapshot-related Parameters
	ofParameter<std::function<void()>> snapshotInspector;
	ofParameter<int> matrixRows;
	ofParameter<int> matrixCols;
	ofParameter<bool> showSnapshotNames;
	ofParameter<void> addSnapshotButton;
	ofParameter<int> activeSnapshotSlot;
	ofParameter<float> buttonSize;
	ofParameter<bool> retriggerSnapshotOnActive; 
	bool showSnapshotMatrix;
	int currentSnapshotSlot;
	string lastSnapshotHash;
	
	// Collections
	std::map<std::string, RouterInfo> routerNodes;
	std::map<int, SnapshotData> snapshots;
	
	// Additional Components
	std::function<void(ImVec2)> minimizedViewCallback;
};

#endif /* ofxOceanodeNodeMacro_h */
