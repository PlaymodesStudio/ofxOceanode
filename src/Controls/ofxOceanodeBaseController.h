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
    virtual void drawBeforeWindows() {}

    // Menu controllers draw inside a top-level menu instead of a dockable window.
    // They remain registered and keep receiving updates when the menu is closed.
    virtual bool isMenuController() const { return false; }

    // Draw dialogs from the persistent DockSpace, even when a menu is closed.
    virtual void drawPopups() {}

    // A controller that opens and draws its own windows (from drawBeforeWindows):
    // Controls draws no window named after it, and the Window menu lists none.
    virtual bool drawsOwnWindows() const { return false; }

    // Entries of its own in the Window menu, after the controllers' checkboxes
    // (a controller that draws its own windows lists them here).
    virtual void drawWindowMenuItems() {}

    // An optional readout at the right end of the menu bar (CPU, status...).
    // Return its width (0: none); it is drawn at the cursor in that width.
    virtual float menuBarStatusWidth() { return 0.0f; }
    virtual void drawMenuBarStatus() {}
    // Optional readouts placed before the FPS counter.
    virtual float menuBarStatusSuffixWidth() { return 0.0f; }
    virtual void drawMenuBarStatusSuffix() {}
    
    string getControllerName(){return controllerName;};
protected:
    string controllerName;
};

#endif /* ofxOceanodeBaseController_h */
