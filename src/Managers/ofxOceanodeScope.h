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
class ImVec2;

class ofxOceanodeScopeItem {
public:
    ofxOceanodeScopeItem(ofxOceanodeAbstractParameter* p, ofColor c = ofColor::white, float s = 1) : parameter(p), color(c), sizeRelative(s){};
    ~ofxOceanodeScopeItem(){};
    
    ofxOceanodeAbstractParameter* parameter;
    ofColor color;
    float sizeRelative;
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
    
private:
    std::vector<scopeFunc> scopeTypes;
    std::vector<ofxOceanodeScopeItem> scopedParameters;
    
    float windowHeight;
};

#endif /* ofxOceanodeScope_h */
