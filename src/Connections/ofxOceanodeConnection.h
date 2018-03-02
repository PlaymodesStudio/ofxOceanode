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
    ofxOceanodeAbstractConnection(ofAbstractParameter& _sourceParameter, ofAbstractParameter& _sinkParameter){
        sourceParameter = &_sourceParameter;
        sinkParameter = &_sinkParameter;
    };
    
    ofxOceanodeAbstractConnection(ofAbstractParameter& _sourceParameter){
        sourceParameter = &_sourceParameter;
        sinkParameter = nullptr;
    };
    virtual ~ofxOceanodeAbstractConnection(){};
    
    void moveSourcePoint(glm::vec2 moveVec){
        graphics.movePoint(0, moveVec);
    }
    
    void moveSinkePoint(glm::vec2 moveVec){
        graphics.movePoint(1, moveVec);
    }
    
    glm::vec2 getPostion(int index){return graphics.getPoint(index);};
    
    ofAbstractParameter& getSourceParameter(){return *sourceParameter;};
    ofAbstractParameter& getSinkParameter(){return *sinkParameter;};
    
    bool getIsTemporalConnection(){return isTemporalConnection;};
    
    ofEvent<void> destroyConnection;
protected:
    bool isTemporalConnection;
    ofxOceanodeConnectionGraphics graphics;
    
    ofAbstractParameter* sourceParameter;
    ofAbstractParameter* sinkParameter;
};

class ofxOceanodeNode;

class ofxOceanodeTemporalConnection: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeTemporalConnection(ofAbstractParameter& _p, glm::vec2 pos) : ofxOceanodeAbstractConnection(_p){
        isTemporalConnection = true;
        graphics.setPoint(0, pos);
        graphics.setPoint(1, glm::vec2(ofGetMouseX(), ofGetMouseY()));
        
        mouseDraggedListener = ofEvents().mouseDragged.newListener([&](ofMouseEventArgs & mouse){
            graphics.setPoint(1, mouse);
        });
        mouseReleasedListener = ofEvents().mouseReleased.newListener([&](ofMouseEventArgs & mouse){
            graphics.deactivate();
            ofNotifyEvent(destroyConnection);
        });
    }
    ~ofxOceanodeTemporalConnection(){};
    
    glm::vec2 getSourcePosition(){return graphics.getPoint(0);};
    
private:
    ofEventListener mouseDraggedListener;
    ofEventListener mouseReleasedListener;
};

template<typename Tsource, typename Tsink>
class ofxOceanodeConnection: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofParameter<Tsource>& pSource, ofParameter<Tsink>& pSink, glm::vec2 posSource, glm::vec2 posSink) : ofxOceanodeAbstractConnection(pSource, pSink), sourceParameter(pSource), sinkParameter(pSink){
        parameterEventListener = sourceParameter.newListener(this, &ofxOceanodeConnection::parameterListener);
        sinkParameter = sourceParameter;
        isTemporalConnection = false;
        graphics.setPoint(0, posSource);
        graphics.setPoint(1, posSink);
    }
    ~ofxOceanodeConnection(){
        ofNotifyEvent(destroyConnection);
    };
    
private:
    void parameterListener(Tsource &value){
        if(typeid(Tsource).name() == typeid(Tsink).name()){
            sinkParameter = sourceParameter;
        }
    }
    ofEventListener parameterEventListener;
    
    ofParameter<Tsource>& sourceParameter;
    ofParameter<Tsink>&  sinkParameter;
};

#endif /* ofxOceanodeConnection_h */
