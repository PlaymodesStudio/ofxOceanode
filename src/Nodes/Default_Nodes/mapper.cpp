//
//  phasorClass.cpp
//  DGTL_Generator
//
//  Created by Eduard Frigola on 28/07/16.
//
//

#include "mapper.h"

mapper::mapper() : ofxOceanodeNodeModel("Mapper"){
    parameters->add(input.set("Input", {0}, {0}, {1}));
    parameters->add(minInput.set("MinInput", 0, 0.0, 1.0));
    parameters->add(maxInput.set("MaxInput", 1.0, 0.0, 1.0));
    parameters->add(minOutput.set("MinOutput", 0, 0.0, 1.0));
    parameters->add(maxOutput.set("MaxOutput", 1.0, 0.0, 1.0));
    parameters->add(output.set("Output", {0}, {0}, {1}));

    
    input.addListener(this, &mapper::recalculate);
//    MinInput.addListener(this, &mapper::recalculate);
//    MaxInput.addListener(this, &mapper::recalculate);
//    MinOutput.addListener(this, &mapper::recalculate);
//    MaxOutput.addListener(this, &mapper::recalculate);
}

void mapper::recalculate(vector<float>& vf)
{
    vector<float> tempOut = vf;
    for(auto &f : tempOut){
       f = ofMap(f,minInput,maxInput,minOutput,maxOutput, true);
    }
    output = tempOut;
}

