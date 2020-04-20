//
//  ofxOceanodeConnection.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 22/02/2018.
//

#ifndef ofxOceanodeConnection_h
#define ofxOceanodeConnection_h

#include "ofMain.h"

class ofxOceanodeAbstractConnection{
public:
    ofxOceanodeAbstractConnection(ofAbstractParameter& _sourceParameter, ofAbstractParameter& _sinkParameter, bool _active = true){
        sourceParameter = &_sourceParameter;
        sinkParameter = &_sinkParameter;
        isPersistent = false;
        active = _active;
    };
    
    ofxOceanodeAbstractConnection(ofAbstractParameter& _sourceParameter){
        sourceParameter = &_sourceParameter;
        sinkParameter = nullptr;
        isPersistent = false;
    };
    virtual ~ofxOceanodeAbstractConnection(){};
    
    void setActive(bool a){
        active = a;
        if(active) passValueFunc();
    }
    
    ofAbstractParameter& getSourceParameter(){return *sourceParameter;};
    ofAbstractParameter& getSinkParameter(){return *sinkParameter;};
    
    bool getIsPersistent(){return isPersistent;};
    void setIsPersistent(bool p){isPersistent = p;};
    
    ofEvent<void> destroyConnection;
protected:
    bool active;
    
    virtual void passValueFunc() = 0;
    
    ofAbstractParameter* sourceParameter;
    ofAbstractParameter* sinkParameter;
    
private:
    bool isPersistent;
};

template<typename Tsource, typename Tsink, typename Enable = void>
class ofxOceanodeConnection: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofParameter<Tsource>& pSource, ofParameter<Tsink>& pSink, bool _active) : ofxOceanodeAbstractConnection(pSource, pSink, _active), sourceParameter(pSource), sinkParameter(pSink){
        beforeConnectionValue = sinkParameter.get();
        parameterEventListener = sourceParameter.newListener([&](Tsource &p){
            passValueFunc();
        });
        passValueFunc();
    }
    ~ofxOceanodeConnection(){
        sinkParameter.set(beforeConnectionValue);
        ofNotifyEvent(destroyConnection);
    };
    
private:
    void passValueFunc(){
        if(active){
            sinkParameter = sourceParameter;
        }
    }
    ofEventListener parameterEventListener;
    ofParameter<Tsource>& sourceParameter;
    ofParameter<Tsink>&  sinkParameter;
    Tsink beforeConnectionValue;
};

template<typename _Tsource, typename _Tsink>
class ofxOceanodeConnection<vector<_Tsource>, vector<_Tsink>, typename std::enable_if<!std::is_same<_Tsource, _Tsink>::value>::type>: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofParameter<vector<_Tsource>>& pSource, ofParameter<vector<_Tsink>>& pSink, bool _active) : ofxOceanodeAbstractConnection(pSource, pSink, _active), sourceParameter(pSource), sinkParameter(pSink){
        beforeConnectionValue = sinkParameter.get();
        parameterEventListener = sourceParameter.newListener([&](vector<_Tsource> &vf){
            passValueFunc();
        });
        passValueFunc();
    }
    ~ofxOceanodeConnection(){
        sinkParameter.set(beforeConnectionValue);
        ofNotifyEvent(destroyConnection);
    };
    
private:
    void passValueFunc(){
        if(active){
            sinkParameter = vector<_Tsink>(sourceParameter->begin(), sourceParameter->end());
        }
    }
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
    ofxOceanodeConnection(ofParameter<_Tsource>& pSource, ofParameter<vector<_Tsink>>& pSink, bool _active) : ofxOceanodeAbstractConnection(pSource, pSink, _active), sourceParameter(pSource), sinkParameter(pSink){
        beforeConnectionValue = sinkParameter.get();
        parameterEventListener = sourceParameter.newListener([&](_Tsource &f){
            passValueFunc();
        });
        passValueFunc();
    }
    ~ofxOceanodeConnection(){
        sinkParameter.set(beforeConnectionValue);
        ofNotifyEvent(destroyConnection);
    };
    
private:
    void passValueFunc(){
        if(active){
            sinkParameter = vector<_Tsink>(1, sourceParameter);
        }
    }
    ofEventListener parameterEventListener;
    ofParameter<_Tsource>& sourceParameter;
    ofParameter<vector<_Tsink>>&  sinkParameter;
    vector<_Tsink> beforeConnectionValue;
};

template<typename _Tsource, typename _Tsink>
class ofxOceanodeConnection<vector<_Tsource>, _Tsink, typename std::enable_if<!is_std_vector<_Tsink>::value>::type>: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofParameter<vector<_Tsource>>& pSource, ofParameter<_Tsink>& pSink, bool _active) : ofxOceanodeAbstractConnection(pSource, pSink, _active), sourceParameter(pSource), sinkParameter(pSink){
        beforeConnectionValue = sinkParameter.get();
        parameterEventListener = sourceParameter.newListener([&](vector<_Tsource> &vf){
            passValueFunc();
        });
        passValueFunc();
    }
    ~ofxOceanodeConnection(){
        sinkParameter.set(beforeConnectionValue);
        ofNotifyEvent(destroyConnection);
    };
    
private:
    void passValueFunc(){
        if(active && sourceParameter->size() > 0){
            sinkParameter = sourceParameter.get()[0];
        }
    }
    ofEventListener parameterEventListener;
    ofParameter<vector<_Tsource>>& sourceParameter;
    ofParameter<_Tsink>&  sinkParameter;
    _Tsink beforeConnectionValue;
};


template<typename T>
class ofxOceanodeConnection<void, T>: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofParameter<void>& pSource, ofParameter<T>& pSink, bool _active) : ofxOceanodeAbstractConnection(pSource, pSink, _active), sourceParameter(pSource), sinkParameter(pSink){
        parameterEventListener = sourceParameter.newListener([&](){
            passValueFunc();
        });
    }
    ~ofxOceanodeConnection(){
        ofNotifyEvent(destroyConnection);
    };
    
private:
    void passValueFunc(){
        if(active){
            sinkParameter = sinkParameter;
        }
    }
    ofEventListener parameterEventListener;
    ofParameter<void>& sourceParameter;
    ofParameter<T>&  sinkParameter;
};

template<>
class ofxOceanodeConnection<void, void>: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofParameter<void>& pSource, ofParameter<void>& pSink, bool _active) : ofxOceanodeAbstractConnection(pSource, pSink, _active), sourceParameter(pSource), sinkParameter(pSink){
        parameterEventListener = sourceParameter.newListener([&](){
            passValueFunc();
        });
    }
    ~ofxOceanodeConnection(){
        ofNotifyEvent(destroyConnection);
    };
    
private:
    void passValueFunc(){
        if(active){
            sinkParameter.trigger();
        }
    }
    ofEventListener parameterEventListener;
    ofParameter<void>& sourceParameter;
    ofParameter<void>&  sinkParameter;
};

template<>
class ofxOceanodeConnection<void, bool>: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofParameter<void>& pSource, ofParameter<bool>& pSink, bool _active) : ofxOceanodeAbstractConnection(pSource, pSink, _active), sourceParameter(pSource), sinkParameter(pSink){
        parameterEventListener = sourceParameter.newListener([&](){
            passValueFunc();
        });
    }
    ~ofxOceanodeConnection(){
        ofNotifyEvent(destroyConnection);
    };
    
private:
    void passValueFunc(){
        if(active){
            sinkParameter = !sinkParameter;
        }
    }
    ofEventListener parameterEventListener;
    ofParameter<void>& sourceParameter;
    ofParameter<bool>&  sinkParameter;
};

template<>
class ofxOceanodeConnection<float, bool>: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofParameter<float>& pSource, ofParameter<bool>& pSink, bool _active) : ofxOceanodeAbstractConnection(pSource, pSink, _active), sourceParameter(pSource), sinkParameter(pSink){
        parameterEventListener = sourceParameter.newListener([&](float &f){
            passValueFunc();
        });
    }
    ~ofxOceanodeConnection(){
        ofNotifyEvent(destroyConnection);
    };
    
private:
    void passValueFunc(){
        if(active){
            bool newValue = (sourceParameter > ((sourceParameter.getMax() - sourceParameter.getMin())/2.0 + sourceParameter.getMin())) ? true : false;
            if(newValue != sinkParameter) sinkParameter = newValue;
        }
    }
    ofEventListener parameterEventListener;
    ofParameter<float>& sourceParameter;
    ofParameter<bool>&  sinkParameter;
};



#endif /* ofxOceanodeConnection_h */
