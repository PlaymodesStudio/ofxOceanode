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
    ofxOceanodeTemporalConnection(ofAbstractParameter& _p) : ofxOceanodeAbstractConnection(), p(_p){
        isTemporalConnection = true;
    }
    
    ofAbstractParameter& getParameter(){return p;};
    
    ~ofxOceanodeTemporalConnection(){};
    
private:
    ofAbstractParameter& p;
};

template<typename Tsource, typename Tsink>
class ofxOceanodeConnection: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofParameter<Tsource>& pSource, ofParameter<Tsink>& pSink) : ofxOceanodeAbstractConnection(), sourceParameter(pSource), sinkParameter(pSink){
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
