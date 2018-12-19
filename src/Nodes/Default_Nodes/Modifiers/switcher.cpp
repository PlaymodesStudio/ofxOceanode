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
        changedInput();
    }));
    listeners.push(input2.newListener([&](vector<float> &vf){
        changedInput();
    }));
    listeners.push(switchSelector.newListener([this](float &i){
        changedInput();
    }));
}

void switcher::changedSwitch(int &i)
{
    
}
void switcher::changedInput()
{
    if(input1->size() == input2->size()){
        vector<float> tempOutput(input1->size());
        for(int i = 0; i < tempOutput.size(); i++){
            tempOutput[i] = (input1->at(i)*(1-switchSelector)) + (input2->at(i)*switchSelector);
        }
        output = tempOutput;
    }
    else{
        if(switchSelector < 0.5)
        {
            output = input1;
        }
        else if(switchSelector >= 0.5)
        {
            output = input2;
        }
    }
}

