//
//  curve.cpp
//  ofxOceanode
//
//  Created by Eduard Frigola on 25/05/21.
//
//

#include "curve2.h"
#include "imgui.h"
#include "glm/gtx/closest_point.hpp"

// Helper function to draw dotted lines
void drawDottedLine(ImDrawList* drawList, ImVec2 start, ImVec2 end, ImU32 color, float dashLength = 5.0f, float gapLength = 10.0f) {
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

void curve2::setup() {
    color = ofColor(255,128,0,255);
    description = "Advanced curve editor with asymmetric logistic segments. \n| Double-click: Create points \n| Drag: Move points \n| Right-click: Delete \n| Shift+drag: Asymmetry (vertical) + Inflection (horizontal) \n| Ctrl+drag: B parameter (horizontal) \n| Visual feedback: Yellow=Inflection, Cyan=Asymmetry, Red=B parameter \n| Snap to Grid: Enable for precise grid-aligned positioning";
    addParameter(curveName.set("Name",""));
    addParameter(input.set("Input", {0}, {0}, {1}));
	addParameter(showWindow.set("Show", true));
    addOutputParameter(output.set("Output", {0}, {0}, {1}));
    
	
	addInspectorParameter(numHorizontalDivisions.set("Hor Div", 8, 1, 512));
	addInspectorParameter(numVerticalDivisions.set("Vert Div", 4, 1, 512));
	
	addInspectorParameter(minX.set("Min X", 0, -FLT_MAX, FLT_MAX));
	addInspectorParameter(maxX.set("Max X", 1, -FLT_MAX, FLT_MAX));
	
	addInspectorParameter(minY.set("Min Y", 0, -FLT_MAX, FLT_MAX));
	addInspectorParameter(maxY.set("Max Y", 1, -FLT_MAX, FLT_MAX));
    addInspectorParameter(colorParam.set("Color", color));
    colorListener = colorParam.newListener([this](ofColor &c){
        color = c;
    });
    
    // Initialize global logistic parameters
    globalQ.set("Global Q (Scaling)", 1.0f, 0.1f, 5.0f);
    
    // Initialize snap to grid parameter
    addInspectorParameter(snapToGrid.set("Snap to Grid", false));
    
    // Initialize show info parameter
    addInspectorParameter(showInfo.set("Show Info", false));

    listeners.push(input.newListener([&](vector<float> &vf){
        recalculate();
    }));
	
	listeners.push(minX.newListener([this](float &f){
		input.setMin(vector<float>(1, f));
	}));
	listeners.push(maxX.newListener([this](float &f){
		input.setMax(vector<float>(1, f));
	}));
	listeners.push(minY.newListener([this](float &f){
		output.setMin(vector<float>(1, f));
	}));
	listeners.push(maxY.newListener([this](float &f){
		output.setMax(vector<float>(1, f));
	}));
	
	points.emplace_back(0, 0);
	points.emplace_back(1, 1);
	
	points.front().firstCreated = false;
	points.back().firstCreated = false;
	
	lines.emplace_back();
}

void curve2::draw(ofEventArgs &args){
	
	if(showWindow){
		string modCanvasID = canvasID == "Canvas" ? "" : (canvasID + "/");
		if(ImGui::Begin((ofToString(curveName) + "_" + modCanvasID + "Curve2 " + ofToString(getNumIdentifier())).c_str(), (bool *)&showWindow.get()))
        {
			ImGui::SameLine();
			ImGui::BeginGroup();
			
			const ImVec2 NODE_WINDOW_PADDING(8.0f, 7.0f);
			
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0,1.0,1.0,1.0));
            ImGui::Text(ofToString(curveName).c_str());
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55,0.55,0.55,1.0));
			if(ImGui::Button("[Reset]"))
			{
				// Clear existing points and lines
				points.clear();
				lines.clear();
				
				// Create default curve with 2 points: (0,0) and (1,1)
				points.emplace_back(0, 0);
				points.emplace_back(1, 1);
				
				// Set points as not newly created
				points.front().firstCreated = false;
				points.back().firstCreated = false;
				
				// Create default line segment with default values
				lines.emplace_back();
				
				// Trigger recalculation of the curve
				recalculate();
			}
			ImGui::PopStyleColor();
			ImGui::PopStyleVar();
			
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
			if (true)
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
			
			// Draw visual area boundary to show the safe zone effect
			ImU32 BOUNDARY_COLOR = IM_COL32(10, 10, 10, 128);
			draw_list->AddRect(visual_win_pos,
							   ImVec2(visual_win_pos.x + visual_canvas_sz.x, visual_win_pos.y + visual_canvas_sz.y),
							   BOUNDARY_COLOR, 0.0f, 0, 2.0f);
			
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
				for(int i = 0; i < points.size(); i++){
					points[i].drag = 0;
					points[i].firstCreated = false;
				}
			}
			
			// Point hover detection system
			hoveredPointIndex = -1;
			hoveredSegmentIndex = -1;
			if(canvasHovered){
				glm::vec2 mousePos = glm::vec2(ImGui::GetMousePos());
				glm::vec2 normalizedMousePos = safeNormalizePoint(mousePos);
				
				// First check for point hover (higher priority)
				for(int i = 0; i < points.size(); i++){
					glm::vec2 pointPos = denormalizePoint(points[i].point);
					float mouseToPointDistance = glm::distance(mousePos, pointPos);
					if(mouseToPointDistance < 15){ // Increased hover detection radius
						hoveredPointIndex = i;
						break;
					}
				}
				
				// If no point is hovered, check for segment hover
				if(hoveredPointIndex == -1){
					for(int i = 0; i < points.size()-1; i++){
						if(normalizedMousePos.x >= points[i].point.x &&
						   normalizedMousePos.x <= points[i+1].point.x &&
						   lines[i].type == LINE2_TENSION){
							hoveredSegmentIndex = i;
							break;
						}
					}
				}
			}
			
			// Double-click to add point
			if(canvasDoubleClicked){
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
				
				points.emplace_back(newPointPos);
				points.back().drag = 3;
				lines.emplace_back();
			}
			
			std::sort(points.begin(), points.end(), [](curvePoint2 &p1, curvePoint2 &p2){
				return p1.point.x < p2.point.x;
			});
			
			bool someItemClicked = false;
			int indexToRemove = -1;
			for(int i = 0; i < points.size(); i++){
				auto &p = points[i];
				
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
					float tempFloat = ofMap(p.point.x, 0, 1, minX, maxX);
				                ImGui::SliderFloat("X", &tempFloat, minX, maxX);
				                if(ImGui::IsItemDeactivated() || (ImGui::IsMouseDown(0) && ImGui::IsItemEdited())){
				                    p.point.x = ofMap(tempFloat, minX, maxX, 0, 1);
				                }
				                
					tempFloat = ofMap(p.point.y, 0, 1, minY, maxY);
				                ImGui::SliderFloat("Y", &tempFloat, minY, maxY);
				                if(ImGui::IsItemDeactivated() || (ImGui::IsMouseDown(0) && ImGui::IsItemEdited())){
						p.point.y = ofMap(tempFloat, minY, maxY, 0, 1);
					}
					
					ImGui::Spacing();
					
					if(i > 0){
						int elem = lines[i-1].type;
						const char* elems_names[2] = { "Hold", "Tension"};
						if(ImGui::SliderInt("Line L", &elem, 0, 1, elems_names[elem])){
							lines[i-1].type = static_cast<lineType2>(elem);
						}
						
						// Show asymmetric logistic controls for TENSION segments
						if(lines[i-1].type == LINE2_TENSION){
							if(ImGui::SliderFloat("Asymmetry L", &lines[i-1].tensionExponent, MIN_ASYMMETRY, MAX_ASYMMETRY, "%.3f")){
								recalculate();
							}
							if(ImGui::SliderFloat("Inflection L", &lines[i-1].inflectionX, 0.0f, 1.0f, "%.3f")){
								recalculate();
							}
							if(ImGui::SliderFloat("Segment B L", &lines[i-1].segmentB, MIN_B_PARAMETER, MAX_B_PARAMETER, "%.2f")){
								recalculate();
							}
						}
					}
					
					if(i < lines.size()){
						int elem = lines[i].type;
						const char* elems_names[2] = { "Hold", "Tension"};
						if(ImGui::SliderInt("Line R", &elem, 0, 1, elems_names[elem])){
							lines[i].type = static_cast<lineType2>(elem);
						}
						
						// Show asymmetric logistic controls for TENSION segments
						if(lines[i].type == LINE2_TENSION){
							if(ImGui::SliderFloat("Asymmetry R", &lines[i].tensionExponent, MIN_ASYMMETRY, MAX_ASYMMETRY, "%.3f")){
								recalculate();
							}
							if(ImGui::SliderFloat("Inflection R", &lines[i].inflectionX, 0.0f, 1.0f, "%.3f")){
								recalculate();
							}
							if(ImGui::SliderFloat("Segment B R", &lines[i].segmentB, MIN_B_PARAMETER, MAX_B_PARAMETER, "%.2f")){
								recalculate();
							}
						}
					}
					
					ImGui::Separator();
					ImGui::Text("Global Logistic Parameters:");
					float tempQ = globalQ.get();
					
					if(ImGui::SliderFloat("Q (Scaling)", &tempQ, 0.1f, 5.0f, "%.1f")) {
						globalQ = tempQ;
						recalculate();
					}
					
					ImGui::Spacing();
					
					if(ImGui::Selectable("Remove")){
						ImGui::CloseCurrentPopup();
						indexToRemove = i;
					}
					ImGui::EndPopup();
				}
			}
			
			if(indexToRemove != -1){
				points.erase(points.begin()+indexToRemove);
				if(indexToRemove < lines.size()){
					lines.erase(lines.begin()+indexToRemove);
				}
			}
			
			// Shift+drag interaction for asymmetric logistic curve adjustment (tension/asymmetry control)
			if(ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_LeftShift)) && !someItemClicked && canvasHovered){
				if(ImGui::IsMouseDragging(0)){
					if(!tensionDragActive){
						// Start new tension drag operation
						tensionDragActive = true;
						tensionDragStartPos = safeNormalizePoint(ImGui::GetMousePos());
						tensionDragSegmentIndex = -1;
						
						// Find which segment the mouse is over
						for(int i = 0; i < points.size()-1; i++){
							if(tensionDragStartPos.x >= points[i].point.x && tensionDragStartPos.x <= points[i+1].point.x && lines[i].type == LINE2_TENSION){
								tensionDragSegmentIndex = i;
								tensionDragStartExponent = lines[i].tensionExponent;
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
						float segmentStartX = points[tensionDragSegmentIndex].point.x;
						float segmentEndX = points[tensionDragSegmentIndex + 1].point.x;
						float segmentWidth = segmentEndX - segmentStartX;
						
						if(segmentWidth > 0.0001f){
							// Map current mouse X to xsegment-relative position
							float relativeX = (currentMousePos.x - segmentStartX) / segmentWidth;
							// Constrain to avoid edge issues
							lines[tensionDragSegmentIndex].inflectionX = std::max(MIN_INFLECTION_X, std::min(MAX_INFLECTION_X, relativeX));
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
						
						lines[tensionDragSegmentIndex].tensionExponent = asymmetry;
						
						// Clamp to valid range
						lines[tensionDragSegmentIndex].tensionExponent = std::max(minAsymmetry, std::min(maxAsymmetry, lines[tensionDragSegmentIndex].tensionExponent));
						
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
			
			if(ctrlPressed && !someItemClicked && canvasHovered){
				if(ImGui::IsMouseDragging(0)){
					if(!bDragActive){
						// Start new B parameter drag operation
						bDragActive = true;
						bDragStartPos = safeNormalizePoint(ImGui::GetMousePos());
						bDragSegmentIndex = -1;
						
						// Find which segment the mouse is over
						for(int i = 0; i < points.size()-1; i++){
							if(bDragStartPos.x >= points[i].point.x && bDragStartPos.x <= points[i+1].point.x && lines[i].type == LINE2_TENSION){
								bDragSegmentIndex = i;
								bDragStartValue = lines[i].segmentB;
								break;
							}
						}
					}
					
					// Continue existing B parameter drag operation
					if(bDragActive && bDragSegmentIndex >= 0){
						glm::vec2 currentMousePos = safeNormalizePoint(ImGui::GetMousePos());
						
						// X-position-based B parameter mapping within segment
						float segmentStartX = points[bDragSegmentIndex].point.x;
						float segmentEndX = points[bDragSegmentIndex + 1].point.x;
						float segmentWidth = segmentEndX - segmentStartX;
						
						if(segmentWidth > 0.0001f){
							// Map current mouse X position within segment to B parameter range
							float relativeX = (currentMousePos.x - segmentStartX) / segmentWidth;
							// Clamp to segment bounds
							relativeX = std::max(0.0f, std::min(1.0f, relativeX));
							
							// Linear interpolation: B = maxB - relativeX * (maxB - minB)
							const float minB = MIN_B_PARAMETER;
							const float maxB = MAX_B_PARAMETER;
							lines[bDragSegmentIndex].segmentB = maxB - relativeX * (maxB - minB);
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
			
			// Single-click point creation removed - no longer supported
			// Point creation now only available via double-click with Shift or Control modifier
			
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
				
				// Check for active point dragging
				for(int i = 0; i < points.size(); i++){
					if(points[i].drag == 3){
						showPointInfo = true;
						activePointIndex = i;
						break;
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
				
				// Display point information
				if(showPointInfo && activePointIndex >= 0 && activePointIndex < points.size()){
					ImGui::Text("Point %d:", activePointIndex);
					ImGui::SameLine();
					
					// Point coordinates
					float pointX = ofMap(points[activePointIndex].point.x, 0, 1, minX, maxX);
					float pointY = ofMap(points[activePointIndex].point.y, 0, 1, minY, maxY);
					
					ImGui::PushItemWidth(80);
					ImGui::Text("X: %.3f", pointX);
					ImGui::SameLine();
					ImGui::Text("Y: %.3f", pointY);
					ImGui::PopItemWidth();
				}
				
				// Display segment information
				if(showSegmentInfo && activeSegmentIndex >= 0 && activeSegmentIndex < lines.size()){
					if(showPointInfo) ImGui::Spacing();
					
					ImGui::Text("Segment %d:", activeSegmentIndex);
					ImGui::SameLine();
					
					// Line type
					const char* lineTypeName = (lines[activeSegmentIndex].type == LINE2_HOLD) ? "HOLD" : "TENSION";
					ImGui::Text("Type: %s", lineTypeName);
					
					// Show tension parameters only for TENSION segments
					if(lines[activeSegmentIndex].type == LINE2_TENSION){
						ImGui::Spacing();
						
						// First row: Asymmetry and Inflection
						ImGui::PushItemWidth(80);
						ImGui::Text("Asymmetry: %.3f", lines[activeSegmentIndex].tensionExponent);
						ImGui::SameLine();
						ImGui::Text("Inflection: %.3f", lines[activeSegmentIndex].inflectionX);
						ImGui::PopItemWidth();
						
						// Second row: Segment B and Global Q
						ImGui::PushItemWidth(80);
						ImGui::Text("Segment B: %.2f", lines[activeSegmentIndex].segmentB);
						ImGui::SameLine();
						ImGui::Text("Global Q: %.1f", globalQ.get());
						ImGui::PopItemWidth();
					}
				}
				
				// Show global parameters when no specific element is active
				if(!showPointInfo && !showSegmentInfo){
					ImGui::Text("Global Parameters:");
					ImGui::SameLine();
					ImGui::PushItemWidth(80);
					ImGui::Text("Q: %.1f", globalQ.get());
					ImGui::PopItemWidth();
					
					ImGui::Spacing();
					ImGui::Text("Hover over points or drag to see details");
				}
			}
			
			// Re-push style vars for drawing operations
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
			ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(50, 50, 50, 200));
			
			draw_list->AddLine(denormalizePoint(glm::vec2(0, points[0].point.y)), denormalizePoint(points[0].point), IM_COL32(10, 10, 10, 255));
			for(int i = 0; i < points.size()-1; i++){
				// Determine segment color based on drag state
				bool whiteHighlight = false;
				bool orangeHighlight = false;
				
				// Check for tension drag highlighting (shift+drag) - use white
				if(tensionDragActive && tensionDragSegmentIndex == i){
					whiteHighlight = true;
				}
				
				// Check for B parameter drag highlighting (ctrl+drag) - use white
				if(bDragActive && bDragSegmentIndex == i){
					whiteHighlight = true;
				}
				
				// Check for normal point drag highlighting - use orange
				if(!whiteHighlight){
					for(int p = 0; p < points.size(); p++){
						if(points[p].drag == 3){
							// Highlight segments connected to the dragged point in orange
							if((p == i) || (p == i + 1)){
								orangeHighlight = true;
								break;
							}
						}
					}
				}
				
				// Choose color based on highlighting state
				ImU32 segmentColor;
				if(whiteHighlight){
					segmentColor = IM_COL32(255, 255, 255, 255); // White for shift/ctrl drag
				} else if(orangeHighlight){
					segmentColor = IM_COL32(color.r, color.g, color.b, 255); // Orange for normal drag
				} else {
					segmentColor = IM_COL32(color.r, color.g, color.b, 200); // Default orange with transparency
				}
				
				if(lines[i].type == LINE2_HOLD){
					auto p1 = denormalizePoint(points[i].point);
					auto p2 = denormalizePoint(points[i+1].point);
					draw_list->AddLine(p1, glm::vec2(p2.x, p1.y), segmentColor, 2);
					draw_list->AddLine(glm::vec2(p2.x, p1.y), p2, segmentColor, 2);
				}
				else if(lines[i].type == LINE2_TENSION){
					// Draw asymmetric logistic curve with proper normalization
					// Adaptive resolution based on B parameter - more segments for steeper curves
					// This prevents polygonal appearance when B values are high (60-100)
					float segmentB = lines[i].segmentB;
					int baseSegments = 50;
					int adaptiveSegments = std::max(baseSegments, (int)(segmentB * 2.0f)); // 2 segments per B unit
					// Cap maximum segments to prevent performance issues
					const int maxSegments = 500;
					const int numSegments = std::min(adaptiveSegments, maxSegments);
					
					glm::vec2 prevPoint = points[i].point;
					
					// Asymmetric logistic parameters
					float A = points[i].point.y;      // Y value of segment start point
					float K = points[i+1].point.y;    // Y value of segment end point
					float M = lines[i].inflectionX;   // Inflection point X position (0-1)
					float nu = lines[i].tensionExponent; // Asymmetry parameter ν
					float B = lines[i].segmentB;      // Per-segment B parameter
					float Q = globalQ.get();          // Global Q parameter
					
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
						float segmentX = glm::mix(points[i].point.x, points[i+1].point.x, normalizedX);
						
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
						draw_list->AddLine(p1, p2, segmentColor, 2);
						
						prevPoint = currentPoint;
					}
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
			
			if(showInflectionLine && inflectionSegmentIndex >= 0){
				
				// Ensure segment index is valid
				if(inflectionSegmentIndex < lines.size()){
					// Get the current segment being modified
					float segmentStartX = points[inflectionSegmentIndex].point.x;
					float segmentEndX = points[inflectionSegmentIndex + 1].point.x;
					
					// Calculate the actual X position of the inflection point
					float inflectionX = lines[inflectionSegmentIndex].inflectionX;
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
			
			if(showAsymmetryLine && asymmetrySegmentIndex >= 0){
				// Ensure segment index is valid
				if(asymmetrySegmentIndex < lines.size()){
					// Get current asymmetry value
					float currentAsymmetry = lines[asymmetrySegmentIndex].tensionExponent;
					
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
			
			if(showBParameterLine && bParameterSegmentIndex >= 0){
				// Ensure segment index is valid
				if(bParameterSegmentIndex < lines.size()){
					// Get the current segment being modified
					float segmentStartX = points[bParameterSegmentIndex].point.x;
					float segmentEndX = points[bParameterSegmentIndex + 1].point.x;
					float segmentWidth = segmentEndX - segmentStartX;
					
					// Get current B parameter value
					float currentB = lines[bParameterSegmentIndex].segmentB;
					
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
			
			for(int i = 0; i < points.size(); i++){
				auto &p = points[i];
				glm::vec2 pointPos = denormalizePoint(p.point);
				
				if(i == hoveredPointIndex){
					// Draw highlighted point with larger radius and different color
					draw_list->AddCircleFilled(pointPos, 8, IM_COL32(255, 255, 255, 255)); // White highlight
					draw_list->AddCircle(pointPos, 8, IM_COL32(color.r, color.g, color.b, 255), 0, 2); // Colored outline
					draw_list->AddCircleFilled(pointPos, 5, IM_COL32(0, 0, 0, 255)); // Original black center
				} else {
					// Draw normal point
					draw_list->AddCircleFilled(pointPos, 5, IM_COL32(0, 0, 0, 255));
				}
			}
			
			draw_list->AddLine(denormalizePoint(points.back().point), denormalizePoint(glm::vec2(1, points.back().point.y)), IM_COL32(10, 10, 10, 255));
			
			for(auto &v : input.get()){
				auto mv = ofMap(v, minX, maxX, 0, 1);
				draw_list->AddLine(denormalizePoint(glm::vec2(mv, 0)), denormalizePoint(glm::vec2(mv, 1)), IM_COL32(127, 127, 127, 127));
			}
			
			for(auto &p : debugPoints){
				draw_list->AddCircleFilled(denormalizePoint(p), 3, IM_COL32(255, 0, 0, 255));
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
	debugPoints.resize(input->size());
	vector<float> tempOut(input->size());
	for(int i = 0; i < input->size(); i++){
		auto p = ofMap(input->at(i), minX, maxX, 0, 1);
		if(p <= points[0].point.x){
			tempOut[i] = ofMap(points[0].point.y, 0, 1, minY, maxY);
			debugPoints[i] = glm::vec2(p, points[0].point.y);
		}else if(p >= points.back().point.x){
			tempOut[i] = ofMap(points.back().point.y, 0, 1, minY, maxY);
			debugPoints[i] = glm::vec2(p, points.back().point.y);
		}else{
			for(int j = 1; j < points.size(); j++){
				if(p < points[j].point.x){
					if(lines[j-1].type == LINE2_HOLD){
						debugPoints[i] = glm::vec2(p, points[j-1].point.y);
						tempOut[i] = ofMap(debugPoints[i].y, 0, 1, minY, maxY);
					}
                    else if(lines[j-1].type == LINE2_TENSION){
                        // Calculate normalized X position within the segment
                        float normalizedX = ofMap(p, points[j-1].point.x, points[j].point.x, 0, 1);
                        
                        // Asymmetric logistic parameters
                        float A = points[j-1].point.y;      // Y value of segment start point
                        float K = points[j].point.y;        // Y value of segment end point
                        float M = lines[j-1].inflectionX;   // Inflection point X position (0-1)
                        float nu = lines[j-1].tensionExponent; // Asymmetry parameter ν
                        float B = lines[j-1].segmentB;      // Per-segment B parameter
                        float Q = globalQ.get();            // Global Q parameter
                        
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
                        
                        debugPoints[i] = glm::vec2(p, resultY);
                        tempOut[i] = ofMap(debugPoints[i].y, 0, 1, minY, maxY);
                    }
					break;
				}
			}
		}
	}
	output = tempOut;
}

void curve2::presetSave(ofJson &json){
	json["NumPoints"] = points.size();
	for(int i = 0; i < points.size(); i++){
		json["Points"][i]["Point"]["x"] = points[i].point.x;
		json["Points"][i]["Point"]["y"] = points[i].point.y;
	}
	for(int i = 0; i < lines.size(); i++){
		json["Lines"][i]["Type"] = lines[i].type;
		json["Lines"][i]["TensionExponent"] = lines[i].tensionExponent;
		json["Lines"][i]["InflectionX"] = lines[i].inflectionX;
		json["Lines"][i]["SegmentB"] = lines[i].segmentB;
	}
	// Save global logistic parameters
	json["GlobalQ"] = globalQ.get();
}

void curve2::presetRecallAfterSettingParameters(ofJson &json){
	try{
		points.resize(json["NumPoints"]);
		lines.resize(points.size()-1);
		for(int i = 0; i < points.size(); i++){
			points[i].point.x = json["Points"][i]["Point"]["x"];
			points[i].point.y = json["Points"][i]["Point"]["y"];
			points[i].firstCreated = false;
		}
		for(int i = 0; i < lines.size(); i++){
			lines[i].type = static_cast<lineType2>(json["Lines"][i]["Type"]);
			// Load tension exponent with backward compatibility
			if(json["Lines"][i].contains("TensionExponent")){
				lines[i].tensionExponent = json["Lines"][i]["TensionExponent"];
			} else {
				lines[i].tensionExponent = 1.0f; // Default value for old presets
			}
			// Load inflection point with backward compatibility
			if(json["Lines"][i].contains("InflectionX")){
				lines[i].inflectionX = json["Lines"][i]["InflectionX"];
			} else {
				lines[i].inflectionX = 0.5f; // Default value for old presets
			}
			// Load per-segment B with backward compatibility
			if(json["Lines"][i].contains("SegmentB")){
				lines[i].segmentB = json["Lines"][i]["SegmentB"];
			} else if(json["Lines"][i].contains("SegmentQ")){
				// Backward compatibility: if old SegmentQ exists, use default B value
				lines[i].segmentB = 6.0f; // Use default B value for old presets
			} else {
				lines[i].segmentB = 6.0f; // Default value for old presets
			}
		}
		
		// Load global logistic parameters with backward compatibility
		if(json.contains("GlobalQ")){
			globalQ.set(json["GlobalQ"]);
		} else {
			globalQ.set(1.0f); // Default value for old presets
		}
	}
	catch (ofJson::exception& e)
	{
		ofLog() << e.what();
	}
	
}

