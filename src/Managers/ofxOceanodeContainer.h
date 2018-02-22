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

class ofxOceanodeContainer {
public:
    ofxOceanodeContainer(std::shared_ptr<ofxOceanodeNodeRegistry> _registry =
                         make_shared<ofxOceanodeNodeRegistry>());
    ~ofxOceanodeContainer();
    
    ofxOceanodeNode& createNode(unique_ptr<ofxOceanodeNodeModel> && nodeModel);
    
    ofxOceanodeNodeRegistry & getRegistry(){return *registry;};
    
private:
    //NodeModel;
    vector<unique_ptr<ofxOceanodeNode>> dynamicNodes;
    vector<unique_ptr<ofxOceanodeNode>> persistentNodes;

    
    std::shared_ptr<ofxOceanodeNodeRegistry>   registry;
};

#endif /* ofxOceanodeContainer_h */
