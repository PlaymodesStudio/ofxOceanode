//
//  curve.cpp
//  ofxOceanode
//
//  Created by Eduard Frigola on 25/05/21.
//
//

#include "curve2.h"
#include "glm/gtx/closest_point.hpp"


curve2::curve2() : ofxOceanodeNodeModel("curve2") {
	// Initialize with one curve
	curves.resize(1);
	
	// Set default name for first curve
	curves[0].name.set("Name", "Curve 1");
	
	showCurveLabels = false;
	curveHitTestRadius = 8.0f;
	needsRedraw = true;
	lastFrameMouseX = -1;
	lastFrameMouseY = -1;
	
	// Initialize pointers to point to the first curve
	points = &curves[0].points;
	lines = &curves[0].lines;
	colorParam = &curves[0].color;
	globalQ = &curves[0].globalQ;
}

void curve2::setup() {
	color = ofColor(255,128,0,255);
	description = "Advanced curve editor with asymmetric logistic segments. \n| Double-click: Create points \n| Drag: Move points \n| Right-click: Delete \n| Shift+drag: Asymmetry (vertical) + Inflection (horizontal) \n| Ctrl+drag: B parameter (horizontal) \n| Visual feedback: Yellow=Inflection, Cyan=Asymmetry, Red=B parameter \n| Snap to Grid: Enable for precise grid-aligned positioning";
	
	// Initialize multi-curve system
	numCurves=1;

	// Set up parameters
	addParameter(input.set("Input", {0}, {0}, {1}));
	addParameter(showWindow.set("Show", true));
	addOutputParameter(allCurvesOutput.set("All Curves", {0}, {0}, {1}));
		
	// Initialize outputs based on numCurves
	outputs.resize(numCurves.get());
	for(int i = 0; i < outputs.size(); i++){
		outputs[i] = make_shared<ofParameter<vector<float>>>();
		string outputName = (i < curves.size()) ? curves[i].name.get() : ("Curve " + ofToString(i + 1));
		if(outputs[i]) {
			addOutputParameter(outputs[i]->set(outputName, {0}, {0}, {1}));
		}
	}
	
	// Initialize parameters (but don't add them to inspector yet)
	numHorizontalDivisions.set("Hor Div", 8, 1, 512);
	numVerticalDivisions.set("Vert Div", 4, 1, 512);
	minX.set("Min X", 0, -FLT_MAX, FLT_MAX);
	maxX.set("Max X", 1, -FLT_MAX, FLT_MAX);
	snapToGrid.set("Snap to Grid", false);
	showInfo.set("Show Info", false);
	
	if(colorParam != nullptr){
		colorListener = colorParam->newListener([this](ofColor &c){
			color = c;
		});
	}
	
	// Add the tabbed inspector GUI
	addInspectorParameter(inspectorGui.set("Inspector", [this](){
		renderInspectorInterface();
	}));
	
	// Set up listeners
	listeners.push(input.newListener([&](vector<float> &vf){
		recalculate();
	}));
	
	listeners.push(minX.newListener([this](float &f){
		input.setMin(vector<float>(1, f));
	}));
	listeners.push(maxX.newListener([this](float &f){
		input.setMax(vector<float>(1, f));
	}));
	
	// Multi-curve listeners
	listeners.push(numCurves.newListener([this](int &newCount){
		onNumCurvesChanged(newCount);
	}));
	listeners.push(activeCurve.newListener([this](int &newIndex){
		onActiveCurveChanged(newIndex);
	}));
}

void curve2::draw(ofEventArgs &args){
	
	if(showWindow){
		string modCanvasID = canvasID == "Canvas" ? "" : (canvasID + "/");
		string windowTitle = modCanvasID + "Curve2 " + ofToString(getNumIdentifier());
		if(ImGui::Begin(windowTitle.c_str(), (bool *)&showWindow.get()))
		{
			ImGui::SameLine();
			ImGui::BeginGroup();
			
			const ImVec2 NODE_WINDOW_PADDING(8.0f, 7.0f);
			
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55,0.55,0.55,1.0));
			
			if (ImGui::Button("[+]") && curves.size() < 16) {
				addCurve();
				numCurves = curves.size();
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Add Curve");
			}
			ImGui::SameLine();
			if (ImGui::Button("[-]") && curves.size() > 1) {
				removeCurve(activeCurve.get());
				numCurves = curves.size();
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Remove Curve");
			}
			ImGui::SameLine();
			if(ImGui::Button("[Reset]"))
			{
				if(points != nullptr && lines != nullptr){
					// Clear existing points and lines
					points->clear();
					lines->clear();
					
					// Create default curve with 2 points: (0,0) and (1,1)
					points->emplace_back(0, 0);
					points->emplace_back(1, 1);
					
					// Set points as not newly created
					points->front().firstCreated = false;
					points->back().firstCreated = false;
					
					// Create default line segment with default values
					lines->emplace_back();
					
					// Trigger recalculation of the curve
					recalculate();
				}
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Reset Curve");
			}
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0,1.0,1.0,1.0));
			
			// Active curve selection dropdown with curve names
			string activeCurveName = (activeCurve.get() == -1) ? "None" :
			((activeCurve.get() < curves.size()) ? curves[activeCurve.get()].name.get() : "Invalid");
			ImGui::SetNextItemWidth(120.0f);
			if (ImGui::BeginCombo("##ActiveCurveEditor", activeCurveName.c_str())) {
				// Add "None" option
				bool isNoneSelected = (activeCurve.get() == -1);
				if (ImGui::Selectable("None", isNoneSelected)) {
					activeCurve = -1;
				}
				if (isNoneSelected) {
					ImGui::SetItemDefaultFocus();
				}
				
				// Add curve options with color indicators and names
				for (int i = 0; i < curves.size(); i++) {
					bool isSelected = (activeCurve.get() == i);
					string curveName = curves[i].name.get();
					
					// Draw color indicator
					ofColor curveColor = curves[i].color.get();
					ImVec2 cursorPos = ImGui::GetCursorScreenPos();
					ImDrawList* drawList = ImGui::GetWindowDrawList();
					drawList->AddRectFilled(
											ImVec2(cursorPos.x + 2, cursorPos.y + 2),
											ImVec2(cursorPos.x + 12, cursorPos.y + 12),
											IM_COL32(curveColor.r, curveColor.g, curveColor.b, 255)
											);
					
					// Add spacing for color indicator
					ImGui::Dummy(ImVec2(16, 0));
					ImGui::SameLine();
					
					if (ImGui::Selectable(curveName.c_str(), isSelected)) {
						activeCurve = i;
					}
					if (isSelected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			ImGui::PopStyleColor();
			
			// Add curve parameters on the same line (only when a curve is selected)
			if(activeCurve.get() >= 0 && activeCurve.get() < curves.size()) {
				auto& currentCurve = curves[activeCurve.get()];
				
				ImGui::SameLine();
				
				// Enabled checkbox (compact)
				bool enabled = currentCurve.enabled.get();
				if (ImGui::Checkbox("##Enabled", &enabled)) {
					currentCurve.enabled = enabled;
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Enabled");
				}
				
				ImGui::SameLine();
				
				// Color parameter (compact)
				float colorArray[4] = {
					currentCurve.color.get().r / 255.0f,
					currentCurve.color.get().g / 255.0f,
					currentCurve.color.get().b / 255.0f,
					currentCurve.color.get().a / 255.0f
				};
				ImGui::SetNextItemWidth(60.0f);
				if (ImGui::ColorEdit4("##Color", colorArray, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
					currentCurve.color = ofColor(
												 colorArray[0] * 255,
												 colorArray[1] * 255,
												 colorArray[2] * 255,
												 colorArray[3] * 255
												 );
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Color");
				}
				
				ImGui::SameLine();
				
				// Name text input
				char nameBuffer[256];
				string currentName = currentCurve.name.get();
				strncpy(nameBuffer, currentName.c_str(), sizeof(nameBuffer) - 1);
				nameBuffer[sizeof(nameBuffer) - 1] = '\0';
				
				ImGui::SetNextItemWidth(100.0f);
				if (ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer))) {
					string newName = string(nameBuffer);
					if (!newName.empty()) {
						currentCurve.name = newName;
						// Update output parameter name
						if (activeCurve.get() < outputs.size() && outputs[activeCurve.get()]) {
							outputs[activeCurve.get()]->setName(newName);
						}
					}
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Curve Name");
				}
			}
			ImGui::PopStyleVar();
			ImGui::PopStyleColor(); // Pop the grayed text color from line 132
			
			// Calculate available height for curve editor
			ImVec2 windowSize = ImGui::GetContentRegionAvail();
			float curveEditorHeight;
			
			if (showInfo) {
				// Current behavior - calculate widget height and subtract from total
				float widgetAreaHeight = 0.0f;
				
				// Estimate widget area height based on content
				// Base height for "Curve Parameters" text and spacing
				widgetAreaHeight += ImGui::GetTextLineHeight() + ImGui::GetStyle().ItemSpacing.y * 2;
				
				// Height for separator
				widgetAreaHeight += 1.0f + ImGui::GetStyle().ItemSpacing.y;
				
				// Height for parameter display (2-3 lines of text)
				widgetAreaHeight += ImGui::GetTextLineHeight() * 3 + ImGui::GetStyle().ItemSpacing.y * 2;
				
				// Add some padding for safety
				widgetAreaHeight += ImGui::GetStyle().WindowPadding.y * 2;
				
				curveEditorHeight = std::max(50.0f, windowSize.y - widgetAreaHeight);
			} else {
				// Use all available height for curve area
				curveEditorHeight = std::max(50.0f, windowSize.y);
			}
			
			// Create our child canvas with calculated height
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
			ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(25, 25, 25, 200));
			ImGui::BeginChild("scrolling_region", ImVec2(0, curveEditorHeight), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse);
			ImGui::PushItemWidth(120.0f);
			
			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			
			// Safe zone padding - creates an invisible interaction area around the visual curve editor
			const float SAFE_ZONE_PADDING = 10.0f;
			
			// Get the available space
			ImVec2 win_pos = ImGui::GetCursorScreenPos();
			ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
			
			// Visual drawing area (smaller, centered within the canvas)
			ImVec2 visual_win_pos = ImVec2(win_pos.x + SAFE_ZONE_PADDING, win_pos.y + SAFE_ZONE_PADDING);
			ImVec2 visual_canvas_sz = ImVec2(canvas_sz.x - 2 * SAFE_ZONE_PADDING, canvas_sz.y - 2 * SAFE_ZONE_PADDING);
			
			// Ensure visual area is not negative
			if (visual_canvas_sz.x < 50.0f) visual_canvas_sz.x = 50.0f;
			if (visual_canvas_sz.y < 50.0f) visual_canvas_sz.y = 50.0f;
			
			// Display grid only in the visual area
			{
				ImU32 GRID_COLOR = IM_COL32(120, 120, 120, 60);
				ImVec2 cell_sz = visual_canvas_sz / ImVec2(numHorizontalDivisions, numVerticalDivisions);
				
				// Draw vertical lines (skip first and last by starting at cell_sz.x and stopping before visual_canvas_sz.x)
				for (float x = cell_sz.x; x < visual_canvas_sz.x - cell_sz.x * 0.5f; x += cell_sz.x)
					draw_list->AddLine(ImVec2(x, 0.0f) + visual_win_pos, ImVec2(x, visual_canvas_sz.y) + visual_win_pos, GRID_COLOR);
				// Draw horizontal lines (skip first and last by starting at cell_sz.y and stopping before visual_canvas_sz.y)
				for (float y = cell_sz.y; y < visual_canvas_sz.y - cell_sz.y * 0.5f; y += cell_sz.y)
					draw_list->AddLine(ImVec2(0.0f, y) + visual_win_pos, ImVec2(visual_canvas_sz.x, y) + visual_win_pos, GRID_COLOR);
			}
			
			//			// Draw visual area boundary to show the safe zone effect
			//			ImU32 BOUNDARY_COLOR = IM_COL32(10, 10, 10, 128);
			//			draw_list->AddRect(visual_win_pos,
			//							   ImVec2(visual_win_pos.x + visual_canvas_sz.x, visual_win_pos.y + visual_canvas_sz.y),
			//							   BOUNDARY_COLOR, 0.0f, 0, 2.0f);
			
			// Make the ENTIRE canvas area interactive (including safe zone) - this is the key fix!
			ImGui::InvisibleButton("canvas", canvas_sz);
			bool canvasHovered = ImGui::IsItemHovered();
			bool canvasClicked = ImGui::IsItemClicked(0);
			bool canvasDoubleClicked = ImGui::IsItemClicked(0) && ImGui::IsMouseDoubleClicked(0);
			
			ImGui::PopItemWidth();
			
			// Use visual coordinates for all curve calculations and drawing
			auto normalizePoint = [visual_win_pos, visual_canvas_sz](glm::vec2 p) -> glm::vec2{
				return glm::vec2(ofMap(p.x, visual_win_pos.x, visual_win_pos.x+visual_canvas_sz.x, 0, 1),
								 ofMap(p.y, visual_win_pos.y, visual_win_pos.y+visual_canvas_sz.y, 1, 0));
			};
			
			auto denormalizePoint = [visual_win_pos, visual_canvas_sz](glm::vec2 p) -> glm::vec2{
				return glm::vec2(ofMap(p.x, 0, 1, visual_win_pos.x, visual_win_pos.x+visual_canvas_sz.x),
								 ofMap(p.y, 1, 0, visual_win_pos.y, visual_win_pos.y+visual_canvas_sz.y));
			};
			
			// Safe zone aware normalize function - clamps coordinates to visual area when in safe zone
			auto safeNormalizePoint = [visual_win_pos, visual_canvas_sz, win_pos, canvas_sz](glm::vec2 p) -> glm::vec2{
				// Check if mouse is within the entire canvas (including safe zone)
				if (p.x >= win_pos.x && p.x <= win_pos.x + canvas_sz.x &&
					p.y >= win_pos.y && p.y <= win_pos.y + canvas_sz.y) {
					
					// Clamp mouse position to visual area boundaries
					float clampedX = std::max(visual_win_pos.x, std::min(visual_win_pos.x + visual_canvas_sz.x, p.x));
					float clampedY = std::max(visual_win_pos.y, std::min(visual_win_pos.y + visual_canvas_sz.y, p.y));
					
					// Normalize the clamped position
					return glm::vec2(ofMap(clampedX, visual_win_pos.x, visual_win_pos.x+visual_canvas_sz.x, 0, 1),
									 ofMap(clampedY, visual_win_pos.y, visual_win_pos.y+visual_canvas_sz.y, 1, 0));
				}
				
				// If outside entire canvas, use regular normalization
				return glm::vec2(ofMap(p.x, visual_win_pos.x, visual_win_pos.x+visual_canvas_sz.x, 0, 1),
								 ofMap(p.y, visual_win_pos.y, visual_win_pos.y+visual_canvas_sz.y, 1, 0));
			};
			
			if(ImGui::IsMouseReleased(0)){
				if(points != nullptr){
					for(int i = 0; i < points->size(); i++){
						(*points)[i].drag = 0;
						(*points)[i].firstCreated = false;
					}
				}
			}
			
			hoveredPointIndex = -1;
			hoveredSegmentIndex = -1;
			
			// Only enable hover interactions when a specific curve is selected (not "None")
			if(canvasHovered && activeCurve.get() >= 0){
				glm::vec2 mousePos = glm::vec2(ImGui::GetMousePos());
				glm::vec2 normalizedMousePos = safeNormalizePoint(mousePos);
				
				// First check for point hover on active curve (highest priority)
				if(activeCurve.get() < curves.size() && curves[activeCurve.get()].enabled.get() && points != nullptr){
					for(int i = 0; i < points->size(); i++){
						glm::vec2 pointPos = denormalizePoint((*points)[i].point);
						float mouseToPointDistance = glm::distance(mousePos, pointPos);
						if(mouseToPointDistance < 15){ // Increased hover detection radius
							hoveredPointIndex = i;
							break;
						}
					}
				}
				
				// If no point is hovered, check for segment hover on active curve
				if(hoveredPointIndex == -1 && activeCurve.get() < curves.size() && curves[activeCurve.get()].enabled.get() && points != nullptr && lines != nullptr){
					for(int i = 0; i < points->size()-1; i++){
						if(normalizedMousePos.x >= (*points)[i].point.x &&
						   normalizedMousePos.x <= (*points)[i+1].point.x &&
						   (*lines)[i].type == LINE2_TENSION){
							hoveredSegmentIndex = i;
							break;
						}
					}
				}
			}
			
			// PHASE 5: Enhanced click handling with curve selection
			if(canvasDoubleClicked && activeCurve.get() >= 0 && points != nullptr && lines != nullptr){
				glm::vec2 newPointPos = safeNormalizePoint(ImGui::GetMousePos());
				
				// Apply snap to grid if enabled
				if(snapToGrid){
					// Calculate grid spacing
					float gridSpacingX = 1.0f / numHorizontalDivisions;
					float gridSpacingY = 1.0f / numVerticalDivisions;
					
					// Snap to nearest grid point
					newPointPos.x = round(newPointPos.x / gridSpacingX) * gridSpacingX;
					newPointPos.y = round(newPointPos.y / gridSpacingY) * gridSpacingY;
				}
				
				// Find which segment is being split to inherit its parameters
				int splitSegmentIndex = -1;
				for(int i = 0; i < points->size()-1; i++){
					if(newPointPos.x >= (*points)[i].point.x && newPointPos.x <= (*points)[i+1].point.x){
						splitSegmentIndex = i;
						break;
					}
				}
				
				points->emplace_back(newPointPos);
				points->back().drag = 3;
				
				// Create new line segment with inherited parameters from the split segment
				if(splitSegmentIndex >= 0 && splitSegmentIndex < lines->size()){
					// Inherit all parameters from the segment being split
					line2 newLine = (*lines)[splitSegmentIndex];
					lines->emplace_back(newLine);
				} else {
					// Fallback to default parameters if no segment found
					lines->emplace_back();
				}
			}
						
			// Declare someItemClicked in broader scope so it can be used by all interaction code
			bool someItemClicked = false;
			
			// Only process point interactions when a curve is selected
			if(activeCurve.get() >= 0 && points != nullptr && lines != nullptr){
				std::sort(points->begin(), points->end(), [](curvePoint2 &p1, curvePoint2 &p2){
					return p1.point.x < p2.point.x;
				});
				
				int indexToRemove = -1;
				for(int i = 0; i < points->size(); i++){
					auto &p = (*points)[i];
					
					if(ImGui::IsMouseDragging(0) && p.drag != 0){
						if(p.drag == 3){
							// Use safe zone aware normalization - allows dragging to continue even when mouse is outside visual area
							glm::vec2 normalizedPos = glm::clamp(safeNormalizePoint(ImGui::GetMousePos()), 0.0f, 1.0f);
							
							// Apply snap to grid if enabled
							if(snapToGrid){
								// Calculate grid spacing
								float gridSpacingX = 1.0f / numHorizontalDivisions;
								float gridSpacingY = 1.0f / numVerticalDivisions;
								
								// Snap to nearest grid point
								normalizedPos.x = round(normalizedPos.x / gridSpacingX) * gridSpacingX;
								normalizedPos.y = round(normalizedPos.y / gridSpacingY) * gridSpacingY;
							}
							
							// Legacy key-based snapping (X and Y keys for individual axis snapping)
							if(ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_X))){
								normalizedPos.x = round(normalizedPos.x * numHorizontalDivisions) / numHorizontalDivisions;
							}
							if(ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Y))){
								normalizedPos.y = round(normalizedPos.y * numVerticalDivisions) / numVerticalDivisions;
							}
							
							p.point = normalizedPos;
						}
					}
					
					glm::vec2 pointPos = denormalizePoint(p.point);
					if(canvasClicked && !someItemClicked){
						auto mouseToPointDistance = glm::distance(glm::vec2(ImGui::GetMousePos()), pointPos);
						if(mouseToPointDistance < 15){ // Increased selection radius
							p.drag = 3;
							someItemClicked = true;
						}
					}else if(ImGui::IsMouseClicked(1) && canvasHovered){
						// Use hovered point for right-click if available, otherwise check distance
						if(hoveredPointIndex == i || (hoveredPointIndex == -1 && glm::distance(glm::vec2(ImGui::GetMousePos()), pointPos) < 15)){
							ImGui::OpenPopup(("##PointPopup " + ofToString(i)).c_str());
						}
					}
					
					if(ImGui::BeginPopup(("##PointPopup " + ofToString(i)).c_str())){
						float tempFloat = ofMap(p.point.x, 0, 1, minX.get(), maxX.get());
						ImGui::SliderFloat("X", &tempFloat, minX.get(), maxX.get());
						if(ImGui::IsItemDeactivated() || (ImGui::IsMouseDown(0) && ImGui::IsItemEdited())){
							p.point.x = ofMap(tempFloat, minX.get(), maxX.get(), 0, 1);
						}
						
						// Y value editing (0-1 range)
						tempFloat = p.point.y;
						ImGui::SliderFloat("Y", &tempFloat, 0.0f, 1.0f);
						if(ImGui::IsItemDeactivated() || (ImGui::IsMouseDown(0) && ImGui::IsItemEdited())){
							p.point.y = tempFloat;
						}
						
						ImGui::Spacing();
						
						if(i > 0 && lines != nullptr){
							int elem = (*lines)[i-1].type;
							const char* elems_names[2] = { "Hold", "Tension"};
							if(ImGui::SliderInt("Line L", &elem, 0, 1, elems_names[elem])){
								(*lines)[i-1].type = static_cast<lineType2>(elem);
							}
							
							// Show asymmetric logistic controls for TENSION segments
							if((*lines)[i-1].type == LINE2_TENSION){
								if(ImGui::SliderFloat("Asymmetry L", &(*lines)[i-1].tensionExponent, MIN_ASYMMETRY, MAX_ASYMMETRY, "%.3f")){
									recalculate();
								}
								if(ImGui::SliderFloat("Inflection L", &(*lines)[i-1].inflectionX, 0.0f, 1.0f, "%.3f")){
									recalculate();
								}
								if(ImGui::SliderFloat("Segment B L", &(*lines)[i-1].segmentB, MIN_B_PARAMETER, MAX_B_PARAMETER, "%.2f")){
									recalculate();
								}
								if(ImGui::SliderFloat("Segment Q L", &(*lines)[i-1].segmentQ, 0.1f, 5.0f, "%.2f")){
									recalculate();
								}
							}
						}
						
						if(lines != nullptr && i < lines->size()){
							int elem = (*lines)[i].type;
							const char* elems_names[2] = { "Hold", "Tension"};
							if(ImGui::SliderInt("Line R", &elem, 0, 1, elems_names[elem])){
								(*lines)[i].type = static_cast<lineType2>(elem);
							}
							
							// Show asymmetric logistic controls for TENSION segments
							if((*lines)[i].type == LINE2_TENSION){
								if(ImGui::SliderFloat("Asymmetry R", &(*lines)[i].tensionExponent, MIN_ASYMMETRY, MAX_ASYMMETRY, "%.3f")){
									recalculate();
								}
								if(ImGui::SliderFloat("Inflection R", &(*lines)[i].inflectionX, 0.0f, 1.0f, "%.3f")){
									recalculate();
								}
								if(ImGui::SliderFloat("Segment B R", &(*lines)[i].segmentB, MIN_B_PARAMETER, MAX_B_PARAMETER, "%.2f")){
									recalculate();
								}
								if(ImGui::SliderFloat("Segment Q R", &(*lines)[i].segmentQ, 0.1f, 5.0f, "%.2f")){
									recalculate();
								}
							}
						}
						
						ImGui::Separator();
						ImGui::Text("Curve Properties:");
						if(globalQ != nullptr){
							float tempQ = globalQ->get();
							
							if(ImGui::SliderFloat("Q (Scaling)", &tempQ, 0.1f, 5.0f, "%.1f")) {
								globalQ->set(tempQ);
								recalculate();
							}
						} else {
							ImGui::Text("Q (Scaling): N/A (no active curve)");
						}
						
						ImGui::Spacing();
						
						if(ImGui::Selectable("[Remove]")){
							ImGui::CloseCurrentPopup();
							indexToRemove = i;
						}
						ImGui::EndPopup();
					}
				}
				
				if(indexToRemove != -1 && points != nullptr && lines != nullptr){
					points->erase(points->begin()+indexToRemove);
					if(indexToRemove < lines->size()){
						lines->erase(lines->begin()+indexToRemove);
					}
				}
			} // End of active curve check for point interactions
			
			// Shift+drag interaction for asymmetric logistic curve adjustment (tension/asymmetry control)
			if(activeCurve.get() >= 0 && points != nullptr && lines != nullptr && ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_LeftShift)) && !someItemClicked && canvasHovered){
				if(ImGui::IsMouseDragging(0)){
					if(!tensionDragActive){
						// Start new tension drag operation
						tensionDragActive = true;
						tensionDragStartPos = safeNormalizePoint(ImGui::GetMousePos());
						tensionDragSegmentIndex = -1;
						
						// Find which segment the mouse is over
						for(int i = 0; i < points->size()-1; i++){
							if(tensionDragStartPos.x >= (*points)[i].point.x && tensionDragStartPos.x <= (*points)[i+1].point.x && (*lines)[i].type == LINE2_TENSION){
								tensionDragSegmentIndex = i;
								tensionDragStartExponent = (*lines)[i].tensionExponent;
								break;
							}
						}
					}
					
					// Continue existing tension drag operation
					if(tensionDragActive && tensionDragSegmentIndex >= 0){
						glm::vec2 currentMousePos = safeNormalizePoint(ImGui::GetMousePos());
						
						// Calculate drag deltas
						float deltaX = currentMousePos.x - tensionDragStartPos.x;
						float deltaY = currentMousePos.y - tensionDragStartPos.y;
						
						// Horizontal drag: Set inflection point X position within segment (0.0 to 1.0)
						float segmentStartX = (*points)[tensionDragSegmentIndex].point.x;
						float segmentEndX = (*points)[tensionDragSegmentIndex + 1].point.x;
						float segmentWidth = segmentEndX - segmentStartX;
						
						if(segmentWidth > 0.0001f){
							// Map current mouse X to xsegment-relative position
							float relativeX = (currentMousePos.x - segmentStartX) / segmentWidth;
							// Constrain to avoid edge issues
							(*lines)[tensionDragSegmentIndex].inflectionX = std::max(MIN_INFLECTION_X, std::min(MAX_INFLECTION_X, relativeX));
						}
						
						// Vertical drag: Simple height mapping for asymmetry parameter
						// Enhanced mapping: Split curve area into two zones for better precision
						float normalizedY = (currentMousePos.y - 0.0f) / (1.0f - 0.0f); // Mouse Y position within curve area (0.0 to 1.0)
						
						// Define asymmetry range
						const float minAsymmetry = MIN_ASYMMETRY;
						const float maxAsymmetry = MAX_ASYMMETRY;
						float asymmetry;
						
						// Split the curve area height into two zones:
						// Lower half (bottom 50%): Maps from 1 (middle) to 0.02 (bottom)
						// Upper half (top 50%): Maps from 1 (middle) to 100 (top)
						if (normalizedY <= 0.5f) {
							// Lower half: map from 1 (at middle) to 0.02 (at bottom)
							// normalizedY 0.0 (bottom) -> asymmetry = 0.02 (minimum)
							// normalizedY 0.5 (middle) -> asymmetry = 1.0
							float lowerHalfY = normalizedY / 0.5f; // 0.0 to 1.0
							asymmetry = minAsymmetry + lowerHalfY * (1.0f - minAsymmetry);
						} else {
							// Upper half: map from 1 (at middle) to 100 (at top)
							// normalizedY 0.5 (middle) -> asymmetry = 1.0
							// normalizedY 1.0 (top) -> asymmetry = 100.0 (maximum)
							float upperHalfY = (normalizedY - 0.5f) / 0.5f; // 0.0 to 1.0
							asymmetry = 1.0f + upperHalfY * (maxAsymmetry - 1.0f);
						}
						
						(*lines)[tensionDragSegmentIndex].tensionExponent = asymmetry;
						
						// Clamp to valid range
						(*lines)[tensionDragSegmentIndex].tensionExponent = std::max(minAsymmetry, std::min(maxAsymmetry, (*lines)[tensionDragSegmentIndex].tensionExponent));
						
						recalculate();
					}
				} else {
					// Reset tension drag state when not dragging
					tensionDragActive = false;
					tensionDragSegmentIndex = -1;
				}
			} else {
				// Reset tension drag state when Shift is not pressed
				tensionDragActive = false;
				tensionDragSegmentIndex = -1;
			}
			
			// Ctrl+drag interaction for B parameter control (all platforms)
			bool ctrlPressed = ImGui::GetIO().KeyCtrl;
			
			if(activeCurve.get() >= 0 && points != nullptr && lines != nullptr && ctrlPressed && !someItemClicked && canvasHovered){
				if(ImGui::IsMouseDragging(0)){
					if(!bDragActive){
						// Start new B parameter drag operation
						bDragActive = true;
						bDragStartPos = safeNormalizePoint(ImGui::GetMousePos());
						bDragSegmentIndex = -1;
						
						// Find which segment the mouse is over
						for(int i = 0; i < points->size()-1; i++){
							if(bDragStartPos.x >= (*points)[i].point.x && bDragStartPos.x <= (*points)[i+1].point.x && (*lines)[i].type == LINE2_TENSION){
								bDragSegmentIndex = i;
								bDragStartValue = (*lines)[i].segmentB;
								break;
							}
						}
					}
					
					// Continue existing B parameter drag operation
					if(bDragActive && bDragSegmentIndex >= 0){
						glm::vec2 currentMousePos = safeNormalizePoint(ImGui::GetMousePos());
						
						// X-position-based B parameter mapping within segment
						float segmentStartX = (*points)[bDragSegmentIndex].point.x;
						float segmentEndX = (*points)[bDragSegmentIndex + 1].point.x;
						float segmentWidth = segmentEndX - segmentStartX;
						
						if(segmentWidth > 0.0001f){
							// Map current mouse X position within segment to B parameter range
							float relativeX = (currentMousePos.x - segmentStartX) / segmentWidth;
							// Clamp to segment bounds
							relativeX = std::max(0.0f, std::min(1.0f, relativeX));
							
							// Linear interpolation: B = maxB - relativeX * (maxB - minB)
							const float minB = MIN_B_PARAMETER;
							const float maxB = MAX_B_PARAMETER;
							(*lines)[bDragSegmentIndex].segmentB = maxB - relativeX * (maxB - minB);
						}
						
						recalculate();
					}
				} else {
					// Reset B drag state when not dragging
					bDragActive = false;
					bDragSegmentIndex = -1;
				}
			} else {
				// Reset B drag state when Ctrl is not pressed
				bDragActive = false;
				bDragSegmentIndex = -1;
			}
						
			ImGui::EndChild();
			ImGui::PopStyleColor();
			ImGui::PopStyleVar(2);
			
			// Conditional widget area below curve editor
			if (showInfo) {
				ImGui::Spacing();
				ImGui::Separator();
				
				// Determine what to display based on current editing context
				bool showPointInfo = false;
				bool showSegmentInfo = false;
				int activePointIndex = -1;
				int activeSegmentIndex = -1;
				
				// Check for active point dragging (only if curve is selected)
				if(activeCurve.get() >= 0 && points != nullptr){
					for(int i = 0; i < points->size(); i++){
						if((*points)[i].drag == 3){
							showPointInfo = true;
							activePointIndex = i;
							break;
						}
					}
				}
				
				// Check for active tension dragging
				if(tensionDragActive && tensionDragSegmentIndex >= 0){
					showSegmentInfo = true;
					activeSegmentIndex = tensionDragSegmentIndex;
				}
				
				// Check for active B parameter dragging
				if(bDragActive && bDragSegmentIndex >= 0){
					showSegmentInfo = true;
					activeSegmentIndex = bDragSegmentIndex;
				}
				
				// If no active dragging, use hovered point
				if(!showPointInfo && !showSegmentInfo && hoveredPointIndex >= 0){
					showPointInfo = true;
					activePointIndex = hoveredPointIndex;
				}
				
				// Display point information (only if curve is selected)
				if(activeCurve.get() >= 0 && points != nullptr && showPointInfo && activePointIndex >= 0 && activePointIndex < points->size()){
					ImGui::Text("Point %d:", activePointIndex);
					ImGui::SameLine();
					
					// Point coordinates (0-1 range)
					float pointX = ofMap((*points)[activePointIndex].point.x, 0, 1, minX.get(), maxX.get());
					float pointY = (*points)[activePointIndex].point.y; // Direct 0-1 range
					
					ImGui::PushItemWidth(80);
					ImGui::Text("X: %.3f", pointX);
					ImGui::SameLine();
					ImGui::Text("Y: %.3f", pointY);
					ImGui::PopItemWidth();
				}
				
				// Display segment information
				if(showSegmentInfo && lines != nullptr && globalQ != nullptr && activeSegmentIndex >= 0 && activeSegmentIndex < lines->size()){
					if(showPointInfo) ImGui::Spacing();
					
					ImGui::Text("Segment %d:", activeSegmentIndex);
					ImGui::SameLine();
					
					// Line type
					const char* lineTypeName = ((*lines)[activeSegmentIndex].type == LINE2_HOLD) ? "HOLD" : "TENSION";
					ImGui::Text("Type: %s", lineTypeName);
					
					// Show tension parameters only for TENSION segments
					if((*lines)[activeSegmentIndex].type == LINE2_TENSION){
						ImGui::Spacing();
						
						// First row: Asymmetry and Inflection
						ImGui::PushItemWidth(80);
						ImGui::Text("Asymmetry: %.3f", (*lines)[activeSegmentIndex].tensionExponent);
						ImGui::SameLine();
						ImGui::Text("Inflection: %.3f", (*lines)[activeSegmentIndex].inflectionX);
						ImGui::PopItemWidth();
						
						// Second row: Segment B and Global Q
						ImGui::PushItemWidth(80);
						ImGui::Text("Segment B: %.2f", (*lines)[activeSegmentIndex].segmentB);
						ImGui::SameLine();
						ImGui::Text("Global Q: %.1f", globalQ->get());
						ImGui::PopItemWidth();
					}
				}
				
				// Show global parameters when no specific element is active
				if(!showPointInfo && !showSegmentInfo){
					ImGui::Text("Global Parameters:");
					ImGui::SameLine();
					ImGui::PushItemWidth(80);
					if(globalQ != nullptr){
						ImGui::Text("Q: %.1f", globalQ->get());
					} else {
						ImGui::Text("Q: N/A");
					}
					ImGui::PopItemWidth();
					
					ImGui::Spacing();
					ImGui::Text("Hover over points or drag to see details");
				}
			}
			
			// Re-push style vars for drawing operations
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
			ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(50, 50, 50, 200));
			
			// ========================================================================
			// PHASE 4: Multi-curve rendering system
			// ========================================================================
			// This implementation provides:
			// - All enabled curves are drawn simultaneously with different colors/opacity
			// - Active curve is highlighted with full opacity and thicker lines (3px)
			// - Inactive curves are dimmed (50% opacity) with thinner lines (1.5px)
			// - Only the active curve shows interactive points for editing
			// - Proper layering: inactive curves drawn first, active curve on top
			// - Each curve uses its own color, opacity, and mathematical parameters
			// ========================================================================
			
			// Draw all enabled curves with proper visual hierarchy
			
			// First pass: Draw all inactive curves (background layer)
			for(int curveIdx = 0; curveIdx < curves.size(); curveIdx++){
				auto& curve = curves[curveIdx];
				
				// Skip disabled curves
				if(!curve.enabled.get()) continue;
				
				// Skip active curve (will be drawn in second pass) unless "None" is selected
				if(activeCurve.get() >= 0 && curveIdx == activeCurve.get()) continue;
				
				// Enhanced visual feedback for inactive curves
				float inactiveOpacity;
				float lineWidth;
				if(activeCurve.get() >= 0) {
					// When a specific curve is selected, inactive curves at 50% alpha with single line width
					inactiveOpacity = 0.5f;
					lineWidth = 1.5f;
				} else {
					// When "None" is selected, all curves at full opacity with single line width
					inactiveOpacity = 1.0f;
					lineWidth = 1.5f;
				}
								
				ofColor curveColor = curve.color.get();
				ImU32 inactiveCurveColor = IM_COL32(curveColor.r, curveColor.g, curveColor.b, (int)(255 * inactiveOpacity));
				
				// Draw curve start extension line
				if(!curve.points.empty()){
					draw_list->AddLine(denormalizePoint(glm::vec2(0, curve.points[0].point.y)),
									   denormalizePoint(curve.points[0].point),
									   IM_COL32(curveColor.r * 0.4f, curveColor.g * 0.4f, curveColor.b * 0.4f, (int)(255 * inactiveOpacity)));
				}
				
				// Draw curve segments
				for(int i = 0; i < curve.points.size()-1; i++){
					if(curve.lines[i].type == LINE2_HOLD){
						auto p1 = denormalizePoint(curve.points[i].point);
						auto p2 = denormalizePoint(curve.points[i+1].point);
						draw_list->AddLine(p1, glm::vec2(p2.x, p1.y), inactiveCurveColor, lineWidth); // PHASE 5: Dynamic line width
						draw_list->AddLine(glm::vec2(p2.x, p1.y), p2, inactiveCurveColor, lineWidth);
					}
					else if(curve.lines[i].type == LINE2_TENSION){
						// Draw asymmetric logistic curve for inactive curves
						float segmentB = curve.lines[i].segmentB;
						int baseSegments = 30; // Reduced segments for inactive curves for performance
						int adaptiveSegments = std::max(baseSegments, (int)(segmentB * 1.5f));
						const int maxSegments = 300;
						const int numSegments = std::min(adaptiveSegments, maxSegments);
						
						glm::vec2 prevPoint = curve.points[i].point;
						
						// Asymmetric logistic parameters
						float A = curve.points[i].point.y;
						float K = curve.points[i+1].point.y;
						float M = curve.lines[i].inflectionX;
						float nu = curve.lines[i].tensionExponent;
						float B = curve.lines[i].segmentB;
						float Q = curve.globalQ.get();
						
						// Calculate normalization values at segment endpoints
						auto logisticFunction = [&](float x) -> float {
							float logisticTerm = 1.0f + Q * exp(-B * (x - M));
							return A + (K - A) / pow(logisticTerm, 1.0f / nu);
						};
						
						float logistic_0 = logisticFunction(0.0f);
						float logistic_1 = logisticFunction(1.0f);
						float logisticRange = logistic_1 - logistic_0;
						bool useNormalization = abs(logisticRange) > 1e-6f;
						
						for(int seg = 1; seg <= numSegments; seg++){
							float normalizedX = (float)seg / numSegments;
							float segmentX = glm::mix(curve.points[i].point.x, curve.points[i+1].point.x, normalizedX);
							
							float resultY;
							if(useNormalization){
								float raw_logistic = logisticFunction(normalizedX);
								resultY = A + (K - A) * (raw_logistic - logistic_0) / logisticRange;
							} else {
								resultY = A + (K - A) * normalizedX;
							}
							
							glm::vec2 currentPoint = glm::vec2(segmentX, resultY);
							auto p1 = denormalizePoint(prevPoint);
							auto p2 = denormalizePoint(currentPoint);
							draw_list->AddLine(p1, p2, inactiveCurveColor, lineWidth); // PHASE 5: Dynamic line width
							
							prevPoint = currentPoint;
						}
					}
				}
				
				// Draw curve end extension line
				if(!curve.points.empty()){
					draw_list->AddLine(denormalizePoint(curve.points.back().point),
									   denormalizePoint(glm::vec2(1, curve.points.back().point.y)),
									   IM_COL32(curveColor.r * 0.4f, curveColor.g * 0.4f, curveColor.b * 0.4f, (int)(255 * inactiveOpacity)));
				}
			}
			
			// Second pass: Draw active curve (foreground layer) with full interactivity
			// Only draw if a specific curve is selected (not "None")
			if(activeCurve.get() >= 0 && activeCurve.get() < curves.size() && curves[activeCurve.get()].enabled.get()){
				auto& activeCurveData = curves[activeCurve.get()];
				ofColor activeCurveColor = activeCurveData.color.get();
				
				// Draw active curve start extension line
				if (points != nullptr && !points->empty()) {
					draw_list->AddLine(denormalizePoint(glm::vec2(0, (*points)[0].point.y)), denormalizePoint((*points)[0].point), IM_COL32(10, 10, 10, 255));
				}
				
				if(points != nullptr && lines != nullptr){
					for(int i = 0; i < points->size()-1; i++){
						// Determine segment color based on drag state for active curve
						bool whiteHighlight = false;
						bool activeHighlight = false;
						
						// Check for tension drag highlighting (shift+drag) - use white
						if(tensionDragActive && tensionDragSegmentIndex == i){
							whiteHighlight = true;
						}
						
						// Check for B parameter drag highlighting (ctrl+drag) - use white
						if(bDragActive && bDragSegmentIndex == i){
							whiteHighlight = true;
						}
						
						// Check for normal point drag highlighting - use active curve color
						if(!whiteHighlight && points != nullptr){
							for(int p = 0; p < points->size(); p++){
								if((*points)[p].drag == 3){
									// Highlight segments connected to the dragged point
									if((p == i) || (p == i + 1)){
										activeHighlight = true;
										break;
									}
								}
							}
						}
						
						// Choose color based on highlighting state for active curve
						ImU32 segmentColor;
						float lineWidth = 2.0f; // Double line width for active curve
						
						if(whiteHighlight){
							segmentColor = IM_COL32(255, 255, 255, 255); // White for shift/ctrl drag
						} else if(activeHighlight){
							segmentColor = IM_COL32(activeCurveColor.r, activeCurveColor.g, activeCurveColor.b, 255); // Full opacity for drag
						} else {
							segmentColor = IM_COL32(activeCurveColor.r, activeCurveColor.g, activeCurveColor.b, 255); // Full opacity for active curve
						}
						
						if((*lines)[i].type == LINE2_HOLD){
							auto p1 = denormalizePoint((*points)[i].point);
							auto p2 = denormalizePoint((*points)[i+1].point);
							draw_list->AddLine(p1, glm::vec2(p2.x, p1.y), segmentColor, lineWidth);
							draw_list->AddLine(glm::vec2(p2.x, p1.y), p2, segmentColor, lineWidth);
						}
						else if((*lines)[i].type == LINE2_TENSION){
							// Draw asymmetric logistic curve with proper normalization for active curve
							// Adaptive resolution based on B parameter - more segments for steeper curves
							float segmentB = (*lines)[i].segmentB;
							int baseSegments = 50;
							int adaptiveSegments = std::max(baseSegments, (int)(segmentB * 2.0f)); // 2 segments per B unit
							// Cap maximum segments to prevent performance issues
							const int maxSegments = 500;
							const int numSegments = std::min(adaptiveSegments, maxSegments);
							
							glm::vec2 prevPoint = (*points)[i].point;
							
							// Asymmetric logistic parameters
							float A = (*points)[i].point.y;      // Y value of segment start point
							float K = (*points)[i+1].point.y;    // Y value of segment end point
							float M = (*lines)[i].inflectionX;   // Inflection point X position (0-1)
							float nu = (*lines)[i].tensionExponent; // Asymmetry parameter ν
							float B = (*lines)[i].segmentB;      // Per-segment B parameter
							float Q = (globalQ != nullptr) ? globalQ->get() : 1.0f;          // Global Q parameter
							
							// Calculate normalization values at segment endpoints
							auto logisticFunction = [&](float x) -> float {
								float logisticTerm = 1.0f + Q * exp(-B * (x - M));
								return A + (K - A) / pow(logisticTerm, 1.0f / nu);
							};
							
							float logistic_0 = logisticFunction(0.0f); // Value at segment start
							float logistic_1 = logisticFunction(1.0f); // Value at segment end
							float logisticRange = logistic_1 - logistic_0;
							
							// Handle edge case where logistic range is very small
							bool useNormalization = abs(logisticRange) > 1e-6f;
							
							for(int seg = 1; seg <= numSegments; seg++){
								float normalizedX = (float)seg / numSegments;
								
								// Calculate normalized X position within the segment
								float segmentX = glm::mix((*points)[i].point.x, (*points)[i+1].point.x, normalizedX);
								
								float resultY;
								if(useNormalization){
									// Apply normalized logistic function to guarantee endpoint continuity
									float raw_logistic = logisticFunction(normalizedX);
									resultY = A + (K - A) * (raw_logistic - logistic_0) / logisticRange;
								} else {
									// Fall back to linear interpolation if logistic range is too small
									resultY = A + (K - A) * normalizedX;
								}
								
								glm::vec2 currentPoint = glm::vec2(segmentX, resultY);
								
								// Draw line segment from previous point to current point
								auto p1 = denormalizePoint(prevPoint);
								auto p2 = denormalizePoint(currentPoint);
								draw_list->AddLine(p1, p2, segmentColor, lineWidth);
								
								prevPoint = currentPoint;
							}
						}
					}
				}
				
				// Draw active curve end extension line
				if(points != nullptr && !points->empty()){
					draw_list->AddLine(denormalizePoint(points->back().point), denormalizePoint(glm::vec2(1, points->back().point.y)), IM_COL32(10, 10, 10, 255));
				}
			}
			
			// Draw vertical line at inflection point during Shift+drag OR CTRL+drag operations OR hover with modifier keys
			bool showInflectionLine = false;
			int inflectionSegmentIndex = -1;
			
			// Check for active drag operations
			if((tensionDragActive && tensionDragSegmentIndex >= 0) ||
			   (bDragActive && bDragSegmentIndex >= 0)){
				showInflectionLine = true;
				inflectionSegmentIndex = tensionDragActive ? tensionDragSegmentIndex : bDragSegmentIndex;
			}
			// Check for hover with modifier keys
			else if(hoveredSegmentIndex >= 0 && canvasHovered &&
					(ImGui::GetIO().KeyShift || ImGui::GetIO().KeyCtrl)){
				showInflectionLine = true;
				inflectionSegmentIndex = hoveredSegmentIndex;
			}
			
			if(showInflectionLine && inflectionSegmentIndex >= 0 && points != nullptr && lines != nullptr){
				
				// Ensure segment index is valid
				if(inflectionSegmentIndex < lines->size()){
					// Get the current segment being modified
					float segmentStartX = (*points)[inflectionSegmentIndex].point.x;
					float segmentEndX = (*points)[inflectionSegmentIndex + 1].point.x;
					
					// Calculate the actual X position of the inflection point
					float inflectionX = (*lines)[inflectionSegmentIndex].inflectionX;
					float lineX = segmentStartX + inflectionX * (segmentEndX - segmentStartX);
					
					// Convert to screen coordinates
					glm::vec2 lineTopPoint = denormalizePoint(glm::vec2(lineX, 0.0f));
					glm::vec2 lineBottomPoint = denormalizePoint(glm::vec2(lineX, 1.0f));
					
					// Draw dotted vertical line with semi-transparent yellow color
					drawDottedLine(draw_list, lineTopPoint, lineBottomPoint, IM_COL32(255, 255, 0, 64));
				}
			}
			
			// Draw cyan horizontal line for asymmetry value during Shift+drag OR CTRL+drag operations OR hover with modifier keys
			bool showAsymmetryLine = false;
			int asymmetrySegmentIndex = -1;
			
			// Check for active drag operations
			if((tensionDragActive && tensionDragSegmentIndex >= 0) ||
			   (bDragActive && bDragSegmentIndex >= 0)){
				showAsymmetryLine = true;
				asymmetrySegmentIndex = tensionDragActive ? tensionDragSegmentIndex : bDragSegmentIndex;
			}
			// Check for hover with modifier keys
			else if(hoveredSegmentIndex >= 0 && canvasHovered &&
					(ImGui::GetIO().KeyShift || ImGui::GetIO().KeyCtrl)){
				showAsymmetryLine = true;
				asymmetrySegmentIndex = hoveredSegmentIndex;
			}
			
			if(showAsymmetryLine && asymmetrySegmentIndex >= 0 && lines != nullptr){
				// Ensure segment index is valid
				if(asymmetrySegmentIndex < lines->size()){
					// Get current asymmetry value
					float currentAsymmetry = (*lines)[asymmetrySegmentIndex].tensionExponent;
					
					// Define asymmetry range (same as in the drag calculation)
					const float minAsymmetry = MIN_ASYMMETRY;
					const float maxAsymmetry = MAX_ASYMMETRY;
					
					// Map asymmetry value to Y position using enhanced split-zone mapping
					// This mirrors the enhanced drag mapping for consistent visual feedback
					float lineY;
					
					if (currentAsymmetry <= 1.0f) {
						// Lower zone: asymmetry 0.02-1.0 maps to Y 0.0-0.5 (bottom to middle)
						float normalizedAsymmetry = (currentAsymmetry - minAsymmetry) / (1.0f - minAsymmetry);
						lineY = normalizedAsymmetry * 0.5f; // Scale to lower half (0.0 to 0.5)
					} else {
						// Upper zone: asymmetry 1.0-100.0 maps to Y 0.5-1.0 (middle to top)
						float normalizedAsymmetry = (currentAsymmetry - 1.0f) / (maxAsymmetry - 1.0f);
						lineY = 0.5f + normalizedAsymmetry * 0.5f; // Scale to upper half (0.5 to 1.0)
					}
					
					// Clamp to valid range
					lineY = std::max(0.0f, std::min(1.0f, lineY));
					
					// Convert to screen coordinates
					glm::vec2 lineLeftPoint = denormalizePoint(glm::vec2(0.0f, lineY));
					glm::vec2 lineRightPoint = denormalizePoint(glm::vec2(1.0f, lineY));
					
					// Draw dotted horizontal line with semi-transparent cyan color
					drawDottedLine(draw_list, lineLeftPoint, lineRightPoint, IM_COL32(0, 255, 255, 64));
				}
			}
			
			// Draw red vertical line for B parameter position during Shift+drag OR CTRL+drag operations OR hover with modifier keys
			bool showBParameterLine = false;
			int bParameterSegmentIndex = -1;
			
			// Check for active drag operations
			if((tensionDragActive && tensionDragSegmentIndex >= 0) ||
			   (bDragActive && bDragSegmentIndex >= 0)){
				showBParameterLine = true;
				bParameterSegmentIndex = tensionDragActive ? tensionDragSegmentIndex : bDragSegmentIndex;
			}
			// Check for hover with modifier keys
			else if(hoveredSegmentIndex >= 0 && canvasHovered &&
					(ImGui::GetIO().KeyShift || ImGui::GetIO().KeyCtrl)){
				showBParameterLine = true;
				bParameterSegmentIndex = hoveredSegmentIndex;
			}
			
			if(showBParameterLine && bParameterSegmentIndex >= 0 && points != nullptr && lines != nullptr){
				// Ensure segment index is valid
				if(bParameterSegmentIndex < lines->size()){
					// Get the current segment being modified
					float segmentStartX = (*points)[bParameterSegmentIndex].point.x;
					float segmentEndX = (*points)[bParameterSegmentIndex + 1].point.x;
					float segmentWidth = segmentEndX - segmentStartX;
					
					// Get current B parameter value
					float currentB = (*lines)[bParameterSegmentIndex].segmentB;
					
					// Map B parameter (0.01 to 100.0) to normalized position within segment (0.0 to 1.0)
					// This represents where the B parameter affects the curve steepness within the segment
					const float minB = MIN_B_PARAMETER;
					const float maxB = MAX_B_PARAMETER;
					
					// Normalize B value to 0.0-1.0 range
					float normalizedB = (currentB - minB) / (maxB - minB);
					normalizedB = std::max(0.0f, std::min(1.0f, normalizedB));
					
					// Calculate the actual X position of the B parameter line within the segment
					// The line position directly represents the normalized B parameter value (inverted for intuitive behavior)
					float lineX = segmentStartX + (1.0f - normalizedB) * segmentWidth;
					
					// Convert to screen coordinates
					glm::vec2 lineTopPoint = denormalizePoint(glm::vec2(lineX, 0.0f));
					glm::vec2 lineBottomPoint = denormalizePoint(glm::vec2(lineX, 1.0f));
					
					// Draw dotted vertical line with semi-transparent red color
					drawDottedLine(draw_list, lineTopPoint, lineBottomPoint, IM_COL32(255, 0, 0, 64));
				}
			}
			
			// PHASE 5: Draw curve name labels
			if(showCurveLabels){
				// Draw labels for all enabled curves
				for(int curveIdx = 0; curveIdx < curves.size(); curveIdx++){
					auto& curve = curves[curveIdx];
					
					// Skip disabled curves
					if(!curve.enabled.get()) continue;
					
					// Use actual curve name
					string curveName = curve.name.get();
					
					// Determine label position and color
					ofColor curveColor = curve.color.get();
					ImU32 labelColor;
					float labelOpacity = 0.8f;
					
					if(curveIdx == activeCurve.get()){
						// Active curve: full opacity, add "[ACTIVE]" suffix
						curveName += " [ACTIVE]";
						labelColor = IM_COL32(curveColor.r, curveColor.g, curveColor.b, (int)(255 * labelOpacity));
					}
					else {
						// Inactive curve: lower opacity
						labelOpacity = 0.6f;
						labelColor = IM_COL32(curveColor.r, curveColor.g, curveColor.b, (int)(255 * labelOpacity));
					}
					
					// Position label in top-left area, stacked vertically
					ImVec2 labelPos = ImVec2(visual_win_pos.x + 10, visual_win_pos.y + 10 + curveIdx * 20);
					
					// Add background for better readability
					ImVec2 textSize = ImGui::CalcTextSize(curveName.c_str());
					draw_list->AddRectFilled(
											 ImVec2(labelPos.x - 2, labelPos.y - 2),
											 ImVec2(labelPos.x + textSize.x + 2, labelPos.y + textSize.y + 2),
											 IM_COL32(0, 0, 0, 120) // Semi-transparent black background
											 );
					
					// Draw the label text
					draw_list->AddText(labelPos, labelColor, curveName.c_str());
				}
			}
			
			// Always Draw vertices for refernce for editing the curves
			if(true)
			{
				// "None" selected: Draw all vertices from all enabled curves as visual reference (no interaction)
				for(int curveIdx = 0; curveIdx < curves.size(); curveIdx++){
					auto& curve = curves[curveIdx];
					
					// Skip disabled curves
					if(!curve.enabled.get()) continue;
					
					ofColor curveColor = curve.color.get();
					
					for(int i = 0; i < curve.points.size(); i++){
						glm::vec2 pointPos = denormalizePoint(curve.points[i].point);
						
						// Draw small, non-interactive vertices with curve color
						draw_list->AddCircleFilled(pointPos, 3, IM_COL32(curveColor.r, curveColor.g, curveColor.b, 180));
						draw_list->AddCircle(pointPos, 3, IM_COL32(255, 255, 255, 100), 0, 1); // Light outline
					}
				}
			}
			// Interactive drawing points
			if(activeCurve.get() >= 0 && activeCurve.get() < curves.size() && curves[activeCurve.get()].enabled.get() && points != nullptr){
				// Specific curve selected: Draw interactive points for the active curve only
				auto& activeCurveData = curves[activeCurve.get()];
				ofColor activeCurveColor = activeCurveData.color.get();
				
				for(int i = 0; i < points->size(); i++){
					auto &p = (*points)[i];
					glm::vec2 pointPos = denormalizePoint(p.point);
					
					if(i == hoveredPointIndex){
						// Draw highlighted point with larger radius and different color
						draw_list->AddCircleFilled(pointPos, 8, IM_COL32(255, 255, 255, 255)); // White highlight
						draw_list->AddCircle(pointPos, 8, IM_COL32(activeCurveColor.r, activeCurveColor.g, activeCurveColor.b, 255), 0, 2); // Active curve colored outline
						draw_list->AddCircleFilled(pointPos, 5, IM_COL32(0, 0, 0, 255)); // Original black center
					} else {
						// Draw normal point for active curve
						draw_list->AddCircleFilled(pointPos, 5, IM_COL32(activeCurveColor.r, activeCurveColor.g, activeCurveColor.b,255));
					}
				}
			}
			
			for(auto &v : input.get()){
				auto mv = ofMap(v, minX, maxX, 0, 1);
				draw_list->AddLine(denormalizePoint(glm::vec2(mv, 0)), denormalizePoint(glm::vec2(mv, 1)), IM_COL32(127, 127, 127, 127));
			}
			
			// Pop the style vars that were re-pushed for drawing operations
			ImGui::PopStyleColor();
			ImGui::PopStyleVar(2);
			
			ImGui::EndGroup();
		}
		ImGui::End();
	}
}

void curve2::recalculate()
{
	if(input.get().empty()) return;
	
	const size_t inputSize = input.get().size();
	
	// Initialize consolidated output structure - simple vector with one value per curve
	vector<float> consolidatedOutput(curves.size(), 0.0f);
	
	// Process each curve and generate its corresponding output
	for(int curveIdx = 0; curveIdx < curves.size() && curveIdx < outputs.size(); curveIdx++){
		auto& curve = curves[curveIdx];
		
		// PHASE 5: Error handling - validate curve data
		if(curve.points.empty()){
			// Initialize with default points if empty
			curve.points.emplace_back(0, 0);
			curve.points.emplace_back(1, 1);
			curve.points.front().firstCreated = false;
			curve.points.back().firstCreated = false;
			curve.lines.emplace_back();
		}
		
		vector<float> tempOut(inputSize);
		
		// PHASE 5: Performance optimization - early exit for disabled curves
		if(!curve.enabled.get()){
			// Output zero values for disabled curves
			fill(tempOut.begin(), tempOut.end(), 0.0f);
			// DIAGNOSTIC: Check shared_ptr before assignment
			if(outputs[curveIdx]) {
				*outputs[curveIdx] = tempOut;
			} else {
				ofLogError("curve2::recalculate") << "NULL shared_ptr for disabled curve " << curveIdx;
			}
			// Update consolidated output with zero for this curve
			consolidatedOutput[curveIdx] = 0.0f;
			continue;
		}
		
		for(int i = 0; i < input.get().size(); i++){
			auto p = ofMap(input.get().at(i), minX.get(), maxX.get(), 0, 1);
			if(p <= curve.points[0].point.x){
				tempOut[i] = curve.points[0].point.y; // Direct 0-1 range output
			}else if(p >= curve.points.back().point.x){
				tempOut[i] = curve.points.back().point.y; // Direct 0-1 range output
			}else{
				for(int j = 1; j < curve.points.size(); j++){
					if(p < curve.points[j].point.x){
						if(curve.lines[j-1].type == LINE2_HOLD){
							float resultY = curve.points[j-1].point.y;
							tempOut[i] = resultY; // Direct 0-1 range output
						}
						else if(curve.lines[j-1].type == LINE2_TENSION){
							// Calculate normalized X position within the segment
							float normalizedX = ofMap(p, curve.points[j-1].point.x, curve.points[j].point.x, 0, 1);
							
							// Asymmetric logistic parameters
							float A = curve.points[j-1].point.y;      // Y value of segment start point
							float K = curve.points[j].point.y;        // Y value of segment end point
							float M = curve.lines[j-1].inflectionX;   // Inflection point X position (0-1)
							float nu = curve.lines[j-1].tensionExponent; // Asymmetry parameter ν
							float B = curve.lines[j-1].segmentB;      // Per-segment B parameter
							float Q = curve.lines[j-1].segmentQ;      // Per-segment Q parameter (FIXED: no longer global)
							
							// Calculate normalization values at segment endpoints
							auto logisticFunction = [&](float x) -> float {
								float logisticTerm = 1.0f + Q * exp(-B * (x - M));
								return A + (K - A) / pow(logisticTerm, 1.0f / nu);
							};
							
							float logistic_0 = logisticFunction(0.0f); // Value at segment start
							float logistic_1 = logisticFunction(1.0f); // Value at segment end
							float logisticRange = logistic_1 - logistic_0;
							
							float resultY;
							// Handle edge case where logistic range is very small
							if(abs(logisticRange) > 1e-6f){
								// Apply normalized logistic function to guarantee endpoint continuity
								float raw_logistic = logisticFunction(normalizedX);
								resultY = A + (K - A) * (raw_logistic - logistic_0) / logisticRange;
							} else {
								// Fall back to linear interpolation if logistic range is too small
								resultY = A + (K - A) * normalizedX;
							}
							
							tempOut[i] = resultY; // Direct 0-1 range output
						}
						break;
					}
				}
			}
			// Update consolidated output with this curve's result (use first input value)
			if(i == 0) {
				consolidatedOutput[curveIdx] = tempOut[i];
			}
		}
		if(outputs[curveIdx]) {
			*outputs[curveIdx] = tempOut;
		}
	}
	
	// Update the consolidated output parameter
	allCurvesOutput = consolidatedOutput;
}

void curve2::presetSave(ofJson &json){
	// Save multi-curve data
	json["NumCurves"] = curves.size();
	json["NumOutputs"] = outputs.size();
	json["ActiveCurve"] = activeCurve.get();
	
	// Save consolidated output parameter
	json["AllCurvesOutput"] = allCurvesOutput.get();
	
	for(int curveIdx = 0; curveIdx < curves.size(); curveIdx++){
		auto& curve = curves[curveIdx];
		auto& curveJson = json["Curves"][curveIdx];
		
		// Save curve metadata
		curveJson["Name"] = curve.name.get();
		curveJson["Color"]["r"] = curve.color.get().r;
		curveJson["Color"]["g"] = curve.color.get().g;
		curveJson["Color"]["b"] = curve.color.get().b;
		curveJson["Color"]["a"] = curve.color.get().a;
		curveJson["Enabled"] = curve.enabled.get();
		curveJson["GlobalQ"] = curve.globalQ.get();
		
		// Save points
		curveJson["NumPoints"] = curve.points.size();
		for(int i = 0; i < curve.points.size(); i++){
			curveJson["Points"][i]["Point"]["x"] = curve.points[i].point.x;
			curveJson["Points"][i]["Point"]["y"] = curve.points[i].point.y;
		}
		
		// Save lines
		for(int i = 0; i < curve.lines.size(); i++){
			curveJson["Lines"][i]["Type"] = curve.lines[i].type;
			curveJson["Lines"][i]["TensionExponent"] = curve.lines[i].tensionExponent;
			curveJson["Lines"][i]["InflectionX"] = curve.lines[i].inflectionX;
			curveJson["Lines"][i]["SegmentB"] = curve.lines[i].segmentB;
			curveJson["Lines"][i]["SegmentQ"] = curve.lines[i].segmentQ;
		}
	}
}

void curve2::presetRecallAfterSettingParameters(ofJson &json){
	try{
		// Check if this is a new multi-curve format
		if(json.contains("NumCurves") && json.contains("Curves")){
			// Load multi-curve format
			int numCurvesFromJson = json["NumCurves"];
			curves.resize(numCurvesFromJson);
			
			for(int curveIdx = 0; curveIdx < numCurvesFromJson; curveIdx++){
				auto& curveJson = json["Curves"][curveIdx];
				auto& curve = curves[curveIdx];
				
				// Load curve metadata
				if(curveJson.contains("Name")){
					curve.name.set(curveJson["Name"]);
				} else {
					// Backward compatibility: set default name
					curve.name.set("Curve " + ofToString(curveIdx + 1));
				}
				if(curveJson.contains("Color")){
					ofColor loadedColor;
					loadedColor.r = curveJson["Color"]["r"];
					loadedColor.g = curveJson["Color"]["g"];
					loadedColor.b = curveJson["Color"]["b"];
					loadedColor.a = curveJson["Color"]["a"];
					curve.color.set(loadedColor);
				}
				if(curveJson.contains("Enabled")){
					curve.enabled.set(curveJson["Enabled"]);
				}
				if(curveJson.contains("GlobalQ")){
					curve.globalQ.set(curveJson["GlobalQ"]);
				}
				
				// Load points
				if(curveJson.contains("NumPoints")){
					int numPoints = curveJson["NumPoints"];
					curve.points.resize(numPoints);
					curve.lines.resize(numPoints - 1);
					
					for(int i = 0; i < numPoints; i++){
						curve.points[i].point.x = curveJson["Points"][i]["Point"]["x"];
						curve.points[i].point.y = curveJson["Points"][i]["Point"]["y"];
						curve.points[i].firstCreated = false;
					}
					
					// Load lines
					for(int i = 0; i < curve.lines.size(); i++){
						curve.lines[i].type = static_cast<lineType2>(curveJson["Lines"][i]["Type"]);
						
						if(curveJson["Lines"][i].contains("TensionExponent")){
							curve.lines[i].tensionExponent = curveJson["Lines"][i]["TensionExponent"];
						} else {
							curve.lines[i].tensionExponent = 1.0f;
						}
						
						if(curveJson["Lines"][i].contains("InflectionX")){
							curve.lines[i].inflectionX = curveJson["Lines"][i]["InflectionX"];
						} else {
							curve.lines[i].inflectionX = 0.5f;
						}
						
						if(curveJson["Lines"][i].contains("SegmentB")){
							curve.lines[i].segmentB = curveJson["Lines"][i]["SegmentB"];
						} else {
							curve.lines[i].segmentB = 6.0f;
						}
						
						if(curveJson["Lines"][i].contains("SegmentQ")){
							curve.lines[i].segmentQ = curveJson["Lines"][i]["SegmentQ"];
						} else {
							curve.lines[i].segmentQ = 1.0f;
						}
					}
				}
			}
			
			
			// Load outputs count (default to numCurves for backward compatibility)
			int numOutputsFromJson = json.contains("NumOutputs") ? static_cast<int>(json["NumOutputs"]) : numCurvesFromJson;
			resizeOutputs(numOutputsFromJson);
			
			// BUGFIX: Synchronize output parameter names with loaded curve names
			// This fixes the issue where output parameter names were set before curve names were loaded
			//			for(int i = 0; i < outputs.size() && i < curves.size(); i++) {
			//				if(outputs[i]) {
			//					ofLogNotice("curve2::loadFromJson") << "Setting output " << i << " name to: " << curves[i].name.get();
			//					outputs[i]->setName(curves[i].name.get());
			//				} else {
			//					ofLogError("curve2::loadFromJson") << "NULL shared_ptr for output " << i;
			//				}
			//			}
			
			// Always set active curve to "none" (-1) when loading presets
			activeCurve.set(-1);
			
			// Restore consolidated output parameter if available
			if(json.contains("AllCurvesOutput")){
				// Handle both old vector<vector<float>> and new vector<float> formats
				auto savedOutput = json["AllCurvesOutput"];
				if(savedOutput.is_array() && !savedOutput.empty()) {
					if(savedOutput[0].is_array()) {
						// Old format: vector<vector<float>> - convert to new format
						// Use the first input's values as the consolidated output
						vector<float> convertedOutput;
						for(const auto& curveOutputs : savedOutput[0]) {
							convertedOutput.push_back(curveOutputs);
						}
						allCurvesOutput.set(convertedOutput);
					} else {
						// New format: vector<float> - use directly
						allCurvesOutput.set(savedOutput);
					}
				}
			}
			
			// Update numCurves parameter
			numCurves.set(curves.size());
			activeCurve.setMax(curves.size() - 1);
		}
		else {
			// Load legacy single-curve format
			curves.resize(1);
			auto& curve = curves[0];
			
			// For legacy format, ensure we have one output
			resizeOutputs(1);
			
			curve.points.resize(json["NumPoints"]);
			curve.lines.resize(curve.points.size()-1);
			
			for(int i = 0; i < curve.points.size(); i++){
				curve.points[i].point.x = json["Points"][i]["Point"]["x"];
				curve.points[i].point.y = json["Points"][i]["Point"]["y"];
				curve.points[i].firstCreated = false;
			}
			
			for(int i = 0; i < curve.lines.size(); i++){
				curve.lines[i].type = static_cast<lineType2>(json["Lines"][i]["Type"]);
				
				if(json["Lines"][i].contains("TensionExponent")){
					curve.lines[i].tensionExponent = json["Lines"][i]["TensionExponent"];
				} else {
					curve.lines[i].tensionExponent = 1.0f;
				}
				
				if(json["Lines"][i].contains("InflectionX")){
					curve.lines[i].inflectionX = json["Lines"][i]["InflectionX"];
				} else {
					curve.lines[i].inflectionX = 0.5f;
				}
				
				if(json["Lines"][i].contains("SegmentB")){
					curve.lines[i].segmentB = json["Lines"][i]["SegmentB"];
				} else {
					curve.lines[i].segmentB = 6.0f;
				}
				
				if(json["Lines"][i].contains("SegmentQ")){
					curve.lines[i].segmentQ = json["Lines"][i]["SegmentQ"];
				} else {
					curve.lines[i].segmentQ = 1.0f;
				}
			}
			//
			//			// Load global logistic parameters
			//			if(json.contains("GlobalQ")){
			//				curve.globalQ.set(json["GlobalQ"]);
			//			} else {
			//				curve.globalQ.set(1.0f);
			//			}
			
			numCurves.set(1);
			activeCurve.set(-1);
			activeCurve.setMax(0);
		}
		
		// Update references to active curve
		int currentActiveCurve = activeCurve.get();
		onActiveCurveChanged(currentActiveCurve);
		
		// Trigger recalculation to populate consolidated output
		recalculate();
	}
	catch (ofJson::exception& e)
	{
		ofLog() << e.what();
	}
}

// Curve management methods implementation
void curve2::addCurve() {
	curves.emplace_back();
	
	// Set default name for new curve using gap-aware naming
	int newCurveIndex = curves.size() - 1;
	int availableNumber = findNextAvailableCurveNumber();
	curves[newCurveIndex].name.set("Name", "Curve " + ofToString(availableNumber));
	
	// Update activeCurve parameter max value
	activeCurve.setMax(curves.size() - 1);
	
	// Switch to the newly added curve and update pointers
	activeCurve = newCurveIndex;  // Auto-select new curve (not "None")
}

void curve2::removeCurve(int index) {
	if (curves.size() <= 1 || index < 0 || index >= curves.size()) {
		return;
	}
	
	// Remove the specific output parameter BEFORE removing the curve
	if (index < outputs.size()) {
		string outputName = curves[index].name.get();
		removeParameter(outputName);
		outputs.erase(outputs.begin() + index);
	}
	
	// Remove the curve from the vector
	curves.erase(curves.begin() + index);
	
	// Update activeCurve bounds and value
	activeCurve.setMax(curves.size() - 1);
	if (activeCurve >= curves.size()) {
		activeCurve = curves.size() - 1;
	}
	
	// Update remaining output parameter names to match new indices
	for (int i = index; i < outputs.size(); i++) {
		if (i < curves.size() && outputs[i]) {
			outputs[i]->setName(curves[i].name.get());
		}
	}
	
	// Update consolidated output
	vector<float> consolidatedDefault(curves.size(), 0.0f);
	allCurvesOutput = consolidatedDefault;
}

int curve2::findNextAvailableCurveNumber() {
	for(int i = 1; i <= curves.size() + 1; i++) {
		string candidateName = "Curve " + ofToString(i);
		bool nameExists = false;
		for(auto& curve : curves) {
			if(curve.name.get() == candidateName) {
				nameExists = true;
				break;
			}
		}
		if(!nameExists) return i;
	}
	return curves.size() + 1; // Fallback
}

void curve2::resizeCurves(int newCount) {
	if (newCount < 1) newCount = 1;
	if (newCount > 16) newCount = 16;
	
	int currentCount = curves.size();
	
	if (newCount > currentCount) {
		// Add new curves
		for (int i = currentCount; i < newCount; i++) {
			addCurve();
		}
	} else if (newCount < currentCount) {
		// Remove curves from the end
		for (int i = currentCount - 1; i >= newCount; i--) {
			removeCurve(i);
		}
	}
}

CurveData& curve2::getCurrentCurve() {
	int index = activeCurve.get();
	if (index < 0 || index >= curves.size()) {
		// If "None" is selected or invalid index, return first curve as fallback
		index = 0;
	}
	return curves[index];
}

const CurveData& curve2::getCurrentCurve() const {
	int index = activeCurve.get();
	if (index < 0 || index >= curves.size()) {
		// If "None" is selected or invalid index, return first curve as fallback
		index = 0;
	}
	return curves[index];
}

void curve2::onNumCurvesChanged(int& newCount) {
	resizeCurves(newCount);
	resizeOutputs(newCount);
	// Trigger recalculation to update consolidated output with new curve count
	recalculate();
}

void curve2::onActiveCurveChanged(int& newIndex) {
	// Handle "None" selection (-1)
	if (newIndex == -1) {
		// Set pointers to nullptr to indicate no active curve
		points = nullptr;
		lines = nullptr;
		colorParam = nullptr;
		globalQ = nullptr;
		
		// Reset hover states
		hoveredPointIndex = -1;
		hoveredSegmentIndex = -1;
//		hoveredCurveIndex = -1;
		
		// No recalculation needed for overview mode
		return;
	}
	
	// Enhanced error handling and bounds checking
	if (curves.empty()) {
		// Initialize with default curve if empty
		curves.resize(1);
		newIndex = 0;
		activeCurve = newIndex;
	} else if (newIndex < -1 || newIndex >= curves.size()) {
		newIndex = std::max(-1, std::min((int)curves.size() - 1, newIndex));
		activeCurve = newIndex;
		
		// If still -1 after clamping, handle as "None"
		if (newIndex == -1) {
			points = nullptr;
			lines = nullptr;
			colorParam = nullptr;
			globalQ = nullptr;
			hoveredPointIndex = -1;
			hoveredSegmentIndex = -1;
//			hoveredCurveIndex = -1;
			return;
		}
	}
	
	// Additional validation - ensure curve has valid data
	auto& targetCurve = curves[newIndex];
	
	if (targetCurve.points.empty()) {
		// Initialize with default points if empty
		targetCurve.points.emplace_back(0, 0);
		targetCurve.points.emplace_back(1, 1);
		targetCurve.points.front().firstCreated = false;
		targetCurve.points.back().firstCreated = false;
		targetCurve.lines.emplace_back();
	}
	
	// Update pointers to point to the new active curve
	points = &curves[newIndex].points;
	lines = &curves[newIndex].lines;
	colorParam = &curves[newIndex].color;
	globalQ = &curves[newIndex].globalQ;
	
	// Update color
	if(colorParam != nullptr){
		color = colorParam->get();
	}
	
	// Reset hover states when switching curves
	hoveredPointIndex = -1;
	hoveredSegmentIndex = -1;
//	hoveredCurveIndex = -1;
	
	// Trigger recalculation
	recalculate();
}

// Output management methods implementation
void curve2::addOutput() {
	int newIndex = outputs.size();
	// Create new shared_ptr with proper initialization
	outputs.emplace_back(make_shared<ofParameter<vector<float>>>());
	string outputName = (newIndex < curves.size()) ? curves[newIndex].name.get() : ("Curve " + ofToString(newIndex + 1));
	
	if(outputs[newIndex]) {
		addOutputParameter(outputs[newIndex]->set(outputName, {0}, {0}, {1}));
	}
}

void curve2::removeOutput() {
	if (outputs.size() <= 1) {
		return; // Cannot remove if only one output
	}
	
	int lastIndex = outputs.size() - 1;
	string outputName = (lastIndex < curves.size()) ? curves[lastIndex].name.get() : ("Curve " + ofToString(lastIndex + 1));
	
	removeParameter(outputName);
	outputs.pop_back();
}

void curve2::resizeOutputs(int count) {
	if (count < 1) count = 1;
	if (count > 16) count = 16;
	
	int currentCount = outputs.size();
	
	if (count > currentCount) {
		// Add new outputs
		for (int i = currentCount; i < count; i++) {
			addOutput();
		}
	} else if (count < currentCount) {
		// Remove outputs from the end
		for (int i = currentCount - 1; i >= count; i--) {
			removeOutput();
		}
	}
	
	// Update consolidated output parameter size to match new curve count
	// Initialize with simple vector structure
	vector<float> consolidatedDefault(count, 0.0f);
	allCurvesOutput = consolidatedDefault;
}

void curve2::renderInspectorInterface() {
	// Single unified inspector interface without tabs
	
	// Global Parameters Section (includes curve management)
	if (ImGui::CollapsingHeader("Global Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
		float minXVal = minX.get();
		if (ImGui::SliderFloat("Min X", &minXVal, -1000.0f, 1000.0f)) {
			minX = minXVal;
		}
		float maxXVal = maxX.get();
		if (ImGui::SliderFloat("Max X", &maxXVal, -1000.0f, 1000.0f)) {
			maxX = maxXVal;
		}
		int horDiv = numHorizontalDivisions.get();
		if (ImGui::SliderInt("Horizontal Divisions", &horDiv, 1, 512)) {
			numHorizontalDivisions = horDiv;
		}
		int vertDiv = numVerticalDivisions.get();
		if (ImGui::SliderInt("Vertical Divisions", &vertDiv, 1, 512)) {
			numVerticalDivisions = vertDiv;
		}
		bool snapGrid = snapToGrid.get();
		if (ImGui::Checkbox("Snap to Grid", &snapGrid)) {
			snapToGrid = snapGrid;
		}
		bool showInfoVal = showInfo.get();
		if (ImGui::Checkbox("Show Info", &showInfoVal)) {
			showInfo = showInfoVal;
		}
		
		// Add curve labels toggle
		if (ImGui::Checkbox("Show Curve Labels", &showCurveLabels)) {
			// Toggle handled by direct assignment
		}
		
	}
	
	// Active Curve Properties Section (hidden when "None" is selected)
	// Only contains Global Q parameter - other parameters moved to Reset button line
	if (activeCurve.get() >= 0 && ImGui::CollapsingHeader("Active Curve Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
		auto& currentCurve = getCurrentCurve();
		
		// Global Q parameter (only parameter kept in this section)
		float globalQVal = currentCurve.globalQ.get();
		if (ImGui::SliderFloat("Global Q", &globalQVal, 0.1f, 5.0f)) {
			currentCurve.globalQ = globalQVal;
			recalculate();
		}
	}
}

// Helper function to draw dotted lines
void curve2::drawDottedLine(ImDrawList* drawList, ImVec2 start, ImVec2 end, ImU32 color, float dashLength, float gapLength) {
	ImVec2 direction = ImVec2(end.x - start.x, end.y - start.y);
	float totalLength = sqrt(direction.x * direction.x + direction.y * direction.y);
	
	if (totalLength == 0) return;
	
	direction.x /= totalLength;
	direction.y /= totalLength;
	
	float currentPos = 0;
	while (currentPos < totalLength) {
		float dashEnd = std::min(currentPos + dashLength, totalLength);
		
		ImVec2 dashStart = ImVec2(start.x + direction.x * currentPos, start.y + direction.y * currentPos);
		ImVec2 dashEndPos = ImVec2(start.x + direction.x * dashEnd, start.y + direction.y * dashEnd);
		
		drawList->AddLine(dashStart, dashEndPos, color, 2.0f);
		
		currentPos += dashLength + gapLength;
	}
}
