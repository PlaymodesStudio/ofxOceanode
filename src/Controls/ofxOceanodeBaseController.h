//
//  ofxOceanodeBaseController.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 12/03/2018.
//

#ifndef ofxOceanodeBaseController_h
#define ofxOceanodeBaseController_h

#include "ofMain.h"

class ofxOceanodeContainer;

class ofxOceanodeBaseController{
public:
    ofxOceanodeBaseController(shared_ptr<ofxOceanodeContainer> _container, string name);
    virtual ~ofxOceanodeBaseController(){};
    
    virtual void draw();
    virtual void update();
    
    string getControllerName(){return controllerName;};
protected:
    string controllerName;
    
    ofEventListeners listeners;
    
    shared_ptr<ofxOceanodeContainer> container;
};

#endif /* ofxOceanodeBaseController_h */
