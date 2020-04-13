//
//  phasorClass.cpp
//  DGTL_Generator
//
//  Created by Eduard Frigola on 28/07/16.
//
//
float minRange = -10000.000;
float maxRange =  10000.000;

#include "ranger.h"

void ranger::setup(){
    color = ofColor::white;
    parameters->add(input.set("Input", {0}, {minRange}, {maxRange}));
    parameters->add(minInput.set("Min In", {0}, {minRange}, {maxRange}));
    parameters->add(maxInput.set("Max In", {1}, {minRange}, {maxRange}));
    parameters->add(minOutput.set("Min Out", {0}, {minRange}, {maxRange}));
    parameters->add(maxOutput.set("Max Out", {1}, {minRange}, {maxRange}));
    addOutputParameterToGroupAndInfo(output.set("Output", {0}, {minRange}, {maxRange}));
    
    
    listeners.push(input.newListener([&](vector<float> &vf){
        recalculate();
    }));
    listeners.push(minInput.newListener([&](vector<float> &f){
        recalculate();
    }));
    listeners.push(maxInput.newListener([&](vector<float> &f){
        recalculate();
    }));
    listeners.push(minOutput.newListener([&](vector<float> &f){
        recalculate();
    }));
    listeners.push(maxOutput.newListener([&](vector<float> &f){
        recalculate();
    }));
}

void ranger::recalculate()
{
    auto getElementFromIndex = [this](const vector<float> f, int index) -> float{
        if(index < f.size()){
            return f[index];
        }else{
            return f[0];
        }
    };
    
    
    vector<float> tempOut(max({input->size(), minInput->size(), maxInput->size(), minOutput->size(), maxOutput->size()}), 0);
    for(int i = 0; i < tempOut.size(); i++){
        tempOut[i] = ofMap(getElementFromIndex(input, i), getElementFromIndex(minInput, i), getElementFromIndex(maxInput, i), getElementFromIndex(minOutput, i), getElementFromIndex(maxOutput, i), true);
    }
    output = tempOut;
}

