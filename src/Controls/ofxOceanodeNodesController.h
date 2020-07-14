//
//  ofxOceanodeNodesController.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 13/03/2018.
//

#ifndef ofxOceanodeNodesController_h
#define ofxOceanodeNodesController_h

#include "ofxOceanodeBaseController.h"


class ofxOceanodeNodesController: public ofxOceanodeBaseController{
public:

    ofxOceanodeNodesController(shared_ptr<ofxOceanodeContainer> _container, ofxOceanodeCanvas* _canvas);
    ~ofxOceanodeNodesController(){};
    
    void draw();
    
private:
    
//    float bpm;
//    ofEventListener bpmListener;
//    ofParameter<float> phase;
//    ofEventListener phaseListener;
    
    string searchField = "";
    string searchFieldMyNodes = "";
    vector<string> categoriesVector;
    vector<vector<string>> options;
    glm::vec2 newNodeClickPos;


    shared_ptr<ofxOceanodeContainer> container;
    ofxOceanodeCanvas* canvas;
};


#endif /* ofxOceanodeNodesController_h */
