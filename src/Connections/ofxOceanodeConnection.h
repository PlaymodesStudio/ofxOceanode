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
    
    void setSourcePosition(glm::vec2 posVec){
        graphics.setPoint(0, posVec);
    }
    
    void setSinkPosition(glm::vec2 posVec){
        graphics.setPoint(1, posVec);
    }
    
    void moveSourcePosition(glm::vec2 moveVec){
        graphics.movePoint(0, moveVec);
    }
    
    void moveSinkPosition(glm::vec2 moveVec){
        graphics.movePoint(1, moveVec);
    }
    
    glm::vec2 getPostion(int index){return graphics.getPoint(index);};
    void setTransformationMatrix(ofParameter<glm::mat4> *m){graphics.setTransformationMatrix(m);};
    
    ofAbstractParameter& getSourceParameter(){return *sourceParameter;};
    ofAbstractParameter& getSinkParameter(){return *sinkParameter;};
        
    ofEvent<void> destroyConnection;
protected:
    ofxOceanodeConnectionGraphics graphics;
    
    ofAbstractParameter* sourceParameter;
    ofAbstractParameter* sinkParameter;
};

class ofxOceanodeNode;

class ofxOceanodeTemporalConnection: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeTemporalConnection(ofAbstractParameter& _p) : ofxOceanodeAbstractConnection(_p){
        graphics.setPoint(0, glm::vec2(ofGetMouseX(), ofGetMouseY()));
        graphics.setPoint(1, glm::vec2(ofGetMouseX(), ofGetMouseY()));
        
        mouseDraggedListener = ofEvents().mouseDragged.newListener([&](ofMouseEventArgs & mouse){
            graphics.setPoint(1, glm::inverse(graphics.getTransformationMatrix()) * glm::vec4(mouse, 0, 1));
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
    ofxOceanodeConnection(ofParameter<Tsource>& pSource, ofParameter<Tsink>& pSink) : ofxOceanodeAbstractConnection(pSource, pSink), sourceParameter(pSource), sinkParameter(pSink){
        linkParameters();
    }
    ~ofxOceanodeConnection(){
        ofNotifyEvent(destroyConnection);
    };
    
private:
    void linkParameters(){
        parameterEventListener = sourceParameter.newListener([&](Tsource &p){
            sinkParameter = sourceParameter;
        });
        sinkParameter = sourceParameter;
    }
    ofEventListener parameterEventListener;
    ofParameter<Tsource>& sourceParameter;
    ofParameter<Tsink>&  sinkParameter;
};

template<typename T>
class ofxOceanodeConnection<T, vector<T>>: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofParameter<T>& pSource, ofParameter<vector<T>>& pSink) : ofxOceanodeAbstractConnection(pSource, pSink), sourceParameter(pSource), sinkParameter(pSink){
        parameterEventListener = sourceParameter.newListener([&](T &f){
            sinkParameter = vector<T>(1, f);
        });
        sinkParameter = vector<T>(1, sourceParameter);
    }
    ~ofxOceanodeConnection(){
        ofNotifyEvent(destroyConnection);
    };
    
private:
    ofEventListener parameterEventListener;
    ofParameter<T>& sourceParameter;
    ofParameter<vector<T>>&  sinkParameter;
};

template<typename T>
class ofxOceanodeConnection<vector<T>, T>: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofParameter<vector<T>>& pSource, ofParameter<T>& pSink) : ofxOceanodeAbstractConnection(pSource, pSink), sourceParameter(pSource), sinkParameter(pSink){
        parameterEventListener = sourceParameter.newListener([&](vector<T> &vf){
            if(vf.size() > 0){
                sinkParameter = vf[0];
            }
        });
        if(sourceParameter.get().size() > 0){
            sinkParameter = sourceParameter.get()[0];
        }
    }
    ~ofxOceanodeConnection(){
        ofNotifyEvent(destroyConnection);
    };
    
private:
    ofEventListener parameterEventListener;
    ofParameter<vector<T>>& sourceParameter;
    ofParameter<T>&  sinkParameter;
};


template<typename T>
class ofxOceanodeConnection<void, T>: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofParameter<void>& pSource, ofParameter<T>& pSink) : ofxOceanodeAbstractConnection(pSource, pSink), sourceParameter(pSource), sinkParameter(pSink){
        linkParameters();
    }
    ~ofxOceanodeConnection(){
        ofNotifyEvent(destroyConnection);
    };
    
private:
    void linkParameters(){
        parameterEventListener = sourceParameter.newListener([&](){
            sinkParameter = sinkParameter;
        });
    }
    
    ofEventListener parameterEventListener;
    ofParameter<void>& sourceParameter;
    ofParameter<T>&  sinkParameter;
};



#endif /* ofxOceanodeConnection_h */
