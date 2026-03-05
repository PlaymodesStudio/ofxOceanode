//
//  ofxOceanodeMiniMapController.h
//  ofxOceanode
//

#ifndef ofxOceanodeMiniMapController_h
#define ofxOceanodeMiniMapController_h

#ifndef OFXOCEANODE_HEADLESS

#include "ofxOceanodeBaseController.h"
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeCanvas.h"

// Forward declarations
class ofxOceanodeNodeMacro;
class ofxOceanodeNode;

struct MiniMapScopeEntry {
    string label;                          // short label (just the macro name, not full path)
    string fullPath;                       // unique path for selection tracking
    ofxOceanodeContainer* container = nullptr;  // the scope's container
    ofxOceanodeCanvas*    canvas    = nullptr;  // the scope's canvas
    ofxOceanodeNodeMacro* macroNode       = nullptr; // nullptr for root; pointer to the macro model for this scope
    ofxOceanodeNode*      parentNode      = nullptr; // nullptr for root; the ofxOceanodeNode that wraps the macroNode
    ofxOceanodeContainer* parentContainer = nullptr; // nullptr for root; the parent container where this macro lives
    ofxOceanodeCanvas*    parentCanvas    = nullptr; // nullptr for root; the parent canvas where this macro lives
    int depth = 0;                         // 0 = root, 1 = first-level macro, etc.
    vector<int> childIndices;              // indices into scopeList of direct children
};

class ofxOceanodeMiniMapController : public ofxOceanodeBaseController {
public:
    ofxOceanodeMiniMapController(shared_ptr<ofxOceanodeContainer> _container, ofxOceanodeCanvas* _canvas);
    ~ofxOceanodeMiniMapController() = default;

    void draw() override;
    void update() override {};

private:
    void buildScopeList(
        ofxOceanodeContainer* container,
        ofxOceanodeCanvas*    canvas,
        const string&         path,
        const string&         label,
        ofxOceanodeNodeMacro* macroNode,
        ofxOceanodeNode*      parentNode,
        ofxOceanodeContainer* parentContainer,
        ofxOceanodeCanvas*    parentCanvas,
        int                   depth,
        int                   parentIndex,
        vector<MiniMapScopeEntry>& out);

    void renderScopeTree(int entryIndex);
    void renderMinimap(ofxOceanodeContainer* activeContainer, ofxOceanodeCanvas* activeCanvas);

    bool entryMatchesSearch(int entryIndex, const string& lowerQuery) const;

    shared_ptr<ofxOceanodeContainer> rootContainer;
    ofxOceanodeCanvas*               rootCanvas;
    int                              selectedScopeIndex = 0;
    vector<MiniMapScopeEntry>        scopeList;
    float                            treeHeight = 120.0f;
    char                             searchBuf[256];
};

#endif // OFXOCEANODE_HEADLESS

#endif /* ofxOceanodeMiniMapController_h */
