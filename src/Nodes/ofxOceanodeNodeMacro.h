//
//  ofxOceanodeNodeMacro.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 20/06/2019.
//

#ifndef ofxOceanodeNodeMacro_h
#define ofxOceanodeNodeMacro_h

#include "ofxOceanode.h"
#include "ofxOceanodeShared.h"
#include "router.h"

class ofxOscMessage;

class ofxOceanodeNodeMacro : public ofxOceanodeNodeModel{
public:
    ofxOceanodeNodeMacro();
    ~ofxOceanodeNodeMacro(){};
    
	void setup(){setup("");};
	void setup(string additionalInfo);
    void update(ofEventArgs &a);
    void draw(ofEventArgs &a);
    
    void setContainer(ofxOceanodeContainer* container);
	
	void macroSave(ofJson &json, string path);
	void macroLoad(ofJson &json, string path);
    
    void setBpm(float bpm){container->setBpm(bpm);};
    void resetPhase(){container->resetPhase();};
    
#ifdef OFXOCEANODE_USE_OSC
    bool receiveOscMessage(ofxOscMessage &m) override{
        container->receiveOscMessage(m);
        return true;};
#endif
	
	shared_ptr<ofxOceanodeContainer> getContainer() {return container;};
    
    void activate(){
        active = true;
    }
    
    void deactivate(){
        active = false;
    }
    
private:
    void newNodeCreated(ofxOceanodeNode* &node);
	void loadMacroInsideCategory(int newPresetIndex);
	void updateCurrentCategoryFromPath(string path);
    
#ifndef OFXOCEANODE_HEADLESS
    ofxOceanodeCanvas canvas;
#endif
    shared_ptr<ofxOceanodeContainer> container;
    shared_ptr<ofxOceanodeNodeRegistry> registry;
    shared_ptr<ofxOceanodeTypesRegistry> typesRegistry;
    
    ofEventListener newNodeListener;
    std::unordered_map<string, ofEventListeners> inoutListeners;
    ofEventListeners deleteListeners;
	
	ofEventListener macroUpdatedListener;
    ofEventListener activeListener;
    
    bool showWindow;
    string presetPath;
    ofParameter<int> bank;
    int previousBank;
    shared_ptr<ofxOceanodeParameter<int>> bankDropdown;
    vector<string> bankNames;
    ofParameter<int> preset;
    shared_ptr<ofxOceanodeParameter<int>> presetDropdown;
    int currentPreset;
    ofParameter<string> savePresetField;
    vector<string> presetsInBank;
    ofParameter<string> presetName;
    ofParameter<bool> savePreset;
	
	bool localPreset;
	string nextPresetPath;
	string currentMacro;
	string currentMacroPath;
	deque<string> currentCategory;
	macroCategory currentCategoryMacro;
	deque<string> saveAsTempCategory;
	
	ofParameter<bool> active;
    
    ofParameter<std::function<void()>> presetControl;

    
    ofEventListeners presetActionsListeners;
    
    string canvasParentID;
};

#endif /* ofxOceanodeNodeMacro_h */
