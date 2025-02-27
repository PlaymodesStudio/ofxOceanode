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
    description = "Same as mapper but for non normalized values";
    addParameter(input.set("Input", {0}, {0}, {1}));
    addParameter(minInput.set("Min In", {0}, {-FLT_MAX}, {FLT_MAX}));
    addParameter(maxInput.set("Max In", {1}, {-FLT_MAX}, {FLT_MAX}));
    addParameter(minOutput.set("Min Out", {0}, {-FLT_MAX}, {FLT_MAX}));
    addParameter(maxOutput.set("Max Out", {1}, {-FLT_MAX}, {FLT_MAX}));
    addOutputParameter(output.set("Output", {0}, {0}, {1}));
    
    listeners.push(input.newListener([&](vector<float> &vf){
        recalculate();
    }));
    listeners.push(minInput.newListener([&](vector<float> &f){
        recalculate();
        input.setMin(vector<float>(1, *std::max_element(f.begin(), f.end())));
    }));
    listeners.push(maxInput.newListener([&](vector<float> &f){
        recalculate();
        input.setMax(vector<float>(1, *std::max_element(f.begin(), f.end())));
    }));
    listeners.push(minOutput.newListener([&](vector<float> &f){
        recalculate();
        output.setMin(vector<float>(1, *std::max_element(f.begin(), f.end())));
    }));
    listeners.push(maxOutput.newListener([&](vector<float> &f){
        recalculate();
        output.setMax(vector<float>(1, *std::max_element(f.begin(), f.end())));
    }));
}

void ranger::recalculate()
{
	if(minInput.get().size()!=0 && maxInput.get().size()!=0 && minOutput.get().size()!=0 && maxOutput.get().size()!=0 && input.get().size()!=0 )
	{
		auto getElementFromIndex = [this](const vector<float> &f, int index) -> float{
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
}

