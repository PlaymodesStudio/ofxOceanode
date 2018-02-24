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

class ofxOceanodeNode {
public:
    ofxOceanodeNode(unique_ptr<ofxOceanodeNodeModel> && _nodeModel);
    ~ofxOceanodeNode(){};
    
    void setGui(std::unique_ptr<ofxOceanodeNodeGui>&& gui);
    
    ofxOceanodeNodeGui& getNodeGui(){return *nodeGui.get();};
    
    void makeConnection(ofxOceanodeContainer& container, int parameterIndex);
    
    ofParameterGroup* getParameters(){return nodeModel->getParameterGroup();};
private:
    std::unique_ptr<ofxOceanodeNodeModel> nodeModel;
    std::unique_ptr<ofxOceanodeNodeGui> nodeGui;
};

#endif /* ofxOceanodeNode_h */
