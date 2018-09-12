//
//  phasorClass.cpp
//  DGTL_Generator
//
//  Created by Eduard Frigola on 28/07/16.
//
//

#include "switcher.h"

switcher::switcher() : ofxOceanodeNodeModel("Switcher Float"){
    color = ofColor::white;
    parameters->add(switchSelector.set("Switch", 0, 0, 1));
    parameters->add(input1.set("Input 1", {0}, {0}, {1}));
    parameters->add(input2.set("Input 2", {0}, {0}, {1}));
    addOutputParameterToGroupAndInfo(output.set("Output", {0}, {0}, {1}));

    listeners.push(input1.newListener([&](vector<float> &vf){
        changedInput(vf);
    }));
    listeners.push(input2.newListener([&](vector<float> &vf){
        changedInput(vf);
    }));
}

void switcher::changedSwitch(int &i)
{
    
}
void switcher::changedInput(vector<float> &vf)
{
    if(switchSelector==0)
    {
        output = input1;
    }
    else if(switchSelector==1)
    {
        output = input2;
    }
}

//void switcher::recalculate()
//{
//    vector<float> tempOut = input.get();
//    for(auto &f : tempOut){
//       f = ofMap(f,minInput,maxInput,minOutput,maxOutput, true);
//    }
//    output = tempOut;
//}

