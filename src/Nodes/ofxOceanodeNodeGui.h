//
//  ofxOceanodeNodeGui.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 22/02/2018.
//

#ifndef ofxOceanodeNodeGui_h
#define ofxOceanodeNodeGui_h

#ifndef OFXOCEANODE_HEADLESS

class ofxOceanodeContainer;
class ofxOceanodeNode;

#include "ofEvents.h"
#include "ofParameterGroup.h"
#include "ofAppBaseWindow.h"

using namespace std;

class ofxOceanodeNodeGui{
public:
    ofxOceanodeNodeGui(ofxOceanodeContainer &container, ofxOceanodeNode &node, shared_ptr<ofAppBaseWindow> window);
    ~ofxOceanodeNodeGui();
    
    void update(ofEventArgs &e){};
    void draw(ofEventArgs &e){};
    
    bool constructGui();
    
    void setPosition(glm::vec2 position);
    void setSize(glm::vec2 size);
    
    shared_ptr<ofParameterGroup> getParameters();
    glm::vec2 getPosition();
    ofRectangle getRectangle();
    
    void setWindow(shared_ptr<ofAppBaseWindow> window);
    
    void enable();
    void disable();
    
    void duplicate();
    
    glm::vec2 getSourceConnectionPositionFromParameter(ofAbstractParameter& parameter);
    glm::vec2 getSinkConnectionPositionFromParameter(ofAbstractParameter& parameter);
    
#ifdef OFXOCEANODE_USE_MIDI
    void setIsListeningMidi(bool b){isListeningMidi = b;};
#endif
    
//    void keyPressed(ofKeyEventArgs &args);
//    void keyReleased(ofKeyEventArgs &args);
//    void mouseMoved(ofMouseEventArgs &args){};
//    void mouseDragged(ofMouseEventArgs &args);
//    void mousePressed(ofMouseEventArgs &args);
//    void mouseReleased(ofMouseEventArgs &args);
//    void mouseScrolled(ofMouseEventArgs &args){};
//    void mouseEntered(ofMouseEventArgs &args){};
//    void mouseExited(ofMouseEventArgs &args){};
    
private:
    
    ofEventListeners keyAndMouseListeners;
    
    ofxOceanodeContainer& container;
    
    ofxOceanodeNode& node;
    
    map<string, glm::vec2> inputPositions;
    map<string, glm::vec2> outputPositions;
    
    ofColor color;
    ofRectangle guiRect;
    
    bool guiToBeDestroyed;
    bool lastExpandedState;
    bool isGuiCreated;
    
#ifdef OFXOCEANODE_USE_MIDI
    bool isListeningMidi;
#endif
};

#endif

#endif /* ofxOceanodeNodeGui_h */
