//
//  phasorClass.cpp
//  DGTL_Generator
//
//  Created by Eduard Frigola on 28/07/16.
//
//

#include "mapper.h"

mapper::mapper() : ofxOceanodeNodeModel("Mapper"){
    color = ofColor::white;
    parameters->add(input.set("Input", {0}, {0}, {1}));
    parameters->add(minInput.set("MinInput", 0, 0.0, 1.0));
    parameters->add(maxInput.set("MaxInput", 1.0, 0.0, 1.0));
    parameters->add(minOutput.set("MinOutput", 0, 0.0, 1.0));
    parameters->add(maxOutput.set("MaxOutput", 1.0, 0.0, 1.0));
    addOutputParameterToGroupAndInfo(output.set("Output", {0}, {0}, {1}));

    listeners.push_back(input.newListener([&](vector<float> &vf){
        recalculate();
    }));
    listeners.push_back(minInput.newListener([&](float &f){
        recalculate();
    }));
    listeners.push_back(maxInput.newListener([&](float &f){
        recalculate();
    }));
    listeners.push_back(minOutput.newListener([&](float &f){
        recalculate();
    }));
    listeners.push_back(maxOutput.newListener([&](float &f){
        recalculate();
    }));
}

void mapper::recalculate()
{
    vector<float> tempOut = input.get();
    for(auto &f : tempOut){
       f = ofMap(f,minInput,maxInput,minOutput,maxOutput, true);
    }
    output = tempOut;
}

