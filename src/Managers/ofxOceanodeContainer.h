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
    ofxOceanodeContainer(std::shared_ptr<ofxOceanodeNodeRegistry> _registry =
                         make_shared<ofxOceanodeNodeRegistry>());
    ~ofxOceanodeContainer();
    
    ofxOceanodeNode& createNode(unique_ptr<ofxOceanodeNodeModel> && nodeModel);
    
    
//    void createConnection(ofAbstractParameter& p);
    
    void createConnection(ofAbstractParameter& p, ofxOceanodeNode& n, glm::vec2 pos){
        temporalConnection = new ofxOceanodeTemporalConnection(p, n, pos);
    }
    
    ofAbstractParameter& getTemporalConnectionParameter(){return temporalConnection->getParameter();};
    
    bool isOpenConnection(){return temporalConnection != nullptr;}
    
    template<typename Tsource, typename Tsink>
    ofxOceanodeAbstractConnection* connectConnection(ofParameter<Tsource>& source, ofParameter<Tsink>& sink, glm::vec2 pos){
        ofLog()<<"cre<te Connectio";
        ofxOceanodeNode& n = temporalConnection->getNode();
        glm::vec2 posSource = temporalConnection->getSourcePosition();
        delete temporalConnection;
        temporalConnection = nullptr;
        connections.push_back(make_shared<ofxOceanodeConnection<Tsource, Tsink>>(source, sink, posSource, pos));
        n.addOutputConnection(connections.back().get());
        return connections.back().get();
    }
    
    ofxOceanodeNodeRegistry & getRegistry(){return *registry;};
    
private:
    //NodeModel;
    vector<unique_ptr<ofxOceanodeNode>> dynamicNodes;
    vector<unique_ptr<ofxOceanodeNode>> persistentNodes;

    string temporalConnectionTypeName;
    ofxOceanodeTemporalConnection*   temporalConnection;
    vector<shared_ptr<ofxOceanodeAbstractConnection>> connections;
    std::shared_ptr<ofxOceanodeNodeRegistry>   registry;
};

#endif /* ofxOceanodeContainer_h */
