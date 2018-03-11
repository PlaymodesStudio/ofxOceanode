//
//  ofxOceanodeConnection.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 22/02/2018.
//

#include "ofxOceanodeConnection.h"

template<>
void ofxOceanodeConnection<void, void>::linkParameters(){
    parameterEventListener = sourceParameter.newListener([&](){
        sinkParameter.trigger();
    });
    sinkParameter.trigger();
}

template<>
void ofxOceanodeConnection<void, bool>::linkParameters(){
    parameterEventListener = sourceParameter.newListener([&](){
        sinkParameter = !sinkParameter;
    });
}
