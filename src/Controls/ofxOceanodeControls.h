//
//  ofxOceanodeControls.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 12/03/2018.
//

#ifndef ofxOceanodeControls_h
#define ofxOceanodeControls_h

#include "ofMain.h"

class ofxOceanodeControls{
public:
    ofxOceanodeControls();
    ~ofxOceanodeControls(){};
    
    void draw(ofEventArgs &e);
    void keyPressed(ofKeyEventArgs &e){};
    void mouseMoved(ofMouseEventArgs &a);
    void mousePressed(ofMouseEventArgs &a);
    void mouseReleased(ofMouseEventArgs &a);
    void mouseDragged(ofMouseEventArgs &a);
    
private:
    std::shared_ptr<ofAppBaseWindow> controlsWindow;
};

#endif /* ofxOceanodeControls_h */
