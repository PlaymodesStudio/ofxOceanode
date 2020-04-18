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
    ofxOceanodeBaseController(string name);
    virtual ~ofxOceanodeBaseController(){};
    
    virtual void draw() = 0;
    virtual void update(){};
    
    string getControllerName(){return controllerName;};
protected:
    string controllerName;
};

#endif /* ofxOceanodeBaseController_h */
