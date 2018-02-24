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
    ofxOceanodeAbstractConnection(ofAbstractParameter& p) : parameter(&p){
        isTemporalConnection = true;
    };
    ~ofxOceanodeAbstractConnection(){};
    
    ofAbstractParameter& getParameter(){return *parameter;};
    
    bool getIsTemporalConnection(){return isTemporalConnection;};
protected:
    bool isTemporalConnection;
    ofAbstractParameter* parameter;
    ofxOceanodeConnectionGraphics graphics;
};

//template<typename T>
//class ofxOceanodeTemporalConnection: public ofxOceanodeAbstractConnection{
//public:
//    ofxOceanodeTemporalConnection(ofParameter<T>& p) : ofxOceanodeAbstractConnection(), parameter(p){
//        isTemporalConnection = true;
//    }
//    ~ofxOceanodeTemporalConnection(){};
//
//private:
//    ofParameter<T>& parameter;
//};

template<typename Tsource, typename Tsink>
class ofxOceanodeConnection: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofxOceanodeAbstractConnection& c, ofParameter<Tsink>& p) : ofxOceanodeAbstractConnection(), sourceParameter(c.getParameter().cast<Tsource>()), sinkParameter(p){
        sourceParameter.addListener(this, &ofxOceanodeConnection::parameterListener);
        sinkParameter = sourceParameter;
        isTemporalConnection = false;
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
