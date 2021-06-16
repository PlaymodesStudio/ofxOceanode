//
//  curve.h
//  ofxOceanode
//
//  Created by Eduard Frigola on 25/05/21.
//
//

#ifndef curve_h
#define curve_h

#pragma once

#include "ofxOceanodeNodeModel.h"

struct curvePoint{
	curvePoint(){
		drag = 0;
		firstCreated = false;
	}
	curvePoint(glm::vec2 p){
		point = p;
		cp1 = p;
		cp2 = p;
		drag = 0;
		firstCreated = true;
	}
	curvePoint(float x, float y){
		point = glm::vec2(x, y);
		cp1 = point;
		cp2 = point;
		drag = 0;
		firstCreated = true;
	}
	glm::vec2 point;
	glm::vec2 cp1;
	glm::vec2 cp2;
	int drag;
	bool firstCreated;
};

enum lineType{
	LINE_BEZIER,
	LINE_HOLD
};

struct line{
	lineType type = LINE_BEZIER;
};

class curve : public ofxOceanodeNodeModel{
public:
    curve() : ofxOceanodeNodeModel("Curve"){};
    ~curve(){};
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
	
	vector<curvePoint> points;
	bool createPoint = false;
	curvePoint* popupPoint;
	
	vector<line> lines;
};


#endif /* curve_h */
