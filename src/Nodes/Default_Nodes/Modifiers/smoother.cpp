//
//  smoother.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 22/03/2018.
//

#include "smoother.h"

void smoother::setup() {
    color = ofColor::green;
    description = "Uses moving average to smooth the signal with custom laggy tension implementation";
    addParameter(input.set("Input", {0}, {0}, {1}));
    addParameter(smoothing.set("Smooth", {0.5}, {0}, {1}));
    addParameter(tension.set("Tension", {0}, {-1}, {1}));
    addOutputParameter(output.set("Output", {0}, {0}, {1}));
    
    inputEventListener = input.newListener(this, &smoother::inputListener);
    
    isFirstInput = true;
}

void smoother::update(ofEventArgs &a){
	vector<float> vf = input.get();
	if(isFirstInput){
		output = vf;
		previousInput = vf;
		isFirstInput = false;
	}else{
		if(previousInput.size() != vf.size()) previousInput = vf;
		vector<float> newOutput(vf.size());
		for(int i = 0; i < vf.size(); i++){
			// Guard: if either input is NaN, reset and pass through
			if(std::isnan(vf[i]) || std::isnan(previousInput[i])){
				newOutput[i] = std::isnan(vf[i]) ? 0.0f : vf[i];
				previousInput[i] = newOutput[i];
				continue;
			}

			float smoothingValue = smoothing.get().size() == 1 ? smoothing.get()[0] : smoothing.get()[i];
			float newSmoothing = smoothingValue;
			float tensionValue = tension.get().size() == 1 ? tension.get()[0] : tension.get()[i];
			float step = abs(previousInput[i] - vf[i]);

			if(tensionValue == 0){
				newSmoothing = smoothingValue;
			}else if(tensionValue > 0.5){
				tensionValue = ofMap(tensionValue, .5, 1, 0, .5);
				newSmoothing = ofLerp(smoothingValue, 0, (step * tensionValue));
			}else if(tensionValue <= 0.5){
				tensionValue = pow(ofMap(tensionValue, -1, .5, 1, 0), 0.2);
				newSmoothing = ofLerp(smoothingValue, .999999, (step * tensionValue));
			}

			float candidate = (newSmoothing * previousInput[i]) + ((1 - newSmoothing) * vf[i]);

			// Guard: if result is still NaN (e.g. bad smoothing value), fall back to input
			newOutput[i] = std::isnan(candidate) ? vf[i] : candidate;
		}
		output = newOutput;
		previousInput = newOutput;
	}
}

