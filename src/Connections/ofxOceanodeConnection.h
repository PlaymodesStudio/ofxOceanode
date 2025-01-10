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
    ofxOceanodeAbstractConnection(ofxOceanodeAbstractParameter& _sourceParameter, ofxOceanodeAbstractParameter& _sinkParameter, bool _active = true){
        sourceParameter = &_sourceParameter;
        sinkParameter = &_sinkParameter;
        isPersistent = false;
        active = _active;
        sourceParameter->addOutConnection(this);
        sinkParameter->setInConnection(this);
    };

    virtual ~ofxOceanodeAbstractConnection(){
        sourceParameter->removeOutConnection(this);
        sinkParameter->removeInConnection(this);
    };
    
    void setActive(bool a){
        active = a;
        if(active)
        {
            if(dynamic_cast<ofxOceanodeParameter<void>*>(sourceParameter)==nullptr) passValueFunc();
        }
    }
    
    void deleteSelf(){
        destroyConnection.notify();
    }
    
    ofxOceanodeAbstractParameter& getSourceParameter(){return *sourceParameter;};
    ofxOceanodeAbstractParameter& getSinkParameter(){return *sinkParameter;};
    
    bool getIsPersistent(){return isPersistent;};
    void setIsPersistent(bool p){isPersistent = p;};
    
    ofEvent<void> destroyConnection;
protected:
    bool active;
    
    virtual void passValueFunc() = 0;
    virtual void restoreValue() = 0;
    
    ofxOceanodeAbstractParameter* sourceParameter;
    ofxOceanodeAbstractParameter* sinkParameter;
    
private:
    bool isPersistent;
};

template<typename T>
class ofxOceanodeCustomConnection : public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeCustomConnection(ofxOceanodeParameter<T>& pSource, ofxOceanodeAbstractParameter& pSink, bool _active) : ofxOceanodeAbstractConnection(pSource, pSink, _active), sourceParameter(pSource.getParameter()){
        parameterEventListener = pSource.getParameter().newListener([&](T &p){
            passValueFunc();
        });
        sinkParameter->connectedParameter();
        passValueFunc();
    }
    
    ~ofxOceanodeCustomConnection(){
        restoreValue();
    };
    
private:
    void passValueFunc(){
        if(active){
            sinkParameter->receiveParameter(&sourceParameter);
        }
    }
    void restoreValue(){
        sinkParameter->disconnectedParameter();
    }
    ofParameter<T>& sourceParameter;
    ofEventListener parameterEventListener;
};

template<>
class ofxOceanodeCustomConnection<void> : public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeCustomConnection(ofxOceanodeParameter<void>& pSource, ofxOceanodeAbstractParameter& pSink, bool _active) : ofxOceanodeAbstractConnection(pSource, pSink, _active), sourceParameter(pSource.getParameter()){
        parameterEventListener = pSource.getParameter().newListener([&](){
            passValueFunc();
        });
        sinkParameter->connectedParameter();
        passValueFunc();
    }
    
    ~ofxOceanodeCustomConnection(){
        restoreValue();
    };
    
private:
    void passValueFunc(){
        if(active){
            sinkParameter->receiveParameter(&sourceParameter);
        }
    }
    void restoreValue(){
        sinkParameter->disconnectedParameter();
    }
    ofParameter<void>& sourceParameter;
    ofEventListener parameterEventListener;
};

template<typename Tsource, typename Tsink, typename Enable = void>
class ofxOceanodeConnection: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofxOceanodeParameter<Tsource>& pSource, ofxOceanodeParameter<Tsink>& pSink, bool _active) : ofxOceanodeAbstractConnection(pSource, pSink, _active), sourceParameter(pSource.getParameter()), sinkParameter(pSink.getParameter()){
        beforeConnectionValue = sinkParameter.get();
        parameterEventListener = sourceParameter.newListener([&](Tsource &p){
            passValueFunc();
        });
        passValueFunc();
    }
    ~ofxOceanodeConnection(){
        restoreValue();
    };
    
private:
    void passValueFunc(){
        if(active){
            sinkParameter = sourceParameter;
        }
    }
    void restoreValue(){
        sinkParameter.set(beforeConnectionValue);
    }
    ofEventListener parameterEventListener;
    ofParameter<Tsource>& sourceParameter;
    ofParameter<Tsink>&  sinkParameter;
    Tsink beforeConnectionValue;
};

template<typename _Tsource, typename _Tsink>
class ofxOceanodeConnection<vector<_Tsource>, vector<_Tsink>, typename std::enable_if<!std::is_same<_Tsource, _Tsink>::value && (std::is_same<_Tsource, _Tsink>::value || std::is_convertible<_Tsource, _Tsink>::value)>::type>: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofxOceanodeParameter<vector<_Tsource>>& pSource, ofxOceanodeParameter<vector<_Tsink>>& pSink, bool _active) : ofxOceanodeAbstractConnection(pSource, pSink, _active), sourceParameter(pSource.getParameter()), sinkParameter(pSink.getParameter()){
        beforeConnectionValue = sinkParameter.get();
        parameterEventListener = sourceParameter.newListener([&](vector<_Tsource> &vf){
            passValueFunc();
        });
        passValueFunc();
    }
    ~ofxOceanodeConnection(){
        restoreValue();
    };
    
private:
    void passValueFunc(){
        if(active){
            sinkParameter = vector<_Tsink>(sourceParameter->begin(), sourceParameter->end());
        }
    }
    void restoreValue(){
        sinkParameter.set(beforeConnectionValue);
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
class ofxOceanodeConnection<_Tsource, vector<_Tsink>, typename std::enable_if<!std::is_same<_Tsource, void>::value && (std::is_same<_Tsource, _Tsink>::value || std::is_convertible<_Tsource, _Tsink>::value)>::type>: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofxOceanodeParameter<_Tsource>& pSource, ofxOceanodeParameter<vector<_Tsink>>& pSink, bool _active) : ofxOceanodeAbstractConnection(pSource, pSink, _active), sourceParameter(pSource.getParameter()), sinkParameter(pSink.getParameter()){
        beforeConnectionValue = sinkParameter.get();
        parameterEventListener = sourceParameter.newListener([&](_Tsource &f){
            passValueFunc();
        });
        passValueFunc();
    }
    ~ofxOceanodeConnection(){
        restoreValue();
    };
    
private:
    void passValueFunc(){
        if(active){
            sinkParameter = vector<_Tsink>(1, sourceParameter);
        }
    }
    void restoreValue(){
        sinkParameter.set(beforeConnectionValue);
    }
    ofEventListener parameterEventListener;
    ofParameter<_Tsource>& sourceParameter;
    ofParameter<vector<_Tsink>>&  sinkParameter;
    vector<_Tsink> beforeConnectionValue;
};

template<typename _Tsource, typename _Tsink>
class ofxOceanodeConnection<vector<_Tsource>, _Tsink, typename std::enable_if<std::is_same<_Tsource, _Tsink>::value || std::is_convertible<_Tsource, _Tsink>::value>::type>: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofxOceanodeParameter<vector<_Tsource>>& pSource, ofxOceanodeParameter<_Tsink>& pSink, bool _active) : ofxOceanodeAbstractConnection(pSource, pSink, _active), sourceParameter(pSource.getParameter()), sinkParameter(pSink.getParameter()){
        beforeConnectionValue = sinkParameter.get();
        parameterEventListener = sourceParameter.newListener([&](vector<_Tsource> &vf){
            passValueFunc();
        });
        passValueFunc();
    }
    ~ofxOceanodeConnection(){
        restoreValue();
    };
    
private:
    void passValueFunc(){
        if(active && sourceParameter->size() > 0){
            sinkParameter = sourceParameter.get()[0];
        }
    }
    void restoreValue(){
        sinkParameter.set(beforeConnectionValue);
    }
    ofEventListener parameterEventListener;
    ofParameter<vector<_Tsource>>& sourceParameter;
    ofParameter<_Tsink>&  sinkParameter;
    _Tsink beforeConnectionValue;
};


template<typename T>
class ofxOceanodeConnection<void, T>: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofxOceanodeParameter<void>& pSource, ofxOceanodeParameter<T>& pSink, bool _active) : ofxOceanodeAbstractConnection(pSource, pSink, _active), sourceParameter(pSource.getParameter()), sinkParameter(pSink.getParameter()){
        parameterEventListener = sourceParameter.newListener([&](){
            passValueFunc();
        });
    }
    ~ofxOceanodeConnection(){};
    
private:
    void passValueFunc(){
        if(active){
            sinkParameter = sinkParameter;
        }
    }
    void restoreValue(){
    }
    ofEventListener parameterEventListener;
    ofParameter<void>& sourceParameter;
    ofParameter<T>&  sinkParameter;
};

template<>
class ofxOceanodeConnection<void, void>: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofxOceanodeParameter<void>& pSource, ofxOceanodeParameter<void>& pSink, bool _active) : ofxOceanodeAbstractConnection(pSource, pSink, _active), sourceParameter(pSource.getParameter()), sinkParameter(pSink.getParameter()){
        parameterEventListener = sourceParameter.newListener([&](){
            passValueFunc();
        });
    }
    ~ofxOceanodeConnection(){};
    
private:
    void passValueFunc(){
        if(active){
            sinkParameter.trigger();
        }
    }
    void restoreValue(){
    }
    ofEventListener parameterEventListener;
    ofParameter<void>& sourceParameter;
    ofParameter<void>&  sinkParameter;
};

template<>
class ofxOceanodeConnection<void, bool>: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofxOceanodeParameter<void>& pSource, ofxOceanodeParameter<bool>& pSink, bool _active) : ofxOceanodeAbstractConnection(pSource, pSink, _active), sourceParameter(pSource.getParameter()), sinkParameter(pSink.getParameter()){
        parameterEventListener = sourceParameter.newListener([&](){
            passValueFunc();
        });
    }
    ~ofxOceanodeConnection(){};
    
private:
    void passValueFunc(){
        if(active){
            sinkParameter = !sinkParameter;
        }
    }
    void restoreValue(){
    }
    ofEventListener parameterEventListener;
    ofParameter<void>& sourceParameter;
    ofParameter<bool>&  sinkParameter;
};

template<>
class ofxOceanodeConnection<float, bool>: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofxOceanodeParameter<float>& pSource, ofxOceanodeParameter<bool>& pSink, bool _active) : ofxOceanodeAbstractConnection(pSource, pSink, _active), sourceParameter(pSource.getParameter()), sinkParameter(pSink.getParameter()){
        parameterEventListener = sourceParameter.newListener([&](float &f){
            passValueFunc();
        });
    }
    ~ofxOceanodeConnection(){};
    
private:
    void passValueFunc(){
        if(active){
            bool newValue = (sourceParameter > 0.00f) ? true : false;
            if(newValue != sinkParameter) sinkParameter = newValue;
        }
    }
    void restoreValue(){
    }
    ofEventListener parameterEventListener;
    ofParameter<float>& sourceParameter;
    ofParameter<bool>&  sinkParameter;
};

template<>
class ofxOceanodeConnection<vector<float>, bool>: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofxOceanodeParameter<vector<float>>& pSource, ofxOceanodeParameter<bool>& pSink, bool _active) : ofxOceanodeAbstractConnection(pSource, pSink, _active), sourceParameter(pSource.getParameter()), sinkParameter(pSink.getParameter()){
        parameterEventListener = sourceParameter.newListener([&](vector<float> &f){
            passValueFunc();
        });
    }
    ~ofxOceanodeConnection(){};
    
private:
    void passValueFunc(){
        if(active){
            if(sourceParameter->size() > 0){
                bool newValue = (sourceParameter->at(0) > 0.00f) ? true : false;
                if(newValue != sinkParameter) sinkParameter = newValue;
            }
        }
    }
    void restoreValue(){
    }
    ofEventListener parameterEventListener;
    ofParameter<vector<float>>& sourceParameter;
    ofParameter<bool>&  sinkParameter;
};


#endif /* ofxOceanodeConnection_h */
