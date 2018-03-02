//
//  ofxOceanodeNode.h
//  example-basic
//
//  Created by Eduard Frigola on 22/06/2017.
//
//

#ifndef ofxOceanodeNode_h
#define ofxOceanodeNode_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeNodeGui.h"
#include "ofxDatGui.h"
#include "ofxOceanodeConnection.h"

class ofxOceanodeNode {
public:
    ofxOceanodeNode(unique_ptr<ofxOceanodeNodeModel> && _nodeModel);
    ~ofxOceanodeNode(){};
    
    void setGui(std::unique_ptr<ofxOceanodeNodeGui>&& gui);
    
    ofxOceanodeNodeGui& getNodeGui(){return *nodeGui.get();};
    
    bool parameterConnectionPress(ofxOceanodeContainer& container, ofAbstractParameter& parameter, glm::vec2 pos);
    bool parameterConnectionRelease(ofxOceanodeContainer& container, ofAbstractParameter& parameter, glm::vec2 pos);
    
    void addOutputConnection(ofxOceanodeAbstractConnection* c);
    
    void addInputConnection(ofxOceanodeAbstractConnection* c);
    
    void moveConnections(glm::vec2 moveVector);
    
    ofParameterGroup* getParameters(){return nodeModel->getParameterGroup();};
private:
    std::unique_ptr<ofxOceanodeNodeModel> nodeModel;
    std::unique_ptr<ofxOceanodeNodeGui> nodeGui;
    
    std::vector<ofxOceanodeAbstractConnection*> inConnections;
    std::vector<ofxOceanodeAbstractConnection*> outConnections;
    
    vector<ofEventListener> inConnectionsListeners;
    vector<ofEventListener> outConnectionsListeners;
};

#endif /* ofxOceanodeNode_h */
