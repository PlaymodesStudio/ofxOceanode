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
    
    
protected:
    bool isTemporalConnection;
    ofxOceanodeConnectionGraphics graphics;
};

template<typename T>
class ofxOceanodeTemporalConnection: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeTemporalConnection(){};
    ~ofxOceanodeTemporalConnection(){};
};

template<typename Tsource, typename Tsink>
class ofxOceanodeConnection: public ofxOceanodeAbstractConnection{
public:
    ofxOceanodeConnection(){};
    ~ofxOceanodeConnection(){};
};

#endif /* ofxOceanodeConnection_h */
