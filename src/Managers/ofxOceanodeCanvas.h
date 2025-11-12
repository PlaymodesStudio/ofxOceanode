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
    
private:
    glm::vec3 getMatrixScale(const glm::mat4 &m);
    glm::mat4 translateMatrixWithoutScale(const glm::mat4 &m, glm::vec3 translationVector);
    
    glm::vec2 screenToCanvas(glm::vec2 p);
    glm::vec2 canvasToScreen(glm::vec2 p);
    
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
    int numTimesPopup = 0;
    
    bool inited = false;
    
    glm::vec2 scrolling = glm::vec2(0.0f, 0.0f);
    glm::vec2 scrollBeforeHover = glm::vec2(0.0f, 0.0f);
    bool returnToOldScrolling=false;
    int itemHovered=-1;
    
    glm::vec2 scrollingOffset = glm::vec2(0.0f, 0.0f);
    glm::vec2 offsetToCenter = glm::vec2(0.0f, 0.0f);

    bool show_grid = true;
    
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
    
};

#endif

#endif /* ofxOceanodeCanvas_h */
