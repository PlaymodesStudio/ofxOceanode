//
//  ofxOceanodeConnection.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 22/02/2018.
//

#ifndef ofxOceanodeConnection_h
#define ofxOceanodeConnection_h

#include "ofMain.h"
#include "ofxOceanodeConnectionGraphics.h"

class ofxOceanodeAbstractConnection{
public:
    ofxOceanodeAbstractConnection(){};
    ~ofxOceanodeAbstractConnection(){};
    
    bool getIsTemporalConnection(){return isTemporalConnection;};
protected:
    bool isTemporalConnection;
    ofxOceanodeConnectionGraphics graphics;
};

class ofxOceanodeNode;

class ofxOceanodeTemporalConnection: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeTemporalConnection(ofAbstractParameter& _p, glm::vec2 pos) : ofxOceanodeAbstractConnection(), p(_p){
        isTemporalConnection = true;
        graphics.movePoint(0, pos);
        
        ofRegisterMouseEvents(this);
    }
    ~ofxOceanodeTemporalConnection(){
        ofUnregisterMouseEvents(this);
    };
    
    ofAbstractParameter& getParameter(){return p;};
    
    glm::vec2 getSourcePosition(){return graphics.getPoint(0);};
    
    void mouseMoved(ofMouseEventArgs &args){};
    void mouseDragged(ofMouseEventArgs &args){
        graphics.movePoint(1, glm::vec2(args.x, args.y));
    }
    void mousePressed(ofMouseEventArgs &args){};
    void mouseReleased(ofMouseEventArgs &args){};
    void mouseScrolled(ofMouseEventArgs &args){};
    void mouseEntered(ofMouseEventArgs &args){};
    void mouseExited(ofMouseEventArgs &args){};
    
private:
    
    ofAbstractParameter& p;
};

template<typename Tsource, typename Tsink>
class ofxOceanodeConnection: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofParameter<Tsource>& pSource, ofParameter<Tsink>& pSink, glm::vec2 posSource, glm::vec2 posSink) : ofxOceanodeAbstractConnection(), sourceParameter(pSource), sinkParameter(pSink){
        sourceParameter.addListener(this, &ofxOceanodeConnection::parameterListener);
        sinkParameter = sourceParameter;
        isTemporalConnection = false;
        graphics.movePoint(0, posSource);
        graphics.movePoint(1, posSink);
    }
    ~ofxOceanodeConnection(){};
    
private:
    void parameterListener(Tsource &value){
        if(typeid(Tsource).name() == typeid(Tsink).name()){
            sinkParameter = sourceParameter;
        }
    }
    
    ofParameter<Tsource>& sourceParameter;
    ofParameter<Tsink>&  sinkParameter;
};

#endif /* ofxOceanodeConnection_h */
