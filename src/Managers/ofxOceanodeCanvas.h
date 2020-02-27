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
#include "ofxImGui.h"

class ofxOceanodeContainer;
class ofxOceanodeNodeGui;
class ofxDatGuiDropdown;

class ofxOceanodeCanvas{
public:
    //ofxOceanodeCanvas(){};
    ~ofxOceanodeCanvas(){};
    
    
    void setup(std::shared_ptr<ofAppBaseWindow> window = ofGetCurrentWindow());
    void update(ofEventArgs &args){};
    void draw(ofEventArgs &args);
    
    void keyPressed(ofKeyEventArgs &e);
    void keyReleased(ofKeyEventArgs &e){};
    void mouseMoved(ofMouseEventArgs &e){};
    void mouseDragged(ofMouseEventArgs &e);
    void mousePressed(ofMouseEventArgs &e);
    void mouseReleased(ofMouseEventArgs &e);
    void mouseScrolled(ofMouseEventArgs &e);
    void mouseEntered(ofMouseEventArgs &e){};
    void mouseExited(ofMouseEventArgs &e){};
    
    void setUniqueID(string s){uniqueID = s;};
    
    void setContainer(shared_ptr<ofxOceanodeContainer> c){container = c;};
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
    bool selecting;
    bool entireSelect;
    ofRectangle selectedRect;
    glm::vec2 selectedRectIntialPosition;
    glm::vec2 dragModulesInitialPoint;
    vector<pair<ofxOceanodeNodeGui*, glm::vec2>> toMoveNodes;
    
    vector<string> categoriesVector;
    vector<vector<string>> options;
    string searchField = "";
    int numTimesPopup = 0;
    
    ofxImGui::Gui gui;
    bool inited = false;
    glm::vec2 scrolling = glm::vec2(0.0f, 0.0f);
    bool show_grid = true;
    string node_selected = "";
    bool isNodeDuplicated = false;
    glm::vec2 newNodeClickPos;
    
    
    string uniqueID;
    
};

#endif

#endif /* ofxOceanodeCanvas_h */
