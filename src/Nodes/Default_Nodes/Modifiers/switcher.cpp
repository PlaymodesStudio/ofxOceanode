//
//  phasorClass.cpp
//  DGTL_Generator
//
//  Created by Eduard Frigola on 28/07/16.
//
//

#include "switcher.h"

void switcher::setup(){
    color = ofColor::white;
    description = "Select one of the inputs to send to the output (with linear interpolation)";
	
	addInspectorParameter(numInputs.set("Num Inputs", 5, 2, INT_MAX));
    addInspectorParameter(normalizedSelector.set("Norm Selector", true));
    addParameter(switchSelector.set("Switch", {0}, {0}, {FLT_MAX}));
	addOutputParameter(output.set("Output", {0}, {0}, {1}));
	inputs.resize(numInputs);
	for(int i = 0; i < numInputs; i++){
		addParameter(inputs[i].set("In " + ofToString(i), {0}, {0}, {1}));
	}
	
	listeners.push(numInputs.newListener([this](int &i){
		if(inputs.size() != i){
			int oldSize = inputs.size();
			bool remove = oldSize > i;
			
			inputs.resize(i);
			
			if(remove){
				for(int j = oldSize-1; j >= i; j--){
					removeParameter("In " + ofToString(j));
				}
			}else{
				for(int j = oldSize; j < i; j++){
					addParameter(inputs[j].set("In " + ofToString(j), {0}, {0}, {1}));
				}
			}
		}
	}));
}

void switcher::update(ofEventArgs &a)
{
//	int maxSize = std::max_element(inputs.begin(), inputs.end(), [](ofParameter<vector<float>> &pv1, ofParameter<vector<float>> &pv2){
//		return pv1->size() < pv2->size();
//	})->get().size();
	
	auto getValueForPosition([](ofParameter<vector<float>> &p, int pos) -> float{
        if(pos < p->size()){
            return p->at(pos);
        }
        return p->at(0);
    });
	
	vector<float> fswitch = switchSelector;
    
    if(!normalizedSelector){
        for(int i = 0; i < fswitch.size(); i++){
            fswitch[i] /= (numInputs-1);
        }
    }
    
    for(int i = 0; i < fswitch.size(); i++){
        fswitch[i] = ofClamp(fswitch[i], 0, 1);
    }
	
	std::transform(fswitch.begin(),
				   fswitch.end(),
				   fswitch.begin(),
				   [this](float& c){
		return c*(numInputs-1);
	});
	
	vector<float> finterpol = fswitch;
	
	std::transform(finterpol.begin(),
				   finterpol.end(),
				   finterpol.begin(),
				   [](float& c){
		return c - ((std::trunc(c) == c) ? std::trunc(c)-1 : std::trunc(c));
	});
	
//	float fswitch = switchSelector*(numInputs-1);
//	float finterpol = fswitch - ((std::trunc(fswitch) == fswitch) ? std::trunc(fswitch)-1 : std::trunc(fswitch));
	
	if(fswitch.size() == 1){
		if(inputs[floor(fswitch[0])]->size() == inputs[ceil(fswitch[0])]->size()){
			vector<float> tempOutput(inputs[floor(fswitch[0])]->size());
			for(int i = 0; i < tempOutput.size(); i++){
				tempOutput[i] = (inputs[floor(fswitch[0])]->at(i)*(1-finterpol[0])) + (inputs[ceil(fswitch[0])]->at(i)*finterpol[0]);
			}
			output = tempOutput;
		}
		else{
			if(finterpol[0] < 0.5)
			{
				output = inputs[floor(fswitch[0])];
			}
			else if(finterpol[0] >= 0.5)
			{
				output = inputs[ceil(fswitch[0])];
			}
		}
	}
	else{
		vector<float> tempOutput(fswitch.size());
		for(int i = 0; i < fswitch.size(); i++){
			tempOutput[i] = (getValueForPosition(inputs[floor(fswitch[i])], i)*(1-finterpol[i])) + (getValueForPosition(inputs[ceil(fswitch[i])], i)*finterpol[i]);
		}
		output = tempOutput;
	}
}
