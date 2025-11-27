//
//  ofxOceanodeScope.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 05/05/2020.
//

#ifndef ofxOceanodeScope_h
#define ofxOceanodeScope_h

#include <functional>
#include <iostream>
#include <vector>
#include "ofColor.h"
#include "ofJson.h"

class ofxOceanodeAbstractParameter;
class ofxOceanodeContainer;
class ImVec2;

class ofxOceanodeScopeItem {
public:
    ofxOceanodeScopeItem(ofxOceanodeAbstractParameter* p, ofColor c = ofColor::white, float s = 1) : parameter(p), color(c), sizeRelative(s){};
    ~ofxOceanodeScopeItem(){};
    
    ofxOceanodeAbstractParameter* parameter;
    ofColor color;
    float sizeRelative;
};

// load / save scopes data structures
struct ofxOceanodeScopeWindowConfig {
    bool hasConfig = false;
    float posX = 0;
    float posY = 0;
    float width = 800;
    float height = 600;
};
struct ofxOceanodeScopeParameterData {
    std::string parameterPath;  // group hierarchy + name
    float sizeRelative;
};
struct ofxOceanodeScopeState {
    ofxOceanodeScopeWindowConfig windowConfig;
    std::vector<ofxOceanodeScopeParameterData> parameters;
    
    // Serialization helpers
    ofJson toJson() const;
    static ofxOceanodeScopeState fromJson(const ofJson& json);
};

class ofxOceanodeScope {
public:
    using scopeFunc = std::function<bool(ofxOceanodeAbstractParameter* p, ImVec2 size)>;
    
    ofxOceanodeScope(){};
    ~ofxOceanodeScope(){};
    
    static ofxOceanodeScope* getInstance(){
        static ofxOceanodeScope instance;
        return &instance;
    }
    
    void setup();
    void draw();
    
    void addParameter(ofxOceanodeAbstractParameter* p, ofColor _color);
    void removeParameter(ofxOceanodeAbstractParameter* p);
    
    void addScopeFunc(scopeFunc f){scopeTypes.push_back(f);};
    const std::vector<scopeFunc> getScopedTypes(){return scopeTypes;};

	// load / save scopes
    ofxOceanodeScopeState getScopeState() const;
    void setScopeState(const ofxOceanodeScopeState& state);
    ofxOceanodeScopeWindowConfig getWindowConfig() const;
    void setWindowConfig(const ofxOceanodeScopeWindowConfig& config);
    void clearScopedParameters();
    // Callback for scope changes (for auto-save)
    using ScopeChangedCallback = std::function<void()>;
    void setScopeChangedCallback(ScopeChangedCallback callback);
    // Accessor for container to get scoped parameters (for setting sizeRelative)
    std::vector<ofxOceanodeScopeItem>& getScopedParameters() { return scopedParameters; }

private:
    std::vector<scopeFunc> scopeTypes;
    std::vector<ofxOceanodeScopeItem> scopedParameters;
    
    float windowWidth;
    float windowHeight;
    
    ofxOceanodeScopeWindowConfig windowConfig;
    std::vector<ofxOceanodeScopeParameterData> loadedParameterData;
    
    // // load / save scopes : Auto-save functionality
    std::string saveFilePath = "scope_config.json";
    ofxOceanodeScopeWindowConfig lastWindowConfig;
    ScopeChangedCallback scopeChangedCallback;
    void notifyScopeChanged();
};

#endif /* ofxOceanodeScope_h */
