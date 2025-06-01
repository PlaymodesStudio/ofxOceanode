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
#include "imgui.h"

//---------------------------------------------------------------

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

//---------------------------------------------------------------

enum lineType2{
	LINE2_HOLD,
	LINE2_TENSION
};

//---------------------------------------------------------------

struct line2{
	lineType2 type = LINE2_TENSION;
	float tensionExponent = 1.0f;  // Now controls asymmetry parameter ν
	float inflectionX = 0.5f;      // Normalized position 0-1 within segment
	float segmentB = 6.0f;         // Per-segment B parameter (default 6.0)
	float segmentQ = 1.0f;         // Per-segment Q parameter (default 1.0)
};

//---------------------------------------------------------------

// CurveData structure to encapsulate all curve-specific data
struct CurveData {
	vector<curvePoint2> points;
	vector<line2> lines;
	
	// Per-curve parameters
	ofParameter<string> name;
	ofParameter<ofColor> color;
	ofParameter<bool> enabled;
	ofParameter<float> globalQ;  // Per-curve Q parameter
	
	CurveData() {
		// Initialize with default curve (0,0) to (1,1)
		points.emplace_back(0, 0);
		points.emplace_back(1, 1);
		points.front().firstCreated = false;
		points.back().firstCreated = false;
		lines.emplace_back();
		
		// Set default parameter values
		name.set("Name", "Curve");
		color.set("Color", ofColor(255, 128, 0, 255));
		enabled.set("Enabled", true);
		globalQ.set("Global Q", 1.0f, 0.1f, 5.0f);
	}
};

//---------------------------------------------------------------

class curve2 : public ofxOceanodeNodeModel{
public:
	curve2();
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
	//vector<ofParameter<vector<float>>> outputs;
	vector<shared_ptr<ofParameter<vector<float>>>> outputs;
	ofParameter<vector<float>> allCurvesOutput;  // Consolidated output parameter
	
	ofParameter<int> numVerticalDivisions;
	ofParameter<int> numHorizontalDivisions;
	
	// Global constants (apply to all curves)
	static constexpr float minX = 0.0f;
	static constexpr float maxX = 1.0f;
	
	
	// Multi-curve support
	ofParameter<int> numCurves;      // Number of curves (default: 1)
	ofParameter<int> activeCurve;    // Currently selected curve index (-1 = None, 0+ = curve index)
	vector<CurveData> curves;        // Vector of curve data
	
	// Legacy single-curve compatibility (pointers to active curve)
	vector<curvePoint2>* points;     // Pointer to active curve points
	vector<line2>* lines;            // Pointer to active curve lines
	
	bool createPoint = false;
	curvePoint2* popupPoint;
	
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
	
	bool showCurveLabels = true;       // Show curve names in editor
	float curveHitTestRadius = 8.0f;   // Distance threshold for curve selection
	
	// Performance optimization flags
	bool needsRedraw = true;           // Flag to minimize unnecessary redraws
	int lastFrameMouseX = -1;          // Mouse position tracking for optimization
	int lastFrameMouseY = -1;
	
	// Legacy compatibility parameters (now point to active curve)
	ofParameter<ofColor>* colorParam;
	ofParameter<float>* globalQ;
	
	ofColor color;
	ofEventListener colorListener;
	
	// Snap to grid functionality
	ofParameter<bool> snapToGrid;  // Enable/disable snap to grid
	
	// Show info toggle functionality
	ofParameter<bool> showInfo;  // Enable/disable text area visibility
	
	// Inspector GUI parameter for tabbed interface
	ofParameter<std::function<void()>> inspectorGui;
	
	// Curve management methods
	void addCurve();
	void removeCurve(int index);
	void resizeCurves(int newCount);
	CurveData& getCurrentCurve();
	const CurveData& getCurrentCurve() const;
	int findNextAvailableCurveNumber();
	
	// Output management methods
	void addOutput();
	void removeOutput();
	void resizeOutputs(int count);
	
	// Parameter listeners
	void onNumCurvesChanged(int& newCount);
	void onActiveCurveChanged(int& newIndex);
	
	// Inspector interface rendering
	void renderInspectorInterface();
	void drawDottedLine(ImDrawList* drawList, ImVec2 start, ImVec2 end, ImU32 color, float dashLength = 5.0f, float gapLength = 10.0f);
};


#endif /* curve2_h */
