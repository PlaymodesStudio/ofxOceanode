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
class ofxOceanodeCanvas;

class ofxOceanodeControls{
public:

    ofxOceanodeControls(shared_ptr<ofxOceanodeContainer> _container,ofxOceanodeCanvas* _canvas);
    ~ofxOceanodeControls(){};
    
    void draw();
    void update();
    
    template<class T>
    shared_ptr<T> get(){
        for(auto c : controllers){
            if(dynamic_pointer_cast<T>(c) != nullptr){
                return dynamic_pointer_cast<T>(c);
            }
        }
        return nullptr;
    }
    
    //TODO: add arguments
    template<typename T>
    shared_ptr<T> addController(){
        controllers.push_back(make_shared<T>());
        return dynamic_pointer_cast<T>(controllers.back());
    }
    
private:
    std::shared_ptr<ofxOceanodeContainer> container;
    ofEventListeners listeners;
    vector<shared_ptr<ofxOceanodeBaseController>> controllers;
};

#endif

#endif /* ofxOceanodeControls_h */
