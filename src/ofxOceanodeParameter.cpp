//
//  ofxOceanodeParameter.cpp
//  example-basic
//
//  Created by Eduard Frigola on 29/04/2020.
//

#include "ofxOceanodeParameter.h"
#include "ofMath.h"
#include "ofxOceanodeConnection.h"
#include "ofxOceanodeScope.h"

void ofxOceanodeAbstractParameter::removeAllConnections(){
    if(inConnection != nullptr) inConnection->deleteSelf();
    for(auto c : outConnections) c->deleteSelf();
    
    if(hasScope) ofxOceanodeScope::getInstance()->removeParameter(this);
};

template<>
void ofxOceanodeParameter<float>::registerDragFunctions(){
	normalDrag = [](ofParameter<float> &p, int drag){
		p = ofClamp(p + (0.001f*drag), p.getMin(), p.getMax());
	};
	precisionDrag = [](ofParameter<float> &p, int drag){
		p = ofClamp(p + (0.0001f*drag), p.getMin(), p.getMax());
	};
	speedDrag = [](ofParameter<float> &p, int drag){
		p = ofClamp(p + (0.01f*drag), p.getMin(), p.getMax());
	};
}

template<>
void ofxOceanodeParameter<std::vector<float>>::registerDragFunctions(){
	parameter = nullptr;
	normalDrag = [](ofParameter<std::vector<float>> &p, int drag){
		if(p->size() == 1)
			p = std::vector<float>(1, ofClamp(p->at(0) + (0.001f*drag), p.getMin()[0], p.getMax()[0]));
	};
	precisionDrag = [](ofParameter<std::vector<float>> &p, int drag){
		if(p->size() == 1)
			p = std::vector<float>(1, ofClamp(p->at(0) + (0.0001f*drag), p.getMin()[0], p.getMax()[0]));
	};
	speedDrag = [](ofParameter<std::vector<float>> &p, int drag){
		if(p->size() == 1)
			p = std::vector<float>(1, ofClamp(p->at(0) + (0.01f*drag), p.getMin()[0], p.getMax()[0]));
	};
}

template<>
void ofxOceanodeParameter<int>::registerDragFunctions(){
	parameter = nullptr;
	normalDrag = [](ofParameter<int> &p, int drag){
		p = ofClamp(p + (5*drag), p.getMin(), p.getMax());
	};
	precisionDrag = [](ofParameter<int> &p, int drag){
		p = ofClamp(p + (1*drag), p.getMin(), p.getMax());
	};
	speedDrag = [](ofParameter<int> &p, int drag){
		p = ofClamp(p + (10*drag), p.getMin(), p.getMax());
	};
}

template<>
void ofxOceanodeParameter<std::vector<int>>::registerDragFunctions(){
	parameter = nullptr;
	normalDrag = [](ofParameter<std::vector<int>> &p, int drag){
		if(p->size() == 1)
			p = std::vector<int>(1, ofClamp(p->at(0) + (5*drag), p.getMin()[0], p.getMax()[0]));
	};
	precisionDrag = [](ofParameter<std::vector<int>> &p, int drag){
		if(p->size() == 1)
			p = std::vector<int>(1, ofClamp(p->at(0) + (1*drag), p.getMin()[0], p.getMax()[0]));
	};
	speedDrag = [](ofParameter<std::vector<int>> &p, int drag){
		if(p->size() == 1)
			p = std::vector<int>(1, ofClamp(p->at(0) + (10*drag), p.getMin()[0], p.getMax()[0]));
	};
}

template<>
void ofxOceanodeParameter<bool>::registerDragFunctions(){
	parameter = nullptr;
	normalDrag = [](ofParameter<bool> &p, int drag){
		if(drag > 0 && !p) p = true;
		else if(drag < 0 && p) p = false;
	};
//	precisionDrag = [](ofParameter<bool> &p, int drag){
//		p = ofClamp(p + (1*drag), p.getMin(), p.getMax());
//	};
//	speedDrag = [](ofParameter<bool> &p, int drag){
//		p = ofClamp(p + (10*drag), p.getMin(), p.getMax());
//	};
}

template<>
void ofxOceanodeParameter<ofColor>::registerDragFunctions(){
	parameter = nullptr;
	normalDrag = [](ofParameter<ofColor> &p, int drag){
		int brightness = p->getBrightness() + (5 * drag);
		brightness = ofClamp(brightness, 0, 255);
		ofColor tempColor = p;
		tempColor.setBrightness(brightness);
		p = tempColor;
	};
	precisionDrag = [](ofParameter<ofColor> &p, int drag){
		int brightness = p->getBrightness() + (1 * drag);
		brightness = ofClamp(brightness, 0, 255);
		ofColor tempColor = p;
		tempColor.setBrightness(brightness);
		p = tempColor;
	};
	speedDrag = [](ofParameter<ofColor> &p, int drag){
		int brightness = p->getBrightness() + (10 * drag);
		brightness = ofClamp(brightness, 0, 255);
		ofColor tempColor = p;
		tempColor.setBrightness(brightness);
		p = tempColor;
	};
}

template<>
void ofxOceanodeParameter<ofFloatColor>::registerDragFunctions(){
	parameter = nullptr;
	normalDrag = [](ofParameter<ofFloatColor> &p, int drag){
		float brightness = p->getBrightness() + (0.001f * drag);
		brightness = ofClamp(brightness, 0, 1);
		ofFloatColor tempColor = p;
		tempColor.setBrightness(brightness);
		p = tempColor;
	};
	precisionDrag = [](ofParameter<ofFloatColor> &p, int drag){
		float brightness = p->getBrightness() + (0.0001f * drag);
		brightness = ofClamp(brightness, 0, 1);
		ofFloatColor tempColor = p;
		tempColor.setBrightness(brightness);
		p = tempColor;
	};
	speedDrag = [](ofParameter<ofFloatColor> &p, int drag){
		float brightness = p->getBrightness() + (0.01f * drag);
		brightness = ofClamp(brightness, 0, 1);
		ofFloatColor tempColor = p;
		tempColor.setBrightness(brightness);
		p = tempColor;
	};
}
