//
//  phasorClass.cpp
//  DGTL_Generator
//
//  Created by Eduard Frigola on 28/07/16.
//
//

#include "mapper.h"

mapper::mapper() : ofxOceanodeNodeModel("Mapper"){
    parameters->add(Input.set("Input", 0, 0.0, 1.0));
    parameters->add(MinInput.set("MinInput", 0, 0.0, 1.0));
    parameters->add(MaxInput.set("MaxInput", 1.0, 0.0, 1.0));
    parameters->add(MinOutput.set("MinOutput", 0, 0.0, 1.0));
    parameters->add(MaxOutput.set("MaxOutput", 1.0, 0.0, 1.0));
    parameters->add(Output.set("Output", 0, 0.0, 1.0));

    
    Input.addListener(this, &mapper::recalculate);
//    MinInput.addListener(this, &mapper::recalculate);
//    MaxInput.addListener(this, &mapper::recalculate);
//    MinOutput.addListener(this, &mapper::recalculate);
//    MaxOutput.addListener(this, &mapper::recalculate);
}

float mapper::getRange(){
    float f;
    recalculate(f);
    return(Output);
}

void mapper::recalculate(float& f)
{
    Output = ofMap(Input,MinInput,MaxInput,MinOutput,MaxOutput, true);
}
    
void mapper::resetRange()
{
    Input = 0.0;
    MinInput = 0.0;
    MaxInput = 1.0;
    MinOutput = 0.0;
    MaxOutput = 1.0;
}

