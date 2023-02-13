//
//  reindexer.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 22/03/2018.
//

#include "reindexer.h"

void reindexer::setup(){
    color = ofColor::orange;
    addInspectorParameter(normIndexs.set("Normalized Indexs", true));
    addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
	addParameter(indexs.set("Indexs", {0}, {0}, {FLT_MAX}));
    addOutputParameter(output.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));
    
    addInspectorParameter(mode.set("Mode 0-Normal / 1-Inverse", 0, 0, 1));
    
	eventListeners.push(input.newListener(this, &reindexer::calculateReindex));
	//eventListeners.push(indexs.newListener(this, &reindexer::calculateReindex));
	
	//alreadyCalculated = false;
}

void reindexer::update(ofEventArgs &a){
	//alreadyCalculated = false;
}

void reindexer::calculateReindex(vector<float> &vf){
	if(mode == 0){
		if(indexs->size() < 2 || input->size() < 2) return;
		tempOutput.resize(indexs->size());
		for(int i = 0; i < tempOutput.size(); i++){
			float floatIndex = indexs.get()[i];
			if(normIndexs) floatIndex = indexs.get()[i]*(input->size()-1);
			floatIndex = ofClamp(floatIndex, 0, input->size()-1);
			tempOutput[i] = ofLerp(input->at(floor(floatIndex)), input->at(ceil(floatIndex)), fmod(floatIndex, 1.0f));
		}
		output = tempOutput;
    }else{
        if(indexs->size() < 2 || indexs->size() != input->size()) return;
        tempOutput.resize(input->size());
        for(int i = 0; i < tempOutput.size(); i++){
            float floatIndex = indexs.get()[i];
            if(normIndexs) floatIndex = indexs.get()[i]*(input->size()-1);
            floatIndex = ofClamp(floatIndex, 0, input->size()-1);
            tempOutput[i] = ofLerp(input->at(floor(floatIndex)), input->at(ceil(floatIndex)), fmod(floatIndex, 1.0f));
        }
        output = tempOutput;
    }
	//alreadyCalculated = true;
}
