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
#include <cstdint>
#include <unordered_map>

class ofxOceanodeNode;
class ofxOceanodeCanvas;
class ofxOceanodeNodeMacro;
class abstractPortal;
class abstractRouter;

class ofxOceanodeNodesController: public ofxOceanodeBaseController{
public:

    ofxOceanodeNodesController(shared_ptr<ofxOceanodeContainer> _container, ofxOceanodeCanvas* _canvas);
    ~ofxOceanodeNodesController(){};
    
    void draw();
    
private:
    ofEventListener nodeSelectedListener; // syncs canvas click → tree selection
    
    struct NavigableNode {
        ofxOceanodeNode*      node;
        ofxOceanodeCanvas*    canvas;        // the canvas containing this node
        ofxOceanodeNodeMacro* macro;         // nullptr if root canvas
        bool                  matchesSearch; // true if name matches searchFieldMyNodes
    };

    struct CachedNode {
        ofxOceanodeNode* node = nullptr;
        ofxOceanodeNodeMacro* macro = nullptr;
        abstractPortal* portal = nullptr;
        abstractRouter* router = nullptr;
        int type = 3; // 0=macro, 1=portal, 2=router, 3=other
        string baseName, detailName, displayName, sortKey, lowercaseName;
        unsigned int identifier = 0;
        bool open = false;
        bool active = true;
        bool matchesSearch = false;
        bool hasMatchingDescendant = false;
        bool visible = true;
    };

    struct CachedContainer {
        std::weak_ptr<ofxOceanodeContainer> owner;
        std::uint64_t revision = 0;
        bool initialized = false;
        vector<CachedNode> nodes;
        vector<size_t> order;
        string search;
        bool hasSearchMatch = false;
        unsigned int typeMask = 0;
    };

    struct VisibleRow {
        CachedNode* entry;
        ofxOceanodeCanvas* canvas;
        ofxOceanodeNodeMacro* macro; // containing macro, not entry's own macro
        int depth;
        bool parentActive;
    };

    CachedContainer& refreshCache(const shared_ptr<ofxOceanodeContainer>& target, bool expandAll);
    void appendRows(CachedContainer& cache, ofxOceanodeCanvas* hostCanvas,
                    ofxOceanodeNodeMacro* hostMacro, int depth, bool parentActive);
    bool findNode(ofxOceanodeNode* target, const shared_ptr<ofxOceanodeContainer>& host,
                  ofxOceanodeCanvas* hostCanvas, ofxOceanodeNodeMacro* hostMacro,
                  NavigableNode& location);
    void navigateTo(const NavigableNode& target);
    void drawRow(VisibleRow& row, int rowIndex);

    std::unordered_map<ofxOceanodeContainer*, CachedContainer> nodeCaches;
    vector<VisibleRow> visibleRows;
    vector<NavigableNode> navigableNodes; // complete expanded list, including offscreen rows
    string lowercaseSearch;
    int cachedTypeFilter = 0;
    bool rowsDirty = true;
    bool topologyChanged = false;
    bool uniformRowHeight = true;

    string searchFieldMyNodes = "";
    int nodeTypeFilter = 0; // 0=All, 1=Macros, 2=Portals, 3=Routers
    ofxOceanodeNode* selectedNode = nullptr;

    // Deferred scroll state (countdown; fires when scrollPendingFrames reaches 0, >0 means pending)
    ofxOceanodeNode*      pendingScrollNode   = nullptr;
    int                   scrollPendingFrames = 0;
    bool               forceExpandAll      = false;
    bool               forceCollapseAll    = false;
    bool               scrollTreeToSelected = false;  // selected row is included by the clipper
    int                refocusNodesDelay    = 0;       // counts down frames before re-focusing the Nodes window

    shared_ptr<ofxOceanodeContainer> container;
    ofxOceanodeCanvas* canvas;
};


#endif /* ofxOceanodeNodesController_h */
