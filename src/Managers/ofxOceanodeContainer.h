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
#include "ofxOceanodeNodeGui.h"

#ifdef OFXOCEANODE_USE_OSC
    #include "ofxOsc.h"
#endif

class ofxOceanodeNodeModel;
class ofxOceanodeNodeRegistry;
class ofxOceanodeTypesRegistry;

class ofxOceanodeContainer {
public:
    using nodeContainerWithId = std::unordered_map<int, unique_ptr<ofxOceanodeNode>>;
    
    ofxOceanodeContainer(std::shared_ptr<ofxOceanodeNodeRegistry> _registry =
                         make_shared<ofxOceanodeNodeRegistry>(), std::shared_ptr<ofxOceanodeTypesRegistry> _typesRegistry =
                         make_shared<ofxOceanodeTypesRegistry>(), bool _isHeadless = false);
    ~ofxOceanodeContainer();
    
    ofxOceanodeNode* createNodeFromName(string name, int identifier = -1);
    ofxOceanodeNode& createNode(unique_ptr<ofxOceanodeNodeModel> && nodeModel, int identifier = -1, bool isPersistent = false);
    
    template<typename ModelType>
    ofxOceanodeNode& createPersistentNode(){
        static_assert(std::is_base_of<ofxOceanodeNodeModel, ModelType>::value,
                      "Must pass a subclass of ofxOceanodeNodeModel to registerType");
        
        auto &node = createNode(std::make_unique<ModelType>(), -1, true);
        node.getNodeGui().setTransformationMatrix(&transformationMatrix);
        return node;
    }
    
//    void createConnection(ofAbstractParameter& p);
    
    ofxOceanodeAbstractConnection* createConnection(ofAbstractParameter& p, ofxOceanodeNode& n);
    
    ofxOceanodeAbstractConnection* disconnectConnection(ofxOceanodeAbstractConnection* c);
    
    ofAbstractParameter& getTemporalConnectionParameter(){return temporalConnection->getSourceParameter();};
    
    bool isOpenConnection(){return temporalConnection != nullptr;}
    
    template<typename Tsource, typename Tsink>
    ofxOceanodeAbstractConnection* connectConnection(ofParameter<Tsource>& source, ofParameter<Tsink>& sink){
        connections.push_back(make_pair(temporalConnectionNode, make_shared<ofxOceanodeConnection<Tsource, Tsink>>(source, sink)));
        temporalConnectionNode->addOutputConnection(connections.back().second.get());
        if(!isHeadless){
            connections.back().second->setSourcePosition(temporalConnectionNode->getNodeGui().getSourceConnectionPositionFromParameter(source));
            connections.back().second->getGraphics().subscribeToDrawEvent(window);
        }
        if(temporalConnectionNode->getIsPersistent()) connections.back().second->setIsPersistent(true); 
        return connections.back().second.get();
    }
    ofxOceanodeAbstractConnection* createConnectionFromInfo(string sourceModule, string sourceParameter, string sinkModule, string sinkParameter);
    ofxOceanodeAbstractConnection* createConnectionFromCustomType(ofAbstractParameter &source, ofAbstractParameter &sink);
    
    ofxOceanodeNodeRegistry & getRegistry(){return *registry;};
    
    bool loadPreset(string presetFolderPath);
    void savePreset(string presetFolderPath);
    
    void setBpm(float _bpm);
    void setPhase(float _phase);
    void resetPhase();
    
    ofEvent<string> loadPresetEvent;
    
#ifdef OFXOCEANODE_USE_OSC
    void setupOscSender(string host, int port);
    void setupOscReceiver(int port);
    void update(ofEventArgs &args);
#endif
    
    ofParameter<glm::mat4> &getTransformationMatrix(){return transformationMatrix;};
    
private:
    void temporalConnectionDestructor();
    
    //NodeModel;
    std::unordered_map<string, nodeContainerWithId> dynamicNodes;
    std::unordered_map<string, nodeContainerWithId> persistentNodes;

    string temporalConnectionTypeName;
    ofxOceanodeNode* temporalConnectionNode;
    ofxOceanodeTemporalConnection*   temporalConnection;
    vector<pair<ofxOceanodeNode*, shared_ptr<ofxOceanodeAbstractConnection>>> connections;
    std::shared_ptr<ofxOceanodeNodeRegistry>   registry;
    std::shared_ptr<ofxOceanodeTypesRegistry>   typesRegistry;
    
    ofEventListeners destroyNodeListeners;
    ofEventListeners duplicateNodeListeners;
    ofEventListeners destroyConnectionListeners;
    
    ofEventListener updateListener;
    
    shared_ptr<ofAppBaseWindow> window;
    
    ofParameter<glm::mat4> transformationMatrix;
    float bpm;
    float phase;
    
    const bool isHeadless;
    
#ifdef OFXOCEANODE_USE_OSC
    ofxOscSender oscSender;
    ofxOscReceiver oscReceiver;
#endif
    
};

#endif /* ofxOceanodeContainer_h */
