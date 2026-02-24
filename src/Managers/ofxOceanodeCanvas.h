//
//  ofxOceanodeCanvas.h
//  example-basic
//
//  Created by Eduard Frigola on 19/06/2017.
//
//

#ifndef ofxOceanodeCanvas_h
#define ofxOceanodeCanvas_h

#ifndef OFXOCEANODE_HEADLESS
#include "ofMain.h"

class ofxOceanodeContainer;
class ofxOceanodeNodeGui;
class ofxOceanodeAbstractParameter;
class ofxOceanodeAbstractConnection;

class ofxOceanodeCanvas{
public:
    //ofxOceanodeCanvas(){};
    ~ofxOceanodeCanvas(){};
    
    void setup(string _uid = "Canvas", string _pid = "");
    void update(){};
    void draw(bool *open = NULL, ofColor color = ofColor(255, 255, 255, 40), string title = "");
    
    void setContainer(shared_ptr<ofxOceanodeContainer> c){container = c;};
    
    string getUniqueID(){return uniqueID;};
    
    glm::vec2 getScrolling(){return scrolling;};
    glm::vec2 getScrollingOffset(){return scrollingOffset;};
    glm::vec2 getOffsetToCenter(){return offsetToCenter;};
    void setScrolling(glm::vec2 o){scrolling = o;}
    
    void bringOnTop(){onTop = true;};
    
	// node dimensions
	int getNodeWidth(){return NODE_WIDTH_TEXT+NODE_WIDTH_WIDGET;};
	void setNodeWidth(int ww, int tw){NODE_WIDTH_WIDGET=ww;NODE_WIDTH_TEXT=tw;};
	
	void setNodeWidthText(int n){NODE_WIDTH_TEXT = n;};
	int getNodeWidthText(){return NODE_WIDTH_TEXT;};
	
	void setNodeWidthWidget(int n){NODE_WIDTH_WIDGET = n;};
	int getNodeWidthWidget(){return NODE_WIDTH_WIDGET;};
	
	void setGridDivisions(int n){gridDivisions= n;};
	int getGridDivisions(){return gridDivisions;};

	int getGridSize(){return GRID_SIZE;};
	// (nodeWidthTotal should be % by 4 as we use it for GRID_SIZE / snaping
	int getTotalNodeWidth(){return NODE_WIDTH_TEXT+NODE_WIDTH_WIDGET+2*NODE_WINDOW_PADDING.x;};
	void updateGridSize(){GRID_SIZE = getTotalNodeWidth()/gridDivisions;};
	
	void setSnapToGrid(bool b){snap_to_grid=b;};
	bool getSnapToGrid(){return snap_to_grid;};
	
private:
    glm::vec3 getMatrixScale(const glm::mat4 &m);
    glm::mat4 translateMatrixWithoutScale(const glm::mat4 &m, glm::vec3 translationVector);
    
    glm::vec2 screenToCanvas(glm::vec2 p);
    glm::vec2 canvasToScreen(glm::vec2 p);
    glm::vec2 snapToGrid(glm::vec2 position);
    
    shared_ptr<ofxOceanodeContainer> container;
    
    ofParameter<glm::mat4> *transformationMatrix;
    glm::vec2 dragCanvasInitialPoint;
    
    ofEventListeners listeners;
    
    ofEventListener changedTransformationMatrix;
    
    glm::vec2 selectInitialPoint;
    glm::vec2 selectEndPoint;
    bool isSelecting;
    bool entireSelect;
    ofRectangle selectedRect;
    
    string lastSelectedNode = "";
    string someSelectedModuleMove = "";
    glm::vec2 moveSelectedModulesWithDrag;
    bool someDragAppliedToSelection = false;
    
    bool canvasHasScolled = false;
    void deselectAllNodes();
    void selectAllNodes();
    
    vector<string> categoriesVector;
    vector<vector<string>> options;
    string searchField = "";
    string lastSearchField = "";
    int numTimesPopup = 0;
    
    // Search result navigation
    struct SearchResultItem {
        string name;
        string type; // "node" or "macro"
        string macroPath; // only used for macros
        
        SearchResultItem(const string& n, const string& t, const string& mp = "")
            : name(n), type(t), macroPath(mp) {}
    };
    vector<SearchResultItem> filteredSearchResults;
    int selectedSearchResultIndex = -1;
    
    bool inited = false;
    
    glm::vec2 scrolling = glm::vec2(0.0f, 0.0f);
    glm::vec2 scrollBeforeHover = glm::vec2(0.0f, 0.0f);
    bool returnToOldScrolling=false;
    int itemHovered=-1;
    
    glm::vec2 scrollingOffset = glm::vec2(0.0f, 0.0f);
    glm::vec2 offsetToCenter = glm::vec2(0.0f, 0.0f);

    bool show_grid = true;
    bool snap_to_grid = false;
	
    string node_selected = "";
    glm::vec2 newNodeClickPos;
	std::map<string, int> nodesDrawingOrder;
    
    bool isCreatingConnection;
    ofxOceanodeAbstractParameter* tempSourceParameter = nullptr;
    ofxOceanodeAbstractParameter* tempSinkParameter = nullptr;
    ofxOceanodeAbstractConnection* duplicatedConnection = nullptr;
    
    string uniqueID;
    string parentID;
    bool isFirstDraw = true;
    bool onTop = false;
	
	// node dimensions (nodeWidthTotal should be % by 4)
	float NODE_SLOT_RADIUS = 4.0f;
	int NODE_WIDTH_TEXT = 90;
	int NODE_WIDTH_WIDGET = 150;
	int GRID_SIZE;
	int gridDivisions;
	glm::vec2 NODE_WINDOW_PADDING;
	
	// Comment checkbox system for keyboard shortcuts
	vector<bool> commentCheckboxStates;  // Track which comments have checkboxes enabled
	vector<int> keyboardSlots;           // Maps keyboard slots (0-9) to comment indices (-1 = empty)
	vector<int> commentToSlot;           // Maps comment indices to keyboard slots (-1 = not assigned)
	static const int MAX_KEYBOARD_SLOTS = 10;
};

#endif

#endif /* ofxOceanodeCanvas_h */
