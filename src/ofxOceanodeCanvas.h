//
//  ofxOceanodeCanvas.h
//  example-basic
//
//  Created by Eduard Frigola on 19/06/2017.
//
//

#ifndef ofxOceanodeCanvas_h
#define ofxOceanodeCanvas_h

#include "ofMain.h"
#include "ofxDatGui.h"
#include "ofxOceanodeContainer.h"

class ofxOceanodeCanvas{
public:
    //ofxOceanodeCanvas(){};
    ~ofxOceanodeCanvas(){};
    
    
    void setup();
    void update(ofEventArgs &args){};
    void draw(ofEventArgs &args){};
    
    void keyPressed(ofKeyEventArgs &e){};
    void keyReleased(ofKeyEventArgs &e){};
    void mouseMoved(ofMouseEventArgs &e){};
    void mouseDragged(ofMouseEventArgs &e){};
    void mousePressed(ofMouseEventArgs &e);
    void mouseReleased(ofMouseEventArgs &e){};
    void mouseScrolled(ofMouseEventArgs &e){};
    void mouseEntered(ofMouseEventArgs &e){};
    void mouseExited(ofMouseEventArgs &e){};
    
    void setContainer(ofxOceanodeContainer* c){container = c;};
    
    
    void newModuleListener(ofxDatGuiDropdownEvent e);
private:
    
    ofxOceanodeContainer* container;
    
    ofxDatGui *popUpMenu;
    
};

#endif /* ofxOceanodeCanvas_h */
