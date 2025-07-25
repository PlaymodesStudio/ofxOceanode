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
class ofxOceanodeAbstractParameter;

#include "ofEvents.h"
#include "ofParameter.h"
#include "ofAppBaseWindow.h"

using namespace std;

class ofxOceanodeNodeGui{
public:
    ofxOceanodeNodeGui(ofxOceanodeContainer &container, ofxOceanodeNode &node);
    ~ofxOceanodeNodeGui();
    
    void update(ofEventArgs &e){};
    void draw(ofEventArgs &e){};
    
    bool constructGui(int nodeWidthText, int nodeWidthWidget);
    
    void setPosition(glm::vec2 position);
    void setSize(glm::vec2 size);
    
    ofParameterGroup &getParameters();
    glm::vec2 getPosition();
    ofRectangle getRectangle();
    
    void enable();
    void disable();
    
    ofColor getColor(){return color;};
    
    bool getExpanded(){return expanded;};
    void setExpanded(bool e){expanded = e;};
    
    
    void setSelected(bool b){selected = b;};
    bool getSelected(){return selected;};
    
    void setVisibility(bool b){visible = b;};
    bool getVisibility(){return visible;};
    
    glm::vec2 getSourceConnectionPositionFromParameter(ofxOceanodeAbstractParameter& parameter);
    glm::vec2 getSinkConnectionPositionFromParameter(ofxOceanodeAbstractParameter& parameter);
    
#ifdef OFXOCEANODE_USE_MIDI
    void setIsListeningMidi(bool b){isListeningMidi = b;};
#endif
    
private:
    
    ofEventListeners keyAndMouseListeners;
    
    ofxOceanodeContainer& container;
    
    ofxOceanodeNode& node;
    
    vector<glm::vec2> inputPositions;
    vector<glm::vec2> outputPositions;
    
    ofColor color;
    ofRectangle guiRect;
    
    bool guiToBeDestroyed;
    bool lastExpandedState;
    bool isGuiCreated;
    
    bool expanded;
    bool selected = false;
    bool visible;
    
    bool valueHasBeenReseted = false;
    
#ifdef OFXOCEANODE_USE_MIDI
    bool isListeningMidi;
#endif
};

#endif

#endif /* ofxOceanodeNodeGui_h */
