//
//  curve2.h
//  ofxOceanode
//
//  Created by Eduard Frigola on 25/05/21.
//
//

#ifndef curve2_h
#define curve2_h

#pragma once

#include "ofxOceanodeNodeModel.h"

struct curvePoint2{
	curvePoint2(){
		drag = 0;
		firstCreated = false;
	}
	curvePoint2(glm::vec2 p){
		point = p;
		drag = 0;
		firstCreated = true;
	}
	curvePoint2(float x, float y){
		point = glm::vec2(x, y);
		drag = 0;
		firstCreated = true;
	}
	glm::vec2 point;
	int drag;
	bool firstCreated;
};

enum lineType2{
	LINE2_HOLD,
    LINE2_TENSION
};

struct line2{
	lineType2 type = LINE2_TENSION;
	float tensionExponent = 1.0f;
};

class curve2 : public ofxOceanodeNodeModel{
public:
    curve2() : ofxOceanodeNodeModel("curve2"){};
    ~curve2(){};
    void setup() override;
	void draw(ofEventArgs &args) override;
	
	void recalculate();
	
	void presetSave(ofJson &json);
	void presetRecallAfterSettingParameters(ofJson &json);

private:
    
    ofEventListeners listeners;
    
    ofParameter<vector<float>>  input;
	ofParameter<bool> showWindow;
    ofParameter<vector<float>>  output;
	
	ofParameter<int> numVerticalDivisions;
	ofParameter<int> numHorizontalDivisions;
    
	
	ofParameter<float> minX, maxX, minY, maxY;
    
	vector<glm::vec2> debugPoints;
	
	vector<curvePoint2> points;
	bool createPoint = false;
	curvePoint2* popupPoint;
	
	vector<line2> lines;
	
	// Shift+drag tension control variables
	bool tensionDragActive = false;
	glm::vec2 tensionDragStartPos;
	int tensionDragSegmentIndex = -1;
	float tensionDragStartExponent = 1.0f;
	
	// Point highlighting system
	int hoveredPointIndex = -1;
    
    ofParameter<string> curveName;
    ofParameter<ofColor> colorParam;
    ofColor color;
    ofEventListener colorListener;

};


#endif /* curve2_h */
