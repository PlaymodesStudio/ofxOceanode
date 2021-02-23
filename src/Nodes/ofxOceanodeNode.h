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

class ofxOceanodeNodeGui;
class ofxOceanodeNodeModel;
class ofxOceanodeContainer;
class ofxOceanodeAbstractParameter;

class ofxOceanodeNode {
public:
    ofxOceanodeNode(unique_ptr<ofxOceanodeNodeModel> && _nodeModel);
    ~ofxOceanodeNode();
    
    void setup();
    void update(ofEventArgs &e);
    void draw(ofEventArgs &e);
    
#ifndef OFXOCEANODE_HEADLESS
    void setGui(std::unique_ptr<ofxOceanodeNodeGui>&& gui);
    ofxOceanodeNodeGui& getNodeGui();
#endif
    
    ofxOceanodeNodeModel& getNodeModel();
    
    ofColor getColor();
    
    void deleteSelf();
    
    bool loadPreset(string presetFolderPath);
    void savePreset(string presetFolderPath);
    
    bool loadPersistentPreset(string presetFolderPath);
    void savePersistentPreset(string presetFolderPath);
    
    void presetWillBeLoaded();
    void presetHasLoaded();
    void loadPresetBeforeConnections(string presetFolderPath);
    
    void deserializeParameter(ofJson &json, ofxOceanodeAbstractParameter &p, bool persistentPreset = false);
    
    bool loadConfig(string filename, bool persistentPreset = false);
    void saveConfig(string filename, bool persistentPreset = false);
    
    ofJson saveParametersToJson(bool persistentPreset = false);
    bool loadParametersFromJson(ofJson json, bool persistentPreset = false);
    
    void saveInspectorParametersToJson(ofJson &json);
    void loadInspectorParametersFromJson(ofJson json);
    
    void setBpm(float bpm);
    void resetPhase();
    
    bool getIsPersistent(){return isPersistent;};
    void setIsPersistent(bool p){isPersistent = p;};
    
    void setActive(bool act);
    bool getActive(){return active;};
    
    ofEvent<void> deleteModule;
    
    ofParameterGroup& getParameters();
    ofParameterGroup& getInspectorParameters();
private:
    std::unique_ptr<ofxOceanodeNodeModel> nodeModel;
#ifndef OFXOCEANODE_HEADLESS
    std::unique_ptr<ofxOceanodeNodeGui> nodeGui;
#endif
    
    ofEventListeners nodeModelListeners;
    
    bool isPersistent;
    bool active;
};

#endif /* ofxOceanodeNode_h */
