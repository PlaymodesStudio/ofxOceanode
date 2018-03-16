//
//  ofxOceanodeContainer.h
//  example-basic
//
//  Created by Eduard Frigola on 19/06/2017.
//
//

#ifndef ofxOceanodeContainer_h
#define ofxOceanodeContainer_h

#include "ofxOceanodeNode.h"
#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeNodeRegistry.h"
#include "ofxOceanodeConnection.h"

class ofxOceanodeContainer {
public:
    using nodeContainerWithId = std::unordered_map<int, unique_ptr<ofxOceanodeNode>>;
    
    ofxOceanodeContainer(std::shared_ptr<ofxOceanodeNodeRegistry> _registry =
                         make_shared<ofxOceanodeNodeRegistry>());
    ~ofxOceanodeContainer();
    
    ofxOceanodeNode& createNodeFromName(string name, int identifier = -1);
    ofxOceanodeNode& createNode(unique_ptr<ofxOceanodeNodeModel> && nodeModel, int identifier = -1);
    
//    void createConnection(ofAbstractParameter& p);
    
    ofxOceanodeAbstractConnection* createConnection(ofAbstractParameter& p, ofxOceanodeNode& n);
    
    ofxOceanodeAbstractConnection* disconnectConnection(ofxOceanodeAbstractConnection* c);
    
    ofAbstractParameter& getTemporalConnectionParameter(){return temporalConnection->getSourceParameter();};
    
    bool isOpenConnection(){return temporalConnection != nullptr;}
    
    template<typename Tsource, typename Tsink>
    ofxOceanodeAbstractConnection* connectConnection(ofParameter<Tsource>& source, ofParameter<Tsink>& sink){
        connections.push_back(make_pair(temporalConnectionNode, make_shared<ofxOceanodeConnection<Tsource, Tsink>>(source, sink)));
        temporalConnectionNode->addOutputConnection(connections.back().second.get());
        connections.back().second->setSourcePosition(temporalConnectionNode->getNodeGui().getSourceConnectionPositionFromParameter(source));
        connections.back().second->getGraphics().subscribeToDrawEvent(window);
        return connections.back().second.get();
    }
    ofxOceanodeAbstractConnection* createConnectionFromInfo(string sourceModule, string sourceParameter, string sinkModule, string sinkParameter);
    
    ofxOceanodeNodeRegistry & getRegistry(){return *registry;};
    
    bool loadPreset(string presetFolderPath);
    bool savePreset(string presetFolderPath);
    
    ofParameter<glm::mat4> &getTransformationMatrix(){return transformationMatrix;};
    
private:
    void temporalConnectionDestructor();
    
    //NodeModel;
    std::unordered_map<string, nodeContainerWithId> dynamicNodes;
    vector<unique_ptr<ofxOceanodeNode>> persistentNodes;

    string temporalConnectionTypeName;
    ofxOceanodeNode* temporalConnectionNode;
    ofxOceanodeTemporalConnection*   temporalConnection;
    vector<pair<ofxOceanodeNode*, shared_ptr<ofxOceanodeAbstractConnection>>> connections;
    std::shared_ptr<ofxOceanodeNodeRegistry>   registry;
    
    vector<ofEventListener> destroyNodeListeners;
    
    shared_ptr<ofAppBaseWindow> window;
    
    ofParameter<glm::mat4> transformationMatrix;
};

#endif /* ofxOceanodeContainer_h */
