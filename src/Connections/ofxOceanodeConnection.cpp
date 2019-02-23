//
//  ofxOceanodeConnection.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 22/02/2018.
//

#include "ofxOceanodeConnection.h"

//template<>
//void ofxOceanodeConnection<void, void>::linkParameters(){
//    parameterEventListener = sourceParameter.newListener([&](){
//        sinkParameter.trigger();
//    });
//    sinkParameter.trigger();
//}
//
//template<>
//void ofxOceanodeConnection<void, bool>::linkParameters(){
//    parameterEventListener = sourceParameter.newListener([&](){
//        sinkParameter = !sinkParameter;
//    });
//}
//
//template<>
//void ofxOceanodeConnection<float, bool>::linkParameters(){
//    parameterEventListener = sourceParameter.newListener([&](float &f){
//        bool newValue = (f > ((sourceParameter.getMax() - sourceParameter.getMin())/2.0 + sourceParameter.getMin())) ? true : false;
//        if(newValue != sinkParameter) sinkParameter = newValue;
//    });
//}

