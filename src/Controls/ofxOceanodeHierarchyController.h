//
//  ofxOceanodeHierarchyController.h
//  ofxOceanode
//

#ifndef ofxOceanodeHierarchyController_h
#define ofxOceanodeHierarchyController_h

#ifndef OFXOCEANODE_HEADLESS

#include "ofxOceanodeBaseController.h"
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeCanvas.h"
#include "ofxOceanodeShared.h"

// Forward declarations
class ofxOceanodeNodeMacro;
class ofxOceanodeNode;

struct HierarchyEntry {
    string label;                               // display label: "Canvas" or "Name [Type]"

    // This scope
    ofxOceanodeContainer* container  = nullptr;
    ofxOceanodeCanvas*    canvas     = nullptr;
    ofxOceanodeNodeMacro* macroNode  = nullptr; // nullptr for root
    ofxOceanodeNode*      nodeWrapper = nullptr;// the ofxOceanodeNode wrapping macroNode

    // Parent scope (needed for navigation: select node inside parent canvas)
    ofxOceanodeContainer* hostContainer = nullptr; // container that holds nodeWrapper
    ofxOceanodeCanvas*    hostCanvas    = nullptr; // canvas that holds nodeWrapper
    ofxOceanodeNodeMacro* hostMacro     = nullptr; // macro that owns hostCanvas (nullptr = root canvas)

    int depth = 0;
    int parentIndex = -1;
    vector<int> childIndices;
};

class ofxOceanodeHierarchyController : public ofxOceanodeBaseController {
public:
    ofxOceanodeHierarchyController(shared_ptr<ofxOceanodeContainer> _container,
                                   ofxOceanodeCanvas* _canvas);
    ~ofxOceanodeHierarchyController() = default;

    void draw() override;
    void update() override {}

private:
    void buildHierarchy(
        ofxOceanodeContainer* container,
        ofxOceanodeCanvas*    canvas,
        ofxOceanodeNodeMacro* macroNode,
        ofxOceanodeNode*      nodeWrapper,
        ofxOceanodeContainer* hostContainer,
        ofxOceanodeCanvas*    hostCanvas,
        ofxOceanodeNodeMacro* hostMacro,
        const string&         label,
        int                   depth,
        int                   parentIdx,
        vector<HierarchyEntry>& out);

    shared_ptr<ofxOceanodeContainer> rootContainer;
    ofxOceanodeCanvas*               rootCanvas;
    vector<HierarchyEntry>           entries;

    // Interaction state
    int selectedEntryIndex = -1;       // Currently cyan-highlighted entry (active canvas, -1 = none)
    bool scrollToSelected = false;      // Flag to scroll hierarchy view to selected entry
    ofxOceanodeNode* selectedNodePtr = nullptr;  // Node highlighted in orange (nullptr = none)
    bool clickOriginIsHierarchy = false; // Suppresses scroll when click came from this panel
    string activeCanvasUID;             // Last known active canvas UID
    int pendingClickIndex = -1;        // Index of entry with a pending single-click
    float pendingClickTime = -1.0f;    // Time when the click was registered

    // Zoom / scale
    float hierarchyScale = 1.0f;       // Zoom factor (0.4 – 2.0); controlled by slider
    
    // Event listeners
    ofEventListener activeCanvasListener;
    ofEventListener nodeSelectedListener;

    // Pending centering (deferred to next frame so canvas layout is valid)
    bool pendingCenter = false;
    ofxOceanodeNode* pendingCenterNode = nullptr;
    ofxOceanodeCanvas* pendingCenterCanvas = nullptr;

};

#endif // OFXOCEANODE_HEADLESS
#endif /* ofxOceanodeHierarchyController_h */
