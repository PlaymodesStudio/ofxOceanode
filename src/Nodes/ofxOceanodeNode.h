//
//  ofxOceanodeNode.h
//  example-basic
//
//  Created by Eduard Frigola on 22/06/2017.
//
//

#ifndef ofxOceanodeNode_h
#define ofxOceanodeNode_h

#include "ofMain.h"

class ofxOceanodeAbstractConnection;
class ofxOceanodeNodeGui;
class ofxOceanodeNodeModel;
class ofxOceanodeContainer;

class ofxOceanodeNode {
public:
    ofxOceanodeNode(unique_ptr<ofxOceanodeNodeModel> && _nodeModel);
    ~ofxOceanodeNode();
    
    void setGui(std::unique_ptr<ofxOceanodeNodeGui>&& gui);
    
    ofxOceanodeNodeGui& getNodeGui();
    ofColor getColor();
    
    ofxOceanodeAbstractConnection* parameterConnectionPress(ofxOceanodeContainer& container, ofAbstractParameter& parameter);
    ofxOceanodeAbstractConnection* parameterConnectionRelease(ofxOceanodeContainer& container, ofAbstractParameter& parameter);
    
    ofxOceanodeAbstractConnection* createConnection(ofxOceanodeContainer& container, ofAbstractParameter& sourceParameter, ofAbstractParameter& sinkParameter);
    
    void addOutputConnection(ofxOceanodeAbstractConnection* c);
    
    void addInputConnection(ofxOceanodeAbstractConnection* c);
    
    void moveConnections(glm::vec2 moveVector);
    void collapseConnections(glm::vec2 sinkPos, glm::vec2 sourcePos);
    void expandConnections();
    void setInConnectionsPositionForParameter(ofAbstractParameter &p, glm::vec2 pos);
    void setOutConnectionsPositionForParameter(ofAbstractParameter &p, glm::vec2 pos);
    
    void deleteSelf();
    void duplicateSelf(glm::vec2 posToDuplicate = glm::vec2(-1, -1));
    
    bool loadPreset(string presetFolderPath);
    void savePreset(string presetFolderPath);
    
    bool loadConfig(string filename);
    void saveConfig(string filename);
    
    ofJson saveParametersToJson();
    bool loadParametersFromJson(ofJson json);
    
    void setBpm(float bpm);
    void resetPhase();
    
    ofEvent<vector<ofxOceanodeAbstractConnection*>> deleteModuleAndConnections;
    ofEvent<glm::vec2> duplicateModule;
    
    ofParameterGroup* getParameters();
private:
    std::unique_ptr<ofxOceanodeNodeModel> nodeModel;
    std::unique_ptr<ofxOceanodeNodeGui> nodeGui;
    
    std::vector<ofxOceanodeAbstractConnection*> inConnections;
    std::vector<ofxOceanodeAbstractConnection*> outConnections;
    
    ofEventListeners inConnectionsListeners;
    ofEventListeners outConnectionsListeners;
    ofEventListeners toChangeGuiListeners;
};

#endif /* ofxOceanodeNode_h */
