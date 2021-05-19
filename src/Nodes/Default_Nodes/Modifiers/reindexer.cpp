//
//  reindexer.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 22/03/2018.
//

#include "reindexer.h"

void reindexer::setup(){
    color = ofColor::orange;
    addParameter(input.set("Input", {0}, {0}, {1}));
	addParameter(indexs.set("Indexs", {0}, {0}, {INT_MAX}));
    addOutputParameter(output.set("Output", {0}, {0}, {1}));
    
	eventListeners.push(input.newListener(this, &reindexer::calculateReindex));
	eventListeners.push(indexs.newListener(this, &reindexer::calculateReindex));
	
	alreadyCalculated = false;
}

void reindexer::update(ofEventArgs &a){
	alreadyCalculated = false;
}

void reindexer::calculateReindex(vector<float> &vf){
	if(!alreadyCalculated){
		if(indexs->size() < 2) return;
		tempOutput.resize(indexs->size());
		bool normIndexs = *max_element(indexs.get().begin(), indexs.get().end()) <= 1;
		for(int i = 0; i < tempOutput.size(); i++){
			float floatIndex = indexs.get()[i];
			if(normIndexs) floatIndex = indexs.get()[i]*(input->size()-1);
			floatIndex = ofClamp(floatIndex, 0, input->size()-1);
			tempOutput[i] = ofLerp(input->at(floor(floatIndex)), input->at(ceil(floatIndex)), fmod(floatIndex, 1.0f));
		}
		output = tempOutput;
	}
	alreadyCalculated = true;
}
