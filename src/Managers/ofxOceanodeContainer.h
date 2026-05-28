//
//  ofxOceanodeContainer.h
//  example-basic
//
//  Created by Eduard Frigola on 19/06/2017.
//
//

#ifndef ofxOceanodeContainer_h
#define ofxOceanodeContainer_h

#include "ofxOceanodeConnection.h"
#include "ofxOceanodeNode.h"
#include "CustomGui/ofxOceanodeCustomGuiLayout.h"
#include "ofxOceanodeNodeGui.h"
#include <unordered_set>

class ofxOceanodeNodeModel;
class ofxOceanodeNodeRegistry;
class ofxOceanodeTypesRegistry;
class ofxOceanodeNodeMacro;
class ofxOceanodeCustomGuiPanel;


#ifdef OFXOCEANODE_USE_OSC
#include "ofxOsc.h"
#endif

#ifdef OFXOCEANODE_USE_MIDI
class ofxOceanodeAbstractMidiBinding;
class ofxMidiIn;
class ofxMidiOut;
class ofxMidiListener;
#endif

class ofxOceanodeComment{
public:
	ofxOceanodeComment(glm::vec2 _pos = glm::vec2(0, 0), glm::vec2 _size = glm::vec2(265, 20)) : position(_pos), size(_size){
		text = "-";
		color = ofFloatColor(0, 0, 0, 1);
		textColor = ofFloatColor(1, 1, 1, 1);
        openPopupInNext = false;
		selected = false;
	};
	
	string text;
	glm::vec2 size;
	glm::vec2 position;
	ofFloatColor color;
	ofFloatColor textColor;
	bool openPopupInNext;
	bool selected;
    vector<ofxOceanodeNode*> nodes;
	
	ofRectangle getRectangle() const {
		return ofRectangle(position, size.x, size.y);
	}
};


class ofxOceanodeContainer {
public:
    using nodeContainerWithId = std::unordered_map<int, shared_ptr<ofxOceanodeNode>>;
    
    ofxOceanodeContainer(std::shared_ptr<ofxOceanodeNodeRegistry> _registry = nullptr, std::shared_ptr<ofxOceanodeTypesRegistry> _typesRegistry = nullptr);
    ~ofxOceanodeContainer();
    
    void clearContainer();
    
    void update();
    void draw();
    
    void activate();
    void deactivate();

    
    ofxOceanodeNode* createNodeFromName(string name, int identifier = -1, bool isPersistent = false);
    ofxOceanodeNode& createNode(unique_ptr<ofxOceanodeNodeModel> && nodeModel, int identifier = -1, bool isPersistent = false, string additionalInfo = "");
	ofxOceanodeNode& createNode(unique_ptr<ofxOceanodeNodeModel> && nodeModel, string additionalInfo){return createNode(std::move(nodeModel), -1, false, additionalInfo);}
    
    template<typename ModelType>
    ofxOceanodeNode& createPersistentNode(){
        static_assert(std::is_base_of<ofxOceanodeNodeModel, ModelType>::value,
                      "Must pass a subclass of ofxOceanodeNodeModel to registerType");
        
        auto &node = createNode(std::make_unique<ModelType>(), -1, true);
        node.getNodeGui().setTransformationMatrix(&transformationMatrix);
        return node;
    }
    
    template<typename Tsource, typename Tsink>
    ofxOceanodeAbstractConnection* connectConnection(ofxOceanodeParameter<Tsource>& source, ofxOceanodeParameter<Tsink>& sink, bool active = true){
        connections.push_back(move(make_unique<ofxOceanodeConnection<Tsource, Tsink>>(source, sink, active)));
        auto connectionRef = connections.back().get();
        destroyConnectionListeners.push(connectionRef->destroyConnection.newListener([this, connectionRef](){
            connections.erase(std::find_if(connections.begin(), connections.end(), [connectionRef](std::unique_ptr<ofxOceanodeAbstractConnection> const &c){return c.get() == connectionRef;}));
        }));
        return connectionRef;
    }
    
    template<typename T>
    ofxOceanodeAbstractConnection* connectCustomConnection(ofxOceanodeParameter<T>& source, ofxOceanodeAbstractParameter &sink, bool active = true){
        if(!sink.receiveParameter(&source.getParameter())) return nullptr;
        connections.push_back(move(make_unique<ofxOceanodeCustomConnection<T>>(source, sink, active)));
        auto connectionRef = connections.back().get();
        destroyConnectionListeners.push(connectionRef->destroyConnection.newListener([this, connectionRef](){
            connections.erase(std::find_if(connections.begin(), connections.end(), [connectionRef](std::unique_ptr<ofxOceanodeAbstractConnection> const &c){return c.get() == connectionRef;}));
        }));
        return connectionRef;
    }
    
    ofxOceanodeAbstractConnection* createConnectionFromInfo(string sourceModule, string sourceParameter, string sinkModule, string sinkParameter, bool active = true);
    ofxOceanodeAbstractConnection* createConnection(ofxOceanodeAbstractParameter &source, ofxOceanodeAbstractParameter &sink, bool active = true);
    
    shared_ptr<ofxOceanodeNodeRegistry> getRegistry(){return registry;};
    shared_ptr<ofxOceanodeTypesRegistry> getTypesRegistry(){return typesRegistry;};
    
    bool loadPreset(string presetFolderPath);
    void savePreset(string presetFolderPath);
    
    void loadPreset_presetWillBeLoaded();
    void loadPreset_loadNodes(string presetFolderPath);
    void loadPreset_deactivateConnections();
    void loadPreset_loadBeforeConnections(string presetFolderPath);
    void loadPreset_loadConnections(string presetFolderPath);
    void loadPreset_midiBindings(string presetFolderPath);
    void loadPreset_loadNodePreset(string presetFolderPath);
    void loadPreset_activateConnections();
    void loadPreset_presetHasLoaded();
    void loadPreset_loadComments(string presetFolderPath);
    
    
    bool loadClipboardModulesAndConnections(glm::vec2 referencePosition, bool allowOutsideInputs);
    void saveClipboardModulesAndConnections(vector<ofxOceanodeNode*> nodes, glm::vec2 referencePosition);
    
    void savePersistent();
    void loadPersistent();
    void updatePersistent();
    void saveCurrentPreset();
    
    void saveScope(const std::string& presetPath);
    void loadScope(const std::string& presetPath);
    void saveCustomGuis(const std::string& presetPath);
    void loadCustomGuis(const std::string& presetPath);
    void saveCustomGuis();
    void markCustomGuisDirty();
    void saveCustomGuiSnapshots(const std::string& presetPath);
    void loadCustomGuiSnapshots(const std::string& presetPath);
    void saveCustomGuiSnapshots();
    void markCustomGuiSnapshotsDirty();
    
    void setBpm(float _bpm);
    void resetPhase();
	
	// Node encapsulation functionality
	void encapsulateSelectedNodes(const string& macroName = "Encapsulated");

    
    ofEvent<pair<string, string>> loadPresetEvent;
    ofEvent<pair<string, int>> loadPresetNumEvent;
    ofEvent<float> changedBpmEvent;
    ofEvent<void> saveCurrentPresetEvent;
    ofEvent<ofxOceanodeNode*> newNodeCreated;
    ofEvent<void> allNodesCreated;
    
#ifdef OFXOCEANODE_USE_OSC
    void receiveOscMessage(ofxOscMessage &m);
#endif
    
#ifdef OFXOCEANODE_USE_MIDI
    void setIsListeningMidi(bool b);
    shared_ptr<ofxOceanodeAbstractMidiBinding> createMidiBinding(ofxOceanodeAbstractParameter &p, bool isPersistent = false, int _id = -1);
    bool removeLastMidiBinding(ofxOceanodeAbstractParameter &p);
    shared_ptr<ofxOceanodeAbstractMidiBinding> createMidiBindingFromInfo(string module, string parameter, bool isPersistent = false, int _id = -1);
    vector<string> getMidiDevices(){return midiInPortList;};
    void addNewMidiMessageListener(ofxMidiListener* listener);
    
    map<string, vector<shared_ptr<ofxOceanodeAbstractMidiBinding>>>& getMidiBindings(){return midiBindings;};
    map<string, vector<shared_ptr<ofxOceanodeAbstractMidiBinding>>>& getPersistentMidiBindings(){return persistentMidiBindings;};
#endif
    
    ofParameter<glm::mat4> &getTransformationMatrix(){return transformationMatrix;};
    
    vector<ofxOceanodeNode*> getSelectedModules();
    vector<ofxOceanodeNode*> getAllModules();
    ofxOceanodeNodeGui* getGuiFromModel(ofxOceanodeNodeModel* model);

    const std::vector<CustomGuiPanelData>& getCustomGuiPanelsData() const { return customGuiPanelsData; }
    CustomGuiPanelData* getCustomGuiPanelData(const std::string& panelId);
    const CustomGuiPanelData* getCustomGuiPanelData(const std::string& panelId) const;
    CustomGuiPanelData& createCustomGuiPanel(const std::string& requestedName = "");
    bool deleteCustomGuiPanel(const std::string& panelId);
    void requestDeleteCustomGuiPanel(const std::string& panelId);
    void openCustomGuiPanel(const std::string& panelId, bool designMode = false);
    bool renameCustomGuiPanel(const std::string& panelId, const std::string& requestedName);
    bool customGuiPanelHasSnapshotEligibleParameters(const std::string& panelId) const;
    bool addParameterToCustomGui(const std::string& panelId, ofxOceanodeAbstractParameter& parameter, CustomGuiWidgetType type);
    bool removeParameterFromCustomGui(const std::string& panelId, ofxOceanodeAbstractParameter& parameter);
    bool customGuiContainsParameter(const std::string& panelId, ofxOceanodeAbstractParameter& parameter) const;
    bool customGuiContainsParameterAnywhere(ofxOceanodeAbstractParameter& parameter) const;
    std::vector<CustomGuiWidgetType> getCompatibleCustomGuiWidgetTypes(ofxOceanodeAbstractParameter& parameter) const;
    CustomGuiWidgetType getDefaultCustomGuiWidgetType(ofxOceanodeAbstractParameter& parameter) const;
    std::string getCustomGuiParameterPath(ofxOceanodeAbstractParameter& parameter) const;
    ofxOceanodeAbstractParameter* findCustomGuiParameter(const std::string& parameterPath) const;
    ofxOceanodeNode* getNodeFromParameter(ofxOceanodeAbstractParameter& param);
    bool showNodeInCanvas(ofxOceanodeNode& node);
    bool showParameterInCanvas(ofxOceanodeAbstractParameter& parameter);
    ofxOceanodeContainer* getContainerForCanvasID(const std::string& canvasID);
    const ofxOceanodeContainer* getContainerForCanvasID(const std::string& canvasID) const;
    CustomGuiSnapshotBank* getCustomGuiSnapshotBank(const std::string& panelId);
    const CustomGuiSnapshotBank* getCustomGuiSnapshotBank(const std::string& panelId) const;
    std::string createCustomGuiSnapshot(const std::string& panelId, const std::string& requestedName = "");
    bool updateCustomGuiSnapshot(const std::string& panelId, const std::string& snapshotId);
    bool recallCustomGuiSnapshot(const std::string& panelId, const std::string& snapshotId);
    bool renameCustomGuiSnapshot(const std::string& panelId, const std::string& snapshotId, const std::string& requestedName);
    bool deleteCustomGuiSnapshot(const std::string& panelId, const std::string& snapshotId);
    CustomGuiSnapshotData* getCustomGuiSnapshotBySlot(const std::string& panelId, int slot);
    const CustomGuiSnapshotData* getCustomGuiSnapshotBySlot(const std::string& panelId, int slot) const;
    std::string storeCustomGuiSnapshotToSlot(const std::string& panelId, int slot, const std::string& requestedName = "");
    bool recallCustomGuiSnapshotSlot(const std::string& panelId, int slot);
    void requestCreateCustomGui(const std::string& parameterPath = "", CustomGuiWidgetType type = CustomGuiWidgetType::Slider, bool openInEdit = true);
    void drawCustomGuiCreationModal();
    
    bool copySelectedModulesWithConnections();
    bool cutSelectedModulesWithConnections();
    bool pasteModulesAndConnectionsInPosition(glm::vec2 position, bool allowOutsideInputs);
    bool deleteSelectedModules();
    
    const vector<unique_ptr<ofxOceanodeAbstractConnection>>& getAllConnections(){return connections;};
    const std::unordered_map<string, ofxOceanodeNode*> & getParameterGroupNodesMap(){return parameterGroupNodesMap;};
	
    vector<ofxOceanodeComment> &getComments(){return comments;};
	vector<int> getSelectedCommentIndices();
	void deselectAllComments();
    
    void setCanvasID(string s){canvasID = s;};
    string getCanvasID(){return canvasID;};

    /// Read-only access to the dynamic node map, keyed by node-type name.
    /// Each value is a map from integer identifier to the shared_ptr<ofxOceanodeNode>.
    const std::unordered_map<string, nodeContainerWithId>& getDynamicNodes() const { return dynamicNodes; }
    
private:
    struct ResolvedParameter {
        ofxOceanodeAbstractParameter* parameter;
        ofxOceanodeNode* node;
        
        ResolvedParameter() : parameter(nullptr), node(nullptr) {}
        ResolvedParameter(ofxOceanodeAbstractParameter* p, ofxOceanodeNode* n) : parameter(p), node(n) {}
    };
    
    ResolvedParameter resolveParameterFromPath(const std::string& paramPath, const std::string& canvasID = "");
    
    struct ParsedParameterPath {
        std::string groupName;
        std::string paramName;
        bool isValid;
    };
	
    ParsedParameterPath parseParameterPath(const std::string& path);
    void invalidateCustomGuiMembershipIndex();
    void invalidateCustomGuiParameterPathCache();
    void rebuildCustomGuiMembershipIndexIfNeeded() const;
    void rebuildCustomGuiPanels();
    std::string getCustomGuiFilePath(const std::string& presetPath) const;
    std::string getCustomGuiSnapshotsFilePath(const std::string& presetPath) const;
    std::string makeUniqueCustomGuiName(const std::string& baseName = "Custom GUI") const;
    std::string makeCustomGuiId() const;
    std::string makeCustomGuiSnapshotId() const;
    std::string makeUniqueCustomGuiSnapshotName(const CustomGuiSnapshotBank& bank, const std::string& baseName = "Snapshot") const;
    CustomGuiSnapshotBank* getOrCreateCustomGuiSnapshotBank(const std::string& panelId);
    int getNextAvailableCustomGuiSnapshotSlot(const CustomGuiSnapshotBank& bank) const;
    
    //NodeModel;
    std::unordered_map<string, nodeContainerWithId> dynamicNodes;
    std::unordered_map<string, nodeContainerWithId> persistentNodes;
    
    std::unordered_map<string, ofxOceanodeNode*> parameterGroupNodesMap; //Maps nodes to parameterGroup.getName() reference, used in canvas

    std::vector<CustomGuiPanelData> customGuiPanelsData;
    std::vector<std::unique_ptr<ofxOceanodeCustomGuiPanel>> customGuiPanels;
    std::vector<CustomGuiSnapshotBank> customGuiSnapshotBanks;
    mutable std::unordered_map<const ofxOceanodeAbstractParameter*, std::string> customGuiParameterPathCache;
    mutable std::unordered_map<std::string, std::unordered_set<std::string>> customGuiPanelParameterIndex;
    mutable std::unordered_set<std::string> customGuiPublishedParameterIndex;
    mutable bool customGuiMembershipIndexDirty = true;
    std::string customGuiStoragePath;
    bool customGuisDirty = false;
    bool customGuiSnapshotsDirty = false;
    bool customGuiCreateModalOpen = false;
    std::string pendingCustomGuiName = "Custom GUI";
    std::string pendingDeletedCustomGuiPanelId;
    std::string pendingCustomGuiParameterPath;
    CustomGuiWidgetType pendingCustomGuiWidgetType = CustomGuiWidgetType::Slider;
    bool pendingCustomGuiOpenInEdit = true;

    vector<unique_ptr<ofxOceanodeAbstractConnection>> connections;
    std::shared_ptr<ofxOceanodeNodeRegistry>   registry;
    std::shared_ptr<ofxOceanodeTypesRegistry>   typesRegistry;
    
    ofEventListeners destroyNodeListeners;
    ofEventListeners destroyConnectionListeners;
    
    ofParameter<glm::mat4> transformationMatrix;
    float bpm;
    float phase;
    
#ifdef OFXOCEANODE_USE_MIDI
    bool isListeningMidi;
    map<string, vector<shared_ptr<ofxOceanodeAbstractMidiBinding>>> midiBindings;
    map<string, vector<shared_ptr<ofxOceanodeAbstractMidiBinding>>> persistentMidiBindings;
    map<string, ofxMidiIn> midiIns;
    map<string, ofxMidiOut> midiOuts;
    
    vector<string> midiInPortList;
    vector<string> midiOutPortList;
    
    ofEventListeners midiUnregisterlisteners;
    ofEventListeners midiSenderListeners;
    void midiBindingBound(const void * sender, string &portName);
#endif
    string canvasID;
	
	// Encapsulation support structures and methods
		struct InternalConnection {
			string nodeName;    // Name of the internal node
			string paramName;   // Name of the internal parameter
		};
		
		struct ExternalConnection {
			// For input routers (external → multiple internals)
			ofxOceanodeAbstractParameter* externalParam;  // Single external parameter (for inputs)
			vector<InternalConnection> internalConnections; // Multiple internal connections (for inputs)
			
			// For output routers (single internal → multiple externals)
			InternalConnection internalConnection;  // Single internal connection (for outputs)
			vector<ofxOceanodeAbstractParameter*> externalConnections; // Multiple external parameters (for outputs)
			
			bool isIncoming;           // true: external->internal, false: internal->external
			string routerName;         // Generated name for router
			string routerType;         // Parameter type for router creation
		};
			
		vector<ExternalConnection> analyzeExternalConnections(vector<ofxOceanodeNode*> selectedNodes);
		void createRoutersAndReconnect(ofxOceanodeNode* macroNode, vector<ExternalConnection>& connections, glm::vec2 minPosition = glm::vec2(0, 0));
		string generateRouterName(const string& paramName, map<string, int>& nameCounters);
	string extractNodeType(const string& nodeName);
	void ensureMacroParametersExposed(const vector<ofxOceanodeNode*>& macroNodes);

		string mapParameterTypeToRouterName(const string& paramType);
		bool isNodeInList(ofxOceanodeNode* node, vector<ofxOceanodeNode*>& nodeList);
		string getParameterTypeName(ofxOceanodeAbstractParameter& param);
		ofxOceanodeAbstractParameter* findInternalParameter(const vector<ofxOceanodeNode*>& macroNodes, const InternalConnection& internalConn);
	
		
		vector<ofxOceanodeComment> comments;
	};

#endif /* ofxOceanodeContainer_h */
