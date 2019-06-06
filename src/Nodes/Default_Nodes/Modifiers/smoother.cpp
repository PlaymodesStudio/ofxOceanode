//
//  smoother.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 22/03/2018.
//

#include "smoother.h"

void smoother::setup() {
    color = ofColor::azure;
    parameters->add(input.set("Input", {0}, {0}, {1}));
    parameters->add(smoothing.set("Smoothing", {0.5}, {0}, {1}));
    parameters->add(tension.set("Tension", {0}, {-1}, {1}));
    addOutputParameterToGroupAndInfo(output.set("Output", {0}, {0}, {1}));
    
    inputEventListener = input.newListener(this, &smoother::inputListener);
    
    isFirstInput = true;
}

void smoother::inputListener(vector<float> &vf){
    if(isFirstInput){
        output = vf;
        previousInput = vf;
        isFirstInput = false;
    }else{
        if(previousInput.size() != vf.size()) previousInput = vf;
        vector<float> newOutput(vf.size());
        for(int i = 0; i < vf.size(); i++){
            float smoothingValue = smoothing.get().size() == 1 ? smoothing.get()[0] : smoothing.get()[i];
            float newSmoothing = smoothingValue;
            float tensionValue = tension.get().size() == 1 ? tension.get()[0] : tension.get()[i];
            if(tensionValue > 0)
                newSmoothing = ofClamp(smoothingValue * (1 - (abs(previousInput[i] - vf[i]) * tensionValue)), 0, 1);
            else if(tensionValue < 0)
                newSmoothing = ofClamp(smoothingValue * (1 - ((1 - abs(previousInput[i] - vf[i])) * abs(tensionValue))), 0, 1);
            
            newOutput[i] = (newSmoothing * previousInput[i]) + ((1 - newSmoothing) * vf[i]);
        }
        output = newOutput;
        previousInput = newOutput;
    }
}
