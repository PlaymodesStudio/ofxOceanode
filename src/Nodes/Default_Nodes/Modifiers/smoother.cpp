//
//  smoother.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 22/03/2018.
//

#include "smoother.h"

smoother::smoother() : ofxOceanodeNodeModel("Smoother"){
    color = ofColor::azure;
    parameters->add(input.set("Input", {0}, {0}, {1}));
    parameters->add(smoothing.set("Smoothing", 0.5, 0, 1));
    parameters->add(tension.set("Tension", 0, -1, 1));
    parameters->add(output.set("Output", {0}, {0}, {1}));
    
    inputEventListener = input.newListener(this, &smoother::inputListener);
}

void smoother::inputListener(vector<float> &vf){
    if(previousValue.size() != vf.size()) previousValue = vf;
    vector<float> vecCopy = vf;
    
    for(int i = 0; i < vecCopy.size(); i++){
        float newSmoothing = smoothing;
        if(tension > 0)
            newSmoothing = ofClamp(smoothing * (1 - (abs(previousValue[i] - vf[i]) * tension)), 0, 1);
        else if(tension < 0)
            newSmoothing = ofClamp(smoothing * (1 - ((1 - abs(previousValue[i] - vf[i])) * abs(tension))), 0, 1);
        
        vecCopy[i] = (newSmoothing * previousValue[i]) + ((1 - newSmoothing) * vf[i]);
    }
    output = vecCopy;
    previousValue = vecCopy;
}
