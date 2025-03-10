//
//  phasorClass.cpp
//  DGTL_Generator
//
//  Created by Eduard Frigola on 28/07/16.
//
//

#include "mapper.h"

void mapper::setup() {
    color = ofColor::white;
    description = "Simple map() function. Maps input range to output range";
    addParameter(input.set("Input", {0}, {0}, {1}));
    addParameter(minInput.set("Min In", {0}, {0}, {1}));
    addParameter(maxInput.set("Max In", {1}, {0}, {1}));
    addParameter(minOutput.set("Min Out", {0}, {0}, {1}));
    addParameter(maxOutput.set("Max Out", {1}, {0}, {1}));
    addOutputParameter(output.set("Output", {0}, {0}, {1}));

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

void mapper::recalculate()
{
	if(minInput.get().size()!=0 && maxInput.get().size()!=0 && minOutput.get().size()!=0 && maxOutput.get().size()!=0 && input.get().size()!=0 )
	{
		auto getElementFromIndex = [this](const vector<float> &f, int index) -> float{
				if(index < f.size() ){
					return f[index];
				}else{
					return f[0];
				}
		};
    
		vector<float> tempOut(max({input->size(), minInput->size(), maxInput->size(), minOutput->size(), maxOutput->size()}), 0);
		for(int i = 0; i < tempOut.size(); i++){
			if(getElementFromIndex(minInput, i) == getElementFromIndex(maxInput, i)){
				tempOut[i] = getElementFromIndex(input, i) < getElementFromIndex(minInput, i) ? getElementFromIndex(minOutput, i) : getElementFromIndex(maxOutput, i);
			}else{
				tempOut[i] = ofMap(getElementFromIndex(input, i), getElementFromIndex(minInput, i), getElementFromIndex(maxInput, i), getElementFromIndex(minOutput, i), getElementFromIndex(maxOutput, i), true);
			}
		}
		output = tempOut;
	}
	
}

