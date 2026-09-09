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
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "ofColor.h"
#include "ofJson.h"

class ofxOceanodeAbstractParameter;
class ofxOceanodeContainer;
class ImVec2;

using ofxOceanodeScopeDrawFunc = std::function<void(ofxOceanodeAbstractParameter*, ImVec2)>;

// Kept behind a shared object so existing scope items immediately see a
// renderer override without doing a type lookup on every frame.
struct ofxOceanodeScopeRenderer {
    ofxOceanodeScopeDrawFunc draw;
};

class ofxOceanodeScopeItem {
public:
    ofxOceanodeScopeItem(
        ofxOceanodeAbstractParameter* p,
        ofColor c,
        const std::string& canvasId = "",
        const std::string& nodeName = "",
        const std::string& cachedWindowName = "",
        std::shared_ptr<ofxOceanodeScopeRenderer> cachedRenderer = nullptr
    ) : parameter(p),
        color(c),
        canvasID(canvasId),
        cachedNodeName(nodeName),
        windowName(cachedWindowName),
        renderer(std::move(cachedRenderer)) {};
    
    ~ofxOceanodeScopeItem(){};
    
    ofxOceanodeAbstractParameter* parameter;
    ofColor color;
    std::string canvasID;          // Full macro hierarchy (e.g., "0.2.5")
    std::string cachedNodeName;    // Node name at time of scoping
    std::string windowName;        // Stable ImGui name, built once on add
    std::shared_ptr<ofxOceanodeScopeRenderer> renderer;
    
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
    using scopeDrawFunc = ofxOceanodeScopeDrawFunc;
    // Legacy combined matcher/draw callback. Prefer addScopeRenderer() so
    // canScope() can reject unsupported types without invoking a renderer.
    using scopeFunc = std::function<bool(ofxOceanodeAbstractParameter* p, ImVec2 size)>;
    
    ofxOceanodeScope(){};
    ~ofxOceanodeScope(){};
    
    static ofxOceanodeScope* getInstance(){
        static ofxOceanodeScope instance;
        return &instance;
    }
    
    void setup();
    void draw();
    
    bool addParameter(
        ofxOceanodeAbstractParameter* p,
        ofColor _color,
        const std::string& canvasID = "",
        const std::string& nodeName = ""
    );
    bool removeParameter(ofxOceanodeAbstractParameter* p);
    
    void addScopeRenderer(const std::string& valueType, scopeDrawFunc renderer, bool replaceExisting = true);
    void addScopeFunc(scopeFunc f);
    const std::vector<scopeFunc>& getScopedTypes(); // Legacy renderer-list API
    bool canScope(const ofxOceanodeAbstractParameter* p) const;
    bool drawParameter(ofxOceanodeAbstractParameter* p, ImVec2 size) const;

	// load / save scopes
    ofxOceanodeScopeState getScopeState() const;
    void setScopeState(const ofxOceanodeScopeState& state);
    ofxOceanodeScopeWindowConfig getWindowConfig() const;
    void setWindowConfig(const ofxOceanodeScopeWindowConfig& config);
    void clearScopedParameters();
    // Callback for scope changes (for auto-save)
    using ScopeChangedCallback = std::function<void()>;
    void setScopeChangedCallback(ScopeChangedCallback callback);
    // Flag to suppress dock-tree rebuilds while loading a preset
    void setLoadingFromPreset(bool loading) { isLoadingFromPreset = loading; }

private:
    struct PendingDockWindow {
        std::string windowName;
        size_t scopeCountAtAdd = 1;
    };

    std::shared_ptr<ofxOceanodeScopeRenderer> findRenderer(const ofxOceanodeAbstractParameter* p) const;
    bool drawParameter(
        ofxOceanodeAbstractParameter* p,
        ImVec2 size,
        const std::shared_ptr<ofxOceanodeScopeRenderer>& cachedRenderer
    ) const;

    std::unordered_map<std::string, std::shared_ptr<ofxOceanodeScopeRenderer>> typedRenderers;
    std::vector<scopeFunc> legacyScopeTypes;
    std::vector<scopeFunc> compatibilityScopeTypes;
    std::vector<ofxOceanodeScopeItem> scopedParameters;
    std::unordered_set<ofxOceanodeAbstractParameter*> scopedParameterSet;
    std::vector<PendingDockWindow> pendingDockWindows;
    
    ofxOceanodeScopeWindowConfig windowConfig;
    ofxOceanodeScopeWindowConfig lastWindowConfig;
    ScopeChangedCallback scopeChangedCallback;
    void notifyScopeChanged();
    unsigned int lastCentralScopeWindowID = 0;
    bool dockMaintenancePending = true;
    bool scopeInteractionInProgress = false;
    bool scopeWindowRectChangedDuringInteraction = false;
    unsigned long long dockLayoutSignatureAtInteractionStart = 0;
    
    bool isLoadingFromPreset = false;
};

#endif /* ofxOceanodeScope_h */
