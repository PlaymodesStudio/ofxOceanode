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
class ofxOceanodeCanvas;

class ofxOceanodeBaseController{
public:
    ofxOceanodeBaseController(string name);
    virtual ~ofxOceanodeBaseController(){};
    
    virtual void draw() = 0;
    virtual void update(){};

    // Menu controllers draw inside a top-level menu instead of a dockable window.
    // They remain registered and keep receiving updates when the menu is closed.
    virtual bool isMenuController() const { return false; }

    // Draw dialogs from the persistent DockSpace, even when a menu is closed.
    virtual void drawPopups() {}
    
    string getControllerName(){return controllerName;};
protected:
    string controllerName;
};

#endif /* ofxOceanodeBaseController_h */
