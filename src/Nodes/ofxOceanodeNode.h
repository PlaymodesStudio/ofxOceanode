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
    
    void setup();
    
#ifndef OFXOCEANODE_HEADLESS
    void setGui(std::unique_ptr<ofxOceanodeNodeGui>&& gui);
    ofxOceanodeNodeGui& getNodeGui();
#endif
    
    ofxOceanodeNodeModel& getNodeModel();
    
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
    
    bool loadPersistentPreset(string presetFolderPath);
    void savePersistentPreset(string presetFolderPath);
    
    void presetWillBeLoaded();
    void presetHasLoaded();
    void loadPresetBeforeConnections(string presetFolderPath);
    
    bool loadConfig(string filename, bool persistentPreset = false);
    void saveConfig(string filename, bool persistentPreset = false);
    
    ofJson saveParametersToJson(bool persistentPreset = false);
    bool loadParametersFromJson(ofJson json, bool persistentPreset = false);
    
    void setBpm(float bpm);
    void setPhase(float _phase);
    void resetPhase();
    
    bool getIsPersistent(){return isPersistent;};
    void setIsPersistent(bool p){isPersistent = p;};
    
    bool checkHasInConnection(ofAbstractParameter &p);
    
    ofEvent<vector<ofxOceanodeAbstractConnection*>> deleteModuleAndConnections;
    ofEvent<vector<ofxOceanodeAbstractConnection*>> deleteConnections;
    ofEvent<glm::vec2> duplicateModule;
    
    shared_ptr<ofParameterGroup> getParameters();
private:
    std::unique_ptr<ofxOceanodeNodeModel> nodeModel;
#ifndef OFXOCEANODE_HEADLESS
    std::unique_ptr<ofxOceanodeNodeGui> nodeGui;
#endif
    
    std::vector<ofxOceanodeAbstractConnection*> inConnections;
    std::vector<ofxOceanodeAbstractConnection*> outConnections;
    
    ofEventListeners inConnectionsListeners;
    ofEventListeners outConnectionsListeners;
    ofEventListeners toChangeGuiListeners;
    ofEventListeners nodeModelListeners;
    
    bool isPersistent;
};

#endif /* ofxOceanodeNode_h */
