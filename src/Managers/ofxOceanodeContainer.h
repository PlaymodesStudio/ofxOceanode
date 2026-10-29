//
//  ofxOceanodeContainer.h
//  example-basic
//
//  Created by Eduard Frigola on 19/06/2017.
//
//

#ifndef ofxOceanodeContainer_h
#define ofxOceanodeContainer_h

#include "ofxOceanodeTransport.h"
#include "ofxOceanodeAutomationExtension.h"
class ofxOceanodeTimelineManager; // Optional addon; compatibility accessor is defined there.
#include "ofxOceanodeConnection.h"
#include "ofxOceanodeNode.h"
#include "ofxOceanodeCustomGuiExtension.h"
#include "ofxOceanodeNodeGui.h"
#include <cstdint>
#include <functional>
#include <map>
#include <unordered_set>

class ofxOceanodeNodeModel;
class ofxOceanodeNodeRegistry;
class ofxOceanodeTypesRegistry;
class ofxOceanodeNodeMacro;
class abstractPortal;


#ifdef OFXOCEANODE_USE_OSC
#include "ofxOsc.h"
#endif

#ifdef OFXOCEANODE_USE_MIDI
class ofxOceanodeAbstractMidiBinding;
class ofxOceanodeMidiClock;
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
    
    ofxOceanodeContainer(std::shared_ptr<ofxOceanodeNodeRegistry> _registry = nullptr, std::shared_ptr<ofxOceanodeTypesRegistry> _typesRegistry = nullptr, std::shared_ptr<ofxOceanodeTransport> _transport = nullptr);
    ~ofxOceanodeContainer();
    
    ofxOceanodeCustomGuiExtension* getCustomGuiExtension() const { return customGuiExtension.get(); }
    void setCustomGuiExtension(std::unique_ptr<ofxOceanodeCustomGuiExtension> extension) { customGuiExtension = std::move(extension); }
    void clearContainer();
    // True throughout graph deletion, including nested macro containers.
    // Addons must not traverse/rebuild graphs while their nodes are disappearing.
    static bool isClearingContainers();
    
    void update();
    void draw();
    std::shared_ptr<oceanodeProfiling::Session> getProfilingSession() const {
        return timelineHost ? timelineHost->getProfilingSession() : profilingSession;
    }
    static void drawPendingConnectionErrorDialog();
    
    void activate();
    void deactivate();

    
    ofxOceanodeNode* createNodeFromName(string name, int identifier = -1, bool isPersistent = false);
    ofxOceanodeNode& createNode(unique_ptr<ofxOceanodeNodeModel> && nodeModel, int identifier = -1, bool isPersistent = false, string additionalInfo = "");
    bool replacePortalTypeInAllInstances(abstractPortal* portal, const string& newTypeName);
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
    
    ofxOceanodeAbstractConnection* createConnectionFromInfo(string sourceModule, string sourceParameter,
                                                            string sinkModule, string sinkParameter,
                                                            bool active = true,
                                                            string* failureReason = nullptr);
    ofxOceanodeAbstractConnection* createConnection(ofxOceanodeAbstractParameter &source, ofxOceanodeAbstractParameter &sink, bool active = true);
    
    shared_ptr<ofxOceanodeNodeRegistry> getRegistry(){return registry;};
    shared_ptr<ofxOceanodeTypesRegistry> getTypesRegistry(){return typesRegistry;};
    
    bool loadPreset(string presetFolderPath);
    bool savePreset(string presetFolderPath);
    const std::string& getPresetStoragePath() const { return presetStoragePath; }
    
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
    
    bool savePersistent();
    void loadPersistent();
    void updatePersistent();
    void saveCurrentPreset();
    
    void saveScope(const std::string& presetPath);
    void loadScope(const std::string& presetPath);
    // retimeTimeline=false sets the tempo as-is (preset load: the saved
    // timeline already matches the saved tempo).
    void setBpm(float _bpm, bool retimeTimeline = true);
    void resetPhase(bool notifyTransport = true);
    std::shared_ptr<ofxOceanodeTransport> getTransport() const { return transport; }
    // The root container creates the shared transport.
    bool isTransportOwner() const { return transportOwner; }
    ofxOceanodeTransportState getTransportState() const;
    ofxOceanodeFrameTransportState getFrameTransportState() const;
    // There is a single timeline, owned by the root container. Macro
    // containers forward to their host, so every node sees the same one.
    ofxOceanodeTimelineManager& getTimelineManager();
    const ofxOceanodeTimelineManager& getTimelineManager() const;
    void setTimelineHost(ofxOceanodeContainer* host){ timelineHost = host; }
    ofxOceanodeContainer& getAutomationHost() {
        return timelineHost ? timelineHost->getAutomationHost() : *this;
    }
    ofxOceanodeAutomationExtension* getAutomationExtension() const {
        return timelineHost ? timelineHost->getAutomationExtension() : automationExtension.get();
    }
    void setAutomationExtension(std::unique_ptr<ofxOceanodeAutomationExtension> extension) {
        if(timelineHost) { timelineHost->setAutomationExtension(std::move(extension)); return; }
        automationExtension = std::move(extension);
    }

    // Timeline binding paths name the canvas too ("Macro 1 / Macro 2::Node/param"),
    // so the root timeline can reach parameters inside macros. Root-level
    // parameters keep the plain "Node/param" form.
    std::string getTimelineParameterPath(ofxOceanodeAbstractParameter& parameter) const;
    ofxOceanodeAbstractParameter* findTimelineParameter(const std::string& parameterPath) const;
	
	// Node encapsulation functionality
	void encapsulateSelectedNodes(const string& macroName = "Encapsulated");
	struct UpstreamSubgraph {
		ofxOceanodeAbstractParameter* sink = nullptr;
		vector<ofxOceanodeNode*> nodes;
	};
	// Plans every requested sink together. Nodes used by more than one branch,
	// or with any output outside their branch, stay outside all macros.
	vector<UpstreamSubgraph> findExclusiveUpstreamSubgraphs(
		const vector<ofxOceanodeAbstractParameter*>& sinks);
	ofxOceanodeNode* encapsulateNodes(const vector<ofxOceanodeNode*>& nodes,
	                                  const string& macroName = "Encapsulated");
    // Move the complete contents (including boundary routers) into this canvas.
    bool extractMacro(ofxOceanodeNode& macroNode);
    bool extractSelectedMacro();

    
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
    // Built-in transport sync to an external MIDI clock (root container only;
    // nullptr in macro containers). Settings are saved with the project.
    ofxOceanodeMidiClock* getMidiClockSync() { return midiClockSync.get(); }
    void setMidiClockSyncEnabled(bool enabled);
    bool isMidiClockSyncEnabled() const { return midiClockSyncEnabled; }
    void setMidiClockSyncPort(const std::string& port);

    void setIsListeningMidi(bool b);
    shared_ptr<ofxOceanodeAbstractMidiBinding> createMidiBinding(ofxOceanodeAbstractParameter &p, bool isPersistent = false, int _id = -1, bool keyboard = false);
    bool removeLastMidiBinding(ofxOceanodeAbstractParameter &p);
    void drawMidiKeyboardMenu(ofxOceanodeNodeModel& model);
    shared_ptr<ofxOceanodeAbstractMidiBinding> createMidiBindingFromInfo(string module, string parameter, bool isPersistent = false, int _id = -1, bool keyboard = false);
    vector<string> getMidiDevices(){return midiInPortList;};
    void addNewMidiMessageListener(ofxMidiListener* listener);
    
    map<string, vector<shared_ptr<ofxOceanodeAbstractMidiBinding>>>& getMidiBindings(){return midiBindings;};
    map<string, vector<shared_ptr<ofxOceanodeAbstractMidiBinding>>>& getPersistentMidiBindings(){return persistentMidiBindings;};
#endif
    
    ofParameter<glm::mat4> &getTransformationMatrix(){return transformationMatrix;};
    
    vector<ofxOceanodeNode*> getSelectedModules();
    vector<ofxOceanodeNode*> getAllModules();
    // Dead-node review: a plan lists connected groups of nodes that nothing live
    // consumes, and the live nodes with the reasons they are kept.
    struct UnusedNodePlan {
        struct Group {              // one connected component of dead nodes
            vector<string> nodes;   // escaped node names, sorted
            size_t connections = 0; // cables removed with the group
            bool selected = true;   // the window toggles this; removal honours it
        };
        struct Kept {
            string node;            // escaped node name
            vector<string> reasons; // sorted, unique
        };
        std::uint64_t revision = 0; // getNodesRevision() when computed
        vector<Group> groups;       // sorted by nodes
        vector<Kept> kept;          // sorted by node
    };
    UnusedNodePlan findUnusedNodes();
    // Removes the selected groups only. Recomputes the plan first and refuses
    // (returns false) when the revision or the dead set changed. Runs inside one
    // canvas edit so undo restores everything.
    bool removeUnusedNodes(const UnusedNodePlan& plan);
    // Changes whenever node ownership changes, including bulk preset/teardown clears.
    std::uint64_t getNodesRevision() const { return nodesRevision; }
    ofxOceanodeNodeGui* getGuiFromModel(ofxOceanodeNodeModel* model);

    ofxOceanodeAbstractParameter* findParameter(const std::string& parameterPath) const;
    bool customGuiContainsParameterAnywhere(ofxOceanodeAbstractParameter& parameter) const;
    ofxOceanodeNode* getNodeFromParameter(ofxOceanodeAbstractParameter& param);
    // Selects the node on its canvas and centers on it. alsoSelect adds nodes of the
    // same container to the selection, and the view then centers on the middle of the
    // box that holds all of them, at 100% zoom or the largest zoom that fits them. fitView
    // does the same for a group of one node; a lone node otherwise keeps the zoom. takeFocus false
    // leaves keyboard focus where it is, for a caller that navigates by keys.
    bool showNodeInCanvas(ofxOceanodeNode& node, const vector<ofxOceanodeNode*>& alsoSelect = {}, bool takeFocus = true,
                          bool fitView = false);
    // Selects every node on its canvas and centers on the middle of all of them.
    bool showNodesInCanvas(const vector<ofxOceanodeNode*>& nodes, bool takeFocus = true);
    bool showParameterInCanvas(ofxOceanodeAbstractParameter& parameter);
    ofxOceanodeContainer* getContainerForCanvasID(const std::string& canvasID);
    const ofxOceanodeContainer* getContainerForCanvasID(const std::string& canvasID) const;
    bool copySelectedModulesWithConnections();
    bool cutSelectedModulesWithConnections();
    bool pasteModulesAndConnectionsInPosition(glm::vec2 position, bool allowOutsideInputs);
    bool deleteSelectedModules();

    // Canvas gestures only; runtime parameter updates never enter history.
    void beginCanvasEdit();
    void endCanvasEdit();
    bool undoCanvasEdit();
    bool redoCanvasEdit();
    bool canUndoCanvasEdit() const;
    bool canRedoCanvasEdit() const;
    // UI requests run at the next root update, before any canvas is drawing.
    void requestCanvasHistory(bool redo = false);
    std::uint64_t getCanvasHistoryRevision() const;
    void clearCanvasHistory();
    
    const vector<unique_ptr<ofxOceanodeAbstractConnection>>& getAllConnections(){return connections;};
    const std::unordered_map<string, ofxOceanodeNode*> & getParameterGroupNodesMap(){return parameterGroupNodesMap;};
	
    vector<ofxOceanodeComment> &getComments(){return comments;};
    // Records value changes made while an editor control is being drawn.
    // Runtime updates outside those controls do not enter this set.
    void recordUserEditedValues(ofxOceanodeNode& node, const ofJson& before,
                                const ofJson& after, bool inspector = false);
    bool wasValueUserEdited(ofxOceanodeNode& node, const std::string& field,
                            const ofJson& current, bool inspector = false) const;
    void clearUserEditedValues();
    ofJson getCustomGuisJsonForReview() const;
    ofJson getCustomGuiSnapshotsJsonForReview() const;
    ofJson getMidiBindingsJsonForReview() const;
	vector<int> getSelectedCommentIndices();
	void deselectAllComments();
    
    void setCanvasID(string s){canvasID = s;};
    string getCanvasID(){return canvasID;};
    void setCanvasDisplayPathProvider(std::function<std::string()> provider){canvasDisplayPathProvider = std::move(provider);}
    std::string getCanvasDisplayPath() const {
        return canvasDisplayPathProvider ? canvasDisplayPathProvider() : canvasID;
    }

    /// Read-only access to the dynamic node map, keyed by node-type name.
    /// Each value is a map from integer identifier to the shared_ptr<ofxOceanodeNode>.
    const std::unordered_map<string, nodeContainerWithId>& getDynamicNodes() const { return dynamicNodes; }
    
private:
    std::shared_ptr<oceanodeProfiling::Session> profilingSession = std::make_shared<oceanodeProfiling::Session>();
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
    void scheduleScopeSave(const std::string& presetPath);
    void flushPendingScopeSave();
    struct CanvasNodeSnapshot {
        ofJson values;
        ofJson midi;
        ofJson widgets;
        std::string folder;
        ~CanvasNodeSnapshot();
    };
    struct CanvasLiveNode {
        std::uint64_t identity = 0;
        std::string setupInfo;
    };
    struct CanvasEdit {
        ofJson before;
        ofJson after;
        ofJson beforeAux, afterAux;
        ofJson nodeTransfers = ofJson::array();
        std::uint64_t sequence = 0;
        std::map<std::uint64_t, std::shared_ptr<CanvasNodeSnapshot>> snapshots;
        size_t bytes = 0;
    };
    std::unordered_map<std::string, CanvasLiveNode> canvasLiveNodes;
    std::uint64_t nextCanvasNodeIdentity = 0;
    std::unique_ptr<CanvasEdit> pendingCanvasEdit;
    std::vector<CanvasEdit> canvasUndo, canvasRedo;
    bool replayingCanvasEdit = false;
    std::uint64_t canvasHistorySerial = 0, canvasHistoryRevision = 0;
    int requestedCanvasHistory = 0;
    ofxOceanodeContainer* canvasHistoryRoot();
    void visitCanvasHistories(const std::function<void(ofxOceanodeContainer&)>& visit);
    ofxOceanodeContainer* nextCanvasHistoryOwner(bool backwards);
    void finishPendingCanvasEdits();
    void processCanvasHistoryRequest();
    ofJson captureCanvasAuxiliary() const;
    void restoreCanvasAuxiliary(const ofJson& target, const ofJson& opposite);
    void rollbackMacroCanvasEdit();
    struct MacroCanvasEditScope {
        ofxOceanodeContainer& owner;
        bool standalone, success = false;
        explicit MacroCanvasEditScope(ofxOceanodeContainer& owner);
        ~MacroCanvasEditScope();
    };
    ofJson captureCanvasGraph();
    std::shared_ptr<CanvasNodeSnapshot> captureCanvasNode(ofxOceanodeNode& node);
    void captureDeletedCanvasNode(ofxOceanodeNode& node);
    bool replayCanvasEdit(CanvasEdit& edit, bool backwards);
    bool stepCanvasHistory(bool backwards);
    void trimCanvasHistory();
    void measureCanvasEdit(CanvasEdit& edit);
    void synchronizeCanvasTransfers(CanvasEdit& edit, bool backwards);
    void retargetCanvasNodeSnapshot(CanvasNodeSnapshot& snapshot, const string& type, int from, int to);

    //NodeModel;
    std::unordered_map<string, nodeContainerWithId> dynamicNodes;
    std::unordered_map<string, nodeContainerWithId> persistentNodes;
    
    std::unordered_map<string, ofxOceanodeNode*> parameterGroupNodesMap; //Maps nodes to parameterGroup.getName() reference, used in canvas

    std::unique_ptr<ofxOceanodeCustomGuiExtension> customGuiExtension;
    std::string presetStoragePath;
    std::unordered_map<std::string, ofJson> userEditedValues;
    std::string pendingScopeSavePath;
    std::uint64_t pendingScopeSaveDeadlineMillis = 0;
    bool scopeSavePending = false;
    // Opaque addon documents survive load/save, including Save As, without the addon.
    std::map<std::string, std::string> unavailableCustomGuiFiles;
    // Missing addon nodes remain opaque, with their saved cables and parameters.
    ofJson unavailableModules, unavailableConnections;
    std::map<std::string, std::string> unavailableNodeFiles;
    std::unordered_set<std::string> unavailableNodeKeys;

    vector<unique_ptr<ofxOceanodeAbstractConnection>> connections;
    std::shared_ptr<ofxOceanodeNodeRegistry>   registry;
    std::shared_ptr<ofxOceanodeTypesRegistry>   typesRegistry;
    
    ofEventListeners destroyNodeListeners;
    std::uint64_t nodesRevision = 0;
    ofEventListeners destroyConnectionListeners;
    
    ofParameter<glm::mat4> transformationMatrix;
    float bpm;
    float phase;
    bool transportOwner = false;
    std::shared_ptr<ofxOceanodeTransport> transport;
    std::unique_ptr<ofxOceanodeAutomationExtension> automationExtension;
    ofxOceanodeContainer* timelineHost = nullptr;
    
#ifdef OFXOCEANODE_USE_MIDI
    std::unique_ptr<ofxOceanodeMidiClock> midiClockSync;
    bool midiClockSyncEnabled = false;
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
    std::function<std::string()> canvasDisplayPathProvider;
	
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
