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
    parameters->add(Input.set("Input", 0, minRange, maxRange));
    parameters->add(MinInput.set("MinInput", 0,  minRange, maxRange));
    parameters->add(MaxInput.set("MaxInput", 1.0,  minRange, maxRange));
    parameters->add(MinOutput.set("MinOutput", 0,  minRange, maxRange));
    parameters->add(MaxOutput.set("MaxOutput", 1.0,  minRange, maxRange));
    parameters->add(Output.set("Output", 0, minRange, maxRange));
    
    
    Input.addListener(this, &ranger::recalculate);
    MinInput.addListener(this, &ranger::recalculate);
    MaxInput.addListener(this, &ranger::recalculate);
    MinOutput.addListener(this, &ranger::recalculate);
    MaxOutput.addListener(this, &ranger::recalculate);
}

//float ranger::getRange(){
//    float f;
//    recalculate(f);
//    return(paramOutput);
//}

void ranger::recalculate(float& f)
{
    Output = ofMap(Input,MinInput,MaxInput,MinOutput,MaxOutput, true);
}
    
//void ranger::resetRange()
//{
//    paramInput = 0.0;
//    MinInput = 0.0;
//    MaxInput = 1.0;
//    MinOutput = 0.0;
//    MaxOutput = 1.0;
//}

