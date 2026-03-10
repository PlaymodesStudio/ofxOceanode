//
//  ofxOceanodeNodesController.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 13/03/2018.
//

#ifndef ofxOceanodeNodesController_h
#define ofxOceanodeNodesController_h

#include "ofxOceanodeBaseController.h"
#include "ofEvents.h"

class ofxOceanodeNode;
class ofxOceanodeCanvas;
class ofxOceanodeNodeMacro;

class ofxOceanodeNodesController: public ofxOceanodeBaseController{
public:

    ofxOceanodeNodesController(shared_ptr<ofxOceanodeContainer> _container, ofxOceanodeCanvas* _canvas);
    ~ofxOceanodeNodesController(){};
    
    void draw();
    
private:
    ofEventListener nodeSelectedListener; // syncs canvas click → tree selection
    
//    float bpm;
//    ofEventListener bpmListener;
//    ofParameter<float> phase;
//    ofEventListener phaseListener;
    
    struct NavigableNode {
        ofxOceanodeNode*      node;
        ofxOceanodeCanvas*    canvas;        // the canvas containing this node
        ofxOceanodeNodeMacro* macro;         // nullptr if root canvas
        bool                  matchesSearch; // true if name matches searchFieldMyNodes
    };

    vector<NavigableNode> navigableNodes;   // rebuilt each frame

    string searchFieldMyNodes = "";
    int nodeTypeFilter = 0; // 0=All, 1=Macros, 2=Portals, 3=Routers
    ofxOceanodeNode* selectedNode = nullptr;

    // Deferred scroll state (applied one frame after focus/visibility request)
    ofxOceanodeNode*      pendingScrollNode   = nullptr;
    ofxOceanodeCanvas*    pendingScrollCanvas = nullptr;
    ofxOceanodeNodeMacro* pendingScrollMacro  = nullptr;  // re-activates macro canvas after scroll
    bool                  scrollPending       = false;
    bool               forceExpandAll      = false;
    bool               scrollTreeToSelected = false;  // set by arrow-key nav, consumed by listNodes
    int                refocusNodesDelay    = 0;       // counts down frames before re-focusing the Nodes window

    shared_ptr<ofxOceanodeContainer> container;
    ofxOceanodeCanvas* canvas;
};


#endif /* ofxOceanodeNodesController_h */
