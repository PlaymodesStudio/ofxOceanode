//
//  ofxOceanodeNode.h
//  example-basic
//
//  Created by Eduard Frigola on 22/06/2017.
//
//

#ifndef ofxOceanodeNode_h
#define ofxOceanodeNode_h

#include "ofMain.h"
#include "ofxOceanodeNodeSnapshots.h"

class ofxOceanodeNodeGui;
class ofxOceanodeNodeModel;
class ofxOceanodeContainer;
class ofxOceanodeAbstractParameter;

class ofxOceanodeNode {
public:
    ofxOceanodeNode(std::unique_ptr<ofxOceanodeNodeModel> && _nodeModel);
    ~ofxOceanodeNode();

    // Changes whenever a node is created or destroyed in any container, so
    // per-frame scans of the whole graph can run only after a change.
    static std::uint64_t graphGeneration();
    
	void setup(string additionalInfo);
    void update(ofEventArgs &e);
    void draw(ofEventArgs &e);
    
    void setGui(std::unique_ptr<ofxOceanodeNodeGui>&& gui);
    ofxOceanodeNodeGui& getNodeGui();
    ofxOceanodeNodeModel& getNodeModel();
    
    ofColor getColor();
    
    void deleteSelf();
    
    bool loadPreset(string presetFolderPath);
    void savePreset(string presetFolderPath, bool saveBeforeConnectionValues = false);
    
    bool loadPersistentPreset(string presetFolderPath);
    void savePersistentPreset(string presetFolderPath);
    
    void presetWillBeLoaded();
    void presetHasLoaded();
    void loadPresetBeforeConnections(string presetFolderPath);
    
    void activateConnections();
    void deactivateConnections();
    
    void deserializeParameter(ofJson &json, ofxOceanodeAbstractParameter &p, bool persistentPreset = false);
    
    bool loadConfig(string filename, bool persistentPreset = false);
    void saveConfig(string filename, bool persistentPreset = false);
    
    ofJson saveParametersToJson(bool persistentPreset = false, bool saveBeforeConnectionValues = false);
    void saveParameterToJson(ofJson &json, ofxOceanodeAbstractParameter &parameter,
                             bool persistentPreset = false, bool saveBeforeConnectionValues = false);
    bool loadParametersFromJson(ofJson json, bool persistentPreset = false);
    
    void saveInspectorParametersToJson(ofJson &json);
    void loadInspectorParametersFromJson(ofJson json);
    
    void setBpm(float bpm);
    void resetPhase();
    
    bool getIsPersistent(){return isPersistent;};
    void setIsPersistent(bool p){isPersistent = p;};
    
    void setActive(bool act);
    bool getActive(){return active;};
    
    ofEvent<void> deleteModule;
    
    ofParameterGroup& getParameters();
    ofParameterGroup& getInspectorParameters();

    // Per-parameter choices made in the Inspector. Hidden rows stay drawn
    // while connected, so no cable loses its pin.
    bool isParameterHidden(ofxOceanodeAbstractParameter& p) const;
    bool isParameterShownInGui(ofxOceanodeAbstractParameter& p) const;
    void setParameterHidden(ofxOceanodeAbstractParameter& p, bool hidden);
    // Whether a value of this type can be stored in a Node Snapshot.
    static bool canSnapshotParameter(ofxOceanodeAbstractParameter& p);
    bool isParameterInSnapshots(ofxOceanodeAbstractParameter& p) const;
    void setParameterInSnapshots(ofxOceanodeAbstractParameter& p, bool included);
    // Shown as a scope when the node is folded (and in the Inspector).
    bool isParameterScopedWhenFolded(ofxOceanodeAbstractParameter& p) const;
    void setParameterScopedWhenFolded(ofxOceanodeAbstractParameter& p, bool scoped);

    // Node Snapshots: stored parameter values, recalled with a glide.
    bool supportsSnapshots();
    ofxOceanodeNodeSnapshots& getSnapshots(){return *snapshots;}
    void storeSnapshot(int slot);
    void recallSnapshot(int slot);
    void removeSnapshot(int slot);

    // Settings saved beside the parameter values: snapshots and the
    // per-parameter choices. A missing key restores the defaults.
    void saveNodeSettings(ofJson& json);
    void loadNodeSettings(const ofJson& json);
    static const std::vector<std::string>& nodeSettingsKeys();

private:
    void updateSnapshotGlide();

    friend class ofxOceanodeContainer;
    // Declared before the model so local-macro backing files survive teardown.
    std::shared_ptr<void> canvasHistoryStorage;
    std::unique_ptr<ofxOceanodeNodeModel> nodeModel;
    std::unique_ptr<ofxOceanodeNodeGui> nodeGui;
	
    ofEventListeners nodeModelListeners;

    std::set<std::string> hiddenParameters;
    std::map<std::string, bool> snapshotOverrides;
    std::map<std::string, bool> scopeOverrides;
    std::unique_ptr<ofxOceanodeNodeSnapshots> snapshots;
    
    bool isPersistent;
    bool active;
};

#endif /* ofxOceanodeNode_h */
