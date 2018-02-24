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

class ofxOceanodeTemporalConnection: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeTemporalConnection(ofAbstractParameter& p) : ofxOceanodeAbstractConnection(), sourceParameter(p){
        isTemporalConnection = true;
    }
    ~ofxOceanodeTemporalConnection(){};

    ofAbstractParameter& getParameter(){return sourceParameter;};
    
private:
    ofAbstractParameter& sourceParameter;
};

template<typename Tsource, typename Tsink>
class ofxOceanodeConnection: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(ofxOceanodeTemporalConnection& c, ofParameter<Tsink>& p) : ofxOceanodeAbstractConnection(), sourceParameter(c.getParameter().cast<Tsource>()), sinkParameter(p){
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
