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
        isPersistent = false;
    };
    
    ofxOceanodeAbstractConnection(ofAbstractParameter& _sourceParameter){
        sourceParameter = &_sourceParameter;
        sinkParameter = nullptr;
        isPersistent = false;
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
    
    ofxOceanodeConnectionGraphics& getGraphics(){return graphics;};
    
    glm::vec2 getPostion(int index){return graphics.getPoint(index);};
    void setTransformationMatrix(ofParameter<glm::mat4> *m){graphics.setTransformationMatrix(m);};
    
    ofAbstractParameter& getSourceParameter(){return *sourceParameter;};
    ofAbstractParameter& getSinkParameter(){return *sinkParameter;};
    
    bool getIsPersistent(){return isPersistent;};
    void setIsPersistent(bool p){isPersistent = p;};
        
    ofEvent<void> destroyConnection;
protected:
    ofxOceanodeConnectionGraphics graphics;
    
    ofAbstractParameter* sourceParameter;
    ofAbstractParameter* sinkParameter;
    
private:
    bool isPersistent;
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

template<typename Tsource, typename Tsink, typename Enable = void>
class ofxOceanodeConnection: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofParameter<Tsource>& pSource, ofParameter<Tsink>& pSink) : ofxOceanodeAbstractConnection(pSource, pSink), sourceParameter(pSource), sinkParameter(pSink){
        beforeConnectionValue = sinkParameter.get();
        linkParameters();
    }
    ~ofxOceanodeConnection(){
        sinkParameter.set(beforeConnectionValue);
        ofNotifyEvent(destroyConnection);
    };
    
private:
    void linkParameters(){
        parameterEventListener = sourceParameter.newListener([&](Tsource &p){
            sinkParameter = sourceParameter;
        });
        //sinkParameter = sourceParameter;
    }
    ofEventListener parameterEventListener;
    ofParameter<Tsource>& sourceParameter;
    ofParameter<Tsink>&  sinkParameter;
    Tsink beforeConnectionValue;
};

template<typename _Tsource, typename _Tsink>
class ofxOceanodeConnection<vector<_Tsource>, vector<_Tsink>, typename std::enable_if<!std::is_same<_Tsource, _Tsink>::value>::type>: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofParameter<vector<_Tsource>>& pSource, ofParameter<vector<_Tsink>>& pSink) : ofxOceanodeAbstractConnection(pSource, pSink), sourceParameter(pSource), sinkParameter(pSink){
        beforeConnectionValue = sinkParameter.get();
        parameterEventListener = sourceParameter.newListener([&](vector<_Tsource> &vf){
            //            sinkParameter = vector<_Tsink>(1, f);
            vector<_Tsink> vec(vf.size());
            for(int i = 0; i < vf.size(); i ++){
                vec[i] = vf[i];
            }
            sinkParameter = vec;
        });
        //sinkParameter = vector<T>(1, sourceParameter);
    }
    ~ofxOceanodeConnection(){
        sinkParameter.set(beforeConnectionValue);
        ofNotifyEvent(destroyConnection);
    };
    
private:
    ofEventListener parameterEventListener;
    ofParameter<vector<_Tsource>>& sourceParameter;
    ofParameter<vector<_Tsink>>&  sinkParameter;
    vector<_Tsink> beforeConnectionValue;
};

template<typename>
struct is_std_vector : std::false_type {};

template<typename T, typename A>
struct is_std_vector<std::vector<T,A>> : std::true_type {};

template<typename _Tsource, typename _Tsink>
class ofxOceanodeConnection<_Tsource, vector<_Tsink>, typename std::enable_if<!is_std_vector<_Tsource>::value>::type>: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofParameter<_Tsource>& pSource, ofParameter<vector<_Tsink>>& pSink) : ofxOceanodeAbstractConnection(pSource, pSink), sourceParameter(pSource), sinkParameter(pSink){
        beforeConnectionValue = sinkParameter.get();
        parameterEventListener = sourceParameter.newListener([&](_Tsource &f){
            sinkParameter = vector<_Tsink>(1, f);
        });
        //sinkParameter = vector<T>(1, sourceParameter);
    }
    ~ofxOceanodeConnection(){
        sinkParameter.set(beforeConnectionValue);
        ofNotifyEvent(destroyConnection);
    };
    
private:
    ofEventListener parameterEventListener;
    ofParameter<_Tsource>& sourceParameter;
    ofParameter<vector<_Tsink>>&  sinkParameter;
    vector<_Tsink> beforeConnectionValue;
};

template<typename _Tsource, typename _Tsink>
class ofxOceanodeConnection<vector<_Tsource>, _Tsink, typename std::enable_if<!is_std_vector<_Tsink>::value>::type>: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofParameter<vector<_Tsource>>& pSource, ofParameter<_Tsink>& pSink) : ofxOceanodeAbstractConnection(pSource, pSink), sourceParameter(pSource), sinkParameter(pSink){
        beforeConnectionValue = sinkParameter.get();
        parameterEventListener = sourceParameter.newListener([&](vector<_Tsource> &vf){
            if(vf.size() > 0){
                sinkParameter = vf[0];
            }
        });
//        if(sourceParameter.get().size() > 0){
//            sinkParameter = sourceParameter.get()[0];
//        }
    }
    ~ofxOceanodeConnection(){
        sinkParameter.set(beforeConnectionValue);
        ofNotifyEvent(destroyConnection);
    };
    
private:
    ofEventListener parameterEventListener;
    ofParameter<vector<_Tsource>>& sourceParameter;
    ofParameter<_Tsink>&  sinkParameter;
    _Tsink beforeConnectionValue;
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
