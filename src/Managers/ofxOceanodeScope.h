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

class ofxOceanodeAbstractParameter;

class ofxOceanodeScope {
public:
    using scopeFunc = std::function<bool(ofxOceanodeAbstractParameter* p)>;
    
    ofxOceanodeScope(){};
    ~ofxOceanodeScope(){};
    
    static ofxOceanodeScope* getInstance(){
        static ofxOceanodeScope instance;
        return &instance;
    }
    
    void setup();
    void draw();
    
    void addParameter(ofxOceanodeAbstractParameter* p);
    void removeParameter(ofxOceanodeAbstractParameter* p);
    
private:
    std::vector<scopeFunc> scopeTypes;
    std::vector<ofxOceanodeAbstractParameter*> scopedParameters;
};

#endif /* ofxOceanodeScope_h */
