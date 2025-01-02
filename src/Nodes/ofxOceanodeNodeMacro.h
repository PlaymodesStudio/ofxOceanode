//
//  ofxOceanodeNodeMacro.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 20/06/2019.
//

#ifndef ofxOceanodeNodeMacro_h
#define ofxOceanodeNodeMacro_h

#include "ofxOceanode.h"
#include "ofxOceanodeShared.h"
#include "router.h"

class ofxOscMessage;

// Data Structures
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

class ofxOceanodeNodeMacro : public ofxOceanodeNodeModel {
public:
    ofxOceanodeNodeMacro();
    ~ofxOceanodeNodeMacro(){};

    // Core Methods
    void setup() override { setup(""); }
    void setup(string additionalInfo) override;
    void update(ofEventArgs &a) override;
    void draw(ofEventArgs &a) override;

    // Container Methods
    void setContainer(ofxOceanodeContainer* container) override;
    shared_ptr<ofxOceanodeContainer> getContainer() { return container; }
    ofxOceanodeCanvas* getCanvas() { return &canvas; }

    // State Management
    void activateWindow() { showWindow = true; }
    void activate() override { active = true; }
    void deactivate() override { active = false; }
    string getCurrentMacroName() { return currentMacro; }
    bool isLocal() { return localPreset; }

    // Preset Methods
    void macroSave(ofJson &json, string path) override;
    void macroLoad(ofJson &json, string path) override;
    void presetRecallBeforeSettingParameters(ofJson &json) override;
    void presetRecallAfterSettingParameters(ofJson &json) override;
    void presetWillBeLoaded() override;
    void presetHasLoaded() override;

    // Connection Management
    void loadBeforeConnections(ofJson &json) override;
    void activateConnections() override;
    void deactivateConnections() override;

    // Utility Methods
    void setBpm(float bpm) override { container->setBpm(bpm); }
    void resetPhase() override { container->resetPhase(); }
    bool receiveOscMessage(ofxOscMessage &m) override { container->receiveOscMessage(m); return true; }

protected:
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

    // Node Management
    void newNodeCreated(ofxOceanodeNode* &node);
    void loadMacroInsideCategory(int newPresetIndex);
    void updateCurrentCategoryFromPath(string path);

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

    // Router Value Management
    std::map<string, RouterSnapshot> captureRouterValues();
    void applyRouterValues(const std::map<string, RouterSnapshot>& values);
    ofJson routerValuesToJson(const std::map<string, RouterSnapshot>& values);
    std::map<string, RouterSnapshot> jsonToRouterValues(const ofJson& json);
    ofJson routerSnapshotToJson(const RouterSnapshot& snapshot);
    RouterSnapshot jsonToRouterSnapshot(const ofJson& json);

    // Event Handlers
    void onMatrixSizeChanged(int& value);

    // Core Members
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
    std::unordered_map<string, ofEventListeners> inoutListeners;

    // Parameters
    ofParameter<bool> active;
    ofParameter<int> bank;
    ofParameter<int> preset;
    ofParameter<string> savePresetField;
    ofParameter<string> presetName;
    ofParameter<bool> savePreset;
    ofParameter<ofColor> colorParam;
    ofParameter<bool> resetPhaseOnActive;
    ofParameter<string> localName;
    ofParameter<std::function<void()>> presetControl;
    ofParameter<std::function<void()>> snapshotInspector;
    ofParameter<int> matrixRows;
    ofParameter<int> matrixCols;
    ofParameter<bool> showSnapshotNames;
    ofParameter<void> addSnapshotButton;
    ofParameter<int> activeSnapshotSlot;
    ofParameter<float> buttonSize;

    // State Variables
    bool showWindow;
    bool showSnapshotMatrix;
    bool localPreset;
    bool lastActiveState;
    string presetPath;
    string nextPresetPath;
    string currentMacro;
    string currentMacroPath;
    string canvasParentID;
    int previousBank;
    int currentPreset;
    int currentSnapshotSlot;

    // Collections
    deque<string> currentCategory;
    deque<string> saveAsTempCategory;
    vector<string> bankNames;
    vector<string> presetsInBank;
    std::map<std::string, RouterInfo> routerNodes;
    std::map<int, SnapshotData> snapshots;

    // Additional Components
    shared_ptr<ofxOceanodeParameter<int>> bankDropdown;
    shared_ptr<ofxOceanodeParameter<int>> presetDropdown;
    shared_ptr<macroCategory> currentCategoryMacro;
    std::function<void(ImVec2)> minimizedViewCallback;
};

#endif /* ofxOceanodeNodeMacro_h */
