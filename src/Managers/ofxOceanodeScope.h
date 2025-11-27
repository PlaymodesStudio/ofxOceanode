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

struct ofxOceanodeScopeWindowConfig {
    bool hasConfig = false;
    float posX = 0;
    float posY = 0;
    float width = 800;
    float height = 600;
    unsigned int dockID = 0;  // ImGuiID type
};

struct ofxOceanodeScopeParameterData {
    std::string parameterPath;  // group hierarchy + name
    float sizeRelative;
    // Future: Add more parameter-specific data here as needed
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
    
    void saveScope(const std::string& filepath = "");
    void loadScope(const std::string& filepath = "", ofxOceanodeContainer* container = nullptr);
    
    const std::vector<ofxOceanodeScopeParameterData>& getLoadedParameterData() const { return loadedParameterData; }

private:
    std::vector<scopeFunc> scopeTypes;
    std::vector<ofxOceanodeScopeItem> scopedParameters;
    
    float windowWidth;
    float windowHeight;
    
    ofxOceanodeScopeWindowConfig windowConfig;
    std::vector<ofxOceanodeScopeParameterData> loadedParameterData;
    
    // Auto-save functionality
    std::string saveFilePath = "scope_config.json";
    ofxOceanodeScopeWindowConfig lastWindowConfig;
    bool isLoadingScope = false;
};

#endif /* ofxOceanodeScope_h */
