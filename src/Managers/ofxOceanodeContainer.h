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
#ifndef OFXOCEANODE_HEADLESS
#include "ofxOceanodeNodeGui.h"
#endif

class ofxOceanodeNodeModel;
class ofxOceanodeNodeRegistry;
class ofxOceanodeTypesRegistry;
class ofxOceanodeNodeMacro;


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
		color = ofFloatColor(1, 1, 1, 1);
		textColor = ofFloatColor(0, 0, 0, 1);
        openPopupInNext = false;
	};
	
	string text;
	glm::vec2 size;
	glm::vec2 position;
	ofFloatColor color;
	ofFloatColor textColor;
	bool openPopupInNext;
    vector<ofxOceanodeNode*> nodes;
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
    
#ifndef OFXOCEANODE_HEADLESS
    vector<ofxOceanodeNode*> getSelectedModules();
    vector<ofxOceanodeNode*> getAllModules();
    ofxOceanodeNodeGui* getGuiFromModel(ofxOceanodeNodeModel* model);
    
    bool copySelectedModulesWithConnections();
    bool cutSelectedModulesWithConnections();
    bool pasteModulesAndConnectionsInPosition(glm::vec2 position, bool allowOutsideInputs);
    bool deleteSelectedModules();
    
    const vector<unique_ptr<ofxOceanodeAbstractConnection>>& getAllConnections(){return connections;};
    const std::unordered_map<string, ofxOceanodeNode*> & getParameterGroupNodesMap(){return parameterGroupNodesMap;};
	
	vector<ofxOceanodeComment> &getComments(){return comments;};
#endif
    
    void setCanvasID(string s){canvasID = s;};
    string getCanvasID(){return canvasID;};
    
private:
    
    //NodeModel;
    std::unordered_map<string, nodeContainerWithId> dynamicNodes;
    std::unordered_map<string, nodeContainerWithId> persistentNodes;
    
    std::unordered_map<string, ofxOceanodeNode*> parameterGroupNodesMap; //Maps nodes to parameterGroup.getName() reference, used in canvas

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
		void createRoutersAndReconnect(ofxOceanodeNode* macroNode, vector<ExternalConnection>& connections);
		string generateRouterName(const string& paramName, map<string, int>& nameCounters);
	string extractNodeType(const string& nodeName);
	void ensureMacroParametersExposed(const vector<ofxOceanodeNode*>& macroNodes);

		string mapParameterTypeToRouterName(const string& paramType);
		ofxOceanodeNode* getNodeFromParameter(ofxOceanodeAbstractParameter& param);
		bool isNodeInList(ofxOceanodeNode* node, vector<ofxOceanodeNode*>& nodeList);
		string getParameterTypeName(ofxOceanodeAbstractParameter& param);
		ofxOceanodeAbstractParameter* findInternalParameter(const vector<ofxOceanodeNode*>& macroNodes, const InternalConnection& internalConn);
	
		
		vector<ofxOceanodeComment> comments;
	};

#endif /* ofxOceanodeContainer_h */
