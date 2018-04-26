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

ranger::ranger() : ofxOceanodeNodeModel("Ranger"){
    color = ofColor::white;
    parameters->add(Input.set("Input", {0}, {minRange}, {maxRange}));
    parameters->add(MinInput.set("MinInput", 0,  minRange, maxRange));
    parameters->add(MaxInput.set("MaxInput", 1.0,  minRange, maxRange));
    parameters->add(MinOutput.set("MinOutput", 0,  minRange, maxRange));
    parameters->add(MaxOutput.set("MaxOutput", 1.0,  minRange, maxRange));
    addOutputParameterToGroupAndInfo(Output.set("Output", {0}, {minRange}, {maxRange}));
    
    
    listeners.push(Input.newListener([&](vector<float> &vf){
        recalculate();
    }));
    listeners.push(MinInput.newListener([&](float &f){
        recalculate();
    }));
    listeners.push(MaxInput.newListener([&](float &f){
        recalculate();
    }));
    listeners.push(MinOutput.newListener([&](float &f){
        recalculate();
    }));
    listeners.push(MaxOutput.newListener([&](float &f){
        recalculate();
    }));
}

void ranger::recalculate()
{
    vector<float> vFloat;
    vFloat.resize(Input.get().size());
    for(int i = 0; i < Output.get().size(); i++){
        vFloat[i] = ofMap(Input.get()[i],MinInput,MaxInput,MinOutput,MaxOutput, true);
    }
    Output = vFloat;
}

