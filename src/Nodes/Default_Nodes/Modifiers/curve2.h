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
	float tensionExponent = 1.0f;  // Now controls asymmetry parameter ν
	float inflectionX = 0.5f;      // Normalized position 0-1 within segment
	float segmentB = 6.0f;         // Per-segment B parameter (default 6.0)
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
    // Parameter limits as class constants
    static constexpr float MIN_B_PARAMETER = 0.01f;
    static constexpr float MAX_B_PARAMETER = 100.0f;
    static constexpr float MIN_INFLECTION_X = 0.01f;
    static constexpr float MAX_INFLECTION_X = 0.99f;
    static constexpr float MIN_ASYMMETRY = 0.02f;
    static constexpr float MAX_ASYMMETRY = 10.0f;  // Changed from 100.0
    
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
	
	// Cmd+drag (Ctrl+drag on non-Mac) B parameter control variables
	bool bDragActive = false;
	glm::vec2 bDragStartPos;
	int bDragSegmentIndex = -1;
	float bDragStartValue = 6.0f;
	
	// Point highlighting system
	int hoveredPointIndex = -1;
	
	// Segment hover detection system
	int hoveredSegmentIndex = -1;
    
    ofParameter<string> curveName;
    ofParameter<ofColor> colorParam;
    ofColor color;
    ofEventListener colorListener;
    
    // Global logistic parameters
    ofParameter<float> globalQ;  // Global scaling parameter (default 1.0)
    
    // Snap to grid functionality
    ofParameter<bool> snapToGrid;  // Enable/disable snap to grid
    
    // Show info toggle functionality
    ofParameter<bool> showInfo;  // Enable/disable text area visibility

};


#endif /* curve2_h */
