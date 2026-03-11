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

};

#endif // OFXOCEANODE_HEADLESS
#endif /* ofxOceanodeHierarchyController_h */
