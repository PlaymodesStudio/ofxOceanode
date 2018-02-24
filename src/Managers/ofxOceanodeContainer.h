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
    
    void createConnection(ofAbstractParameter& p, glm::vec2 pos){
        temporalConnection = new ofxOceanodeTemporalConnection(p, pos);
    }
    
    ofAbstractParameter& getTemporalConnectionParameter(){return temporalConnection->getParameter();};
    
    bool isOpenConnection(){return temporalConnection != nullptr;}
    
    template<typename Tsource, typename Tsink>
    void connectConnection(ofParameter<Tsource>& source, ofParameter<Tsink>& sink, glm::vec2 pos){
        glm::vec2 posSource = temporalConnection->getSourcePosition();
        delete temporalConnection;
        temporalConnection = nullptr;
        connections.push_back(make_shared<ofxOceanodeConnection<Tsource, Tsink>>(source, sink, posSource, pos));
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
