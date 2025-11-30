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
    ofxOceanodeScopeItem(
        ofxOceanodeAbstractParameter* p,
        ofColor c = ofColor::white,
        float s = 1,
        const std::string& canvasId = "",
        const std::string& nodeName = ""
    ) : parameter(p),
        color(c),
        sizeRelative(s),
        canvasID(canvasId),
        cachedNodeName(nodeName) {};
    
    ~ofxOceanodeScopeItem(){};
    
    // Existing fields
    ofxOceanodeAbstractParameter* parameter;
    ofColor color;
    float sizeRelative;
    
    // New fields for full path tracking
    std::string canvasID;          // Full macro hierarchy (e.g., "0.2.5")
    std::string cachedNodeName;    // Node name at time of scoping
    
    // Helper method to construct full display path
    std::string getFullPath() const;
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
    std::string parameterPath;  // DEPRECATED - kept for backward compatibility
    std::string canvasID;       // Full macro hierarchy (e.g., "0.2.5")
    std::string nodeName;       // Node name
    std::string paramName;      // Parameter name
    float sizeRelative;
    
    // Backward compatibility: generate old-style path for legacy loading
    std::string getLegacyPath() const {
        return nodeName + "/" + paramName;
    }
    
    // Forward compatibility: generate new full path
    std::string getFullPath() const;
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
    
    void addParameter(
        ofxOceanodeAbstractParameter* p,
        ofColor _color,
        const std::string& canvasID = "",
        const std::string& nodeName = ""
    );
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
