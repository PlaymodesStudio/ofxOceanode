//
//  ofxOceanodeConnection.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 22/02/2018.
//

#ifndef ofxOceanodeConnection_h
#define ofxOceanodeConnection_h

#include "ofMain.h"
#include "ofxOceanodeParameter.h"

class ofxOceanodeAbstractConnection{
public:
    ofxOceanodeAbstractConnection(ofxOceanodeAbstractParameter& _sourceParameter, ofxOceanodeAbstractParameter& _sinkParameter){
        sourceParameter = &_sourceParameter;
        sinkParameter = &_sinkParameter;
        isPersistent = false;
        active = true;
    };

    virtual ~ofxOceanodeAbstractConnection(){};
    
    void setActive(bool a){
        active = a;
    }
    
    ofxOceanodeAbstractParameter& getSourceParameter(){return *sourceParameter;};
    ofxOceanodeAbstractParameter& getSinkParameter(){return *sinkParameter;};
    
    bool getIsPersistent(){return isPersistent;};
    void setIsPersistent(bool p){isPersistent = p;};
    
    ofEvent<void> destroyConnection;
protected:
    bool active;
    
    ofxOceanodeAbstractParameter* sourceParameter;
    ofxOceanodeAbstractParameter* sinkParameter;
    
private:
    bool isPersistent;
};

template<typename Tsource, typename Tsink, typename Enable = void>
class ofxOceanodeConnection: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofxOceanodeParameter<Tsource>& pSource, ofxOceanodeParameter<Tsink>& pSink) : ofxOceanodeAbstractConnection(pSource, pSink), sourceParameter(pSource.getParameter()), sinkParameter(pSink.getParameter()){
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
            if(active)
                sinkParameter = sourceParameter;
        });
        sinkParameter = sourceParameter;
    }
    ofEventListener parameterEventListener;
    ofParameter<Tsource>& sourceParameter;
    ofParameter<Tsink>&  sinkParameter;
    Tsink beforeConnectionValue;
};

template<typename _Tsource, typename _Tsink>
class ofxOceanodeConnection<vector<_Tsource>, vector<_Tsink>, typename std::enable_if<!std::is_same<_Tsource, _Tsink>::value>::type>: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofxOceanodeParameter<vector<_Tsource>>& pSource, ofxOceanodeParameter<vector<_Tsink>>& pSink) : ofxOceanodeAbstractConnection(pSource, pSink), sourceParameter(pSource.getParameter()), sinkParameter(pSink.getParameter()){
        beforeConnectionValue = sinkParameter.get();
        parameterEventListener = sourceParameter.newListener([&](vector<_Tsource> &vf){
            if(active){
                sinkParameter = vector<_Tsink>(vf.begin(), vf.end());
            }
        });
        sinkParameter = vector<_Tsink>(sourceParameter->begin(), sourceParameter->end());
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
    ofxOceanodeConnection(ofxOceanodeParameter<_Tsource>& pSource, ofxOceanodeParameter<vector<_Tsink>>& pSink) : ofxOceanodeAbstractConnection(pSource, pSink), sourceParameter(pSource.getParameter()), sinkParameter(pSink.getParameter()){
        beforeConnectionValue = sinkParameter.get();
        parameterEventListener = sourceParameter.newListener([&](_Tsource &f){
            if(active)
                sinkParameter = vector<_Tsink>(1, f);
        });
        sinkParameter = vector<_Tsink>(1, sourceParameter);
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
    ofxOceanodeConnection(ofxOceanodeParameter<vector<_Tsource>>& pSource, ofxOceanodeParameter<_Tsink>& pSink) : ofxOceanodeAbstractConnection(pSource, pSink), sourceParameter(pSource.getParameter()), sinkParameter(pSink.getParameter()){
        beforeConnectionValue = sinkParameter.get();
        parameterEventListener = sourceParameter.newListener([&](vector<_Tsource> &vf){
            if(active){
                if(vf.size() > 0){
                    sinkParameter = vf[0];
                }
            }
        });
        if(sourceParameter->size() > 0){
            sinkParameter = sourceParameter.get()[0];
        }
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
    ofxOceanodeConnection(ofxOceanodeParameter<void>& pSource, ofxOceanodeParameter<T>& pSink) : ofxOceanodeAbstractConnection(pSource, pSink), sourceParameter(pSource.getParameter()), sinkParameter(pSink.getParameter()){
        linkParameters();
    }
    ~ofxOceanodeConnection(){
        ofNotifyEvent(destroyConnection);
    };
    
private:
    void linkParameters(){
        parameterEventListener = sourceParameter.newListener([&](){
            if(active)
                sinkParameter = sinkParameter;
        });
    }
    
    ofEventListener parameterEventListener;
    ofParameter<void>& sourceParameter;
    ofParameter<T>&  sinkParameter;
};

template<>
class ofxOceanodeConnection<void, void>: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofxOceanodeParameter<void>& pSource, ofxOceanodeParameter<void>& pSink) : ofxOceanodeAbstractConnection(pSource, pSink), sourceParameter(pSource.getParameter()), sinkParameter(pSink.getParameter()){
        linkParameters();
    }
    ~ofxOceanodeConnection(){
        ofNotifyEvent(destroyConnection);
    };
    
private:
    void linkParameters(){
        parameterEventListener = sourceParameter.newListener([&](){
            if(active)
                sinkParameter.trigger();
        });
    }
    
    ofEventListener parameterEventListener;
    ofParameter<void>& sourceParameter;
    ofParameter<void>&  sinkParameter;
};

template<>
class ofxOceanodeConnection<void, bool>: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofxOceanodeParameter<void>& pSource, ofxOceanodeParameter<bool>& pSink) : ofxOceanodeAbstractConnection(pSource, pSink), sourceParameter(pSource.getParameter()), sinkParameter(pSink.getParameter()){
        linkParameters();
    }
    ~ofxOceanodeConnection(){
        ofNotifyEvent(destroyConnection);
    };
    
private:
    void linkParameters(){
        parameterEventListener = sourceParameter.newListener([&](){
            if(active)
                sinkParameter = !sinkParameter;
        });
    }
    
    ofEventListener parameterEventListener;
    ofParameter<void>& sourceParameter;
    ofParameter<bool>&  sinkParameter;
};

template<>
class ofxOceanodeConnection<float, bool>: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofxOceanodeParameter<float>& pSource, ofxOceanodeParameter<bool>& pSink) : ofxOceanodeAbstractConnection(pSource, pSink), sourceParameter(pSource.getParameter()), sinkParameter(pSink.getParameter()){
        linkParameters();
    }
    ~ofxOceanodeConnection(){
        ofNotifyEvent(destroyConnection);
    };
    
private:
    void linkParameters(){
        parameterEventListener = sourceParameter.newListener([&](float &f){
            if(active){
                bool newValue = (f > ((sourceParameter.getMax() - sourceParameter.getMin())/2.0 + sourceParameter.getMin())) ? true : false;
                if(newValue != sinkParameter) sinkParameter = newValue;
            }
        });
    }
    
    ofEventListener parameterEventListener;
    ofParameter<float>& sourceParameter;
    ofParameter<bool>&  sinkParameter;
};



#endif /* ofxOceanodeConnection_h */
