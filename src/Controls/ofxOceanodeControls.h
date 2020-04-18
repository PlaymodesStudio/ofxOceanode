//
//  ofxOceanodeControls.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 12/03/2018.
//

#ifndef ofxOceanodeControls_h
#define ofxOceanodeControls_h

#ifndef OFXOCEANODE_HEADLESS

#include "ofMain.h"
#include "ofxOceanodeBaseController.h"

class ofxOceanodeContainer;

class ofxOceanodeControls{
public:
    ofxOceanodeControls(shared_ptr<ofxOceanodeContainer> _container);
    ~ofxOceanodeControls(){};
    
    void draw();
    void update();
    
    template<class T>
    T* get(){
        for(auto &c : controllers){
            if(dynamic_cast<T>(c.get()) != nullptr){
                return dynamic_cast<T>(c.get());
            }
        }
        return nullptr;
    }
    
    //TODO: add arguments
    template<typename T>
    T& addController(){
        auto uniqueC = make_unique<T>();
        auto ref = uniqueC.get();
        controllers.push_back(move(uniqueC));
        return *ref;
    }
    
private:
    std::shared_ptr<ofxOceanodeContainer> container;
    
    ofEventListeners listeners;
    
    vector<unique_ptr<ofxOceanodeBaseController>> controllers;
};

#endif

#endif /* ofxOceanodeControls_h */
