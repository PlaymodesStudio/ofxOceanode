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

void curve2::setup() {
    color = ofColor(255,128,0,255);
    description = "Used to create functions than transforms an input following the curve to an output";
    addParameter(curveName.set("Name",""));
    addParameter(input.set("Input", {0}, {0}, {1}));
	addParameter(showWindow.set("Show", true));
    addOutputParameter(output.set("Output", {0}, {0}, {1}));
    
	
	addInspectorParameter(numVerticalDivisions.set("Vert Div", 10, 1, 1000));
	addInspectorParameter(numHorizontalDivisions.set("Hor Div", 2, 1, 1000));
	
	addInspectorParameter(minX.set("Min X", 0, -FLT_MAX, FLT_MAX));
	addInspectorParameter(maxX.set("Max X", 1, -FLT_MAX, FLT_MAX));
	
	addInspectorParameter(minY.set("Min Y", 0, -FLT_MAX, FLT_MAX));
	addInspectorParameter(maxY.set("Max Y", 1, -FLT_MAX, FLT_MAX));
    addInspectorParameter(colorParam.set("Color", color));
    colorListener = colorParam.newListener([this](ofColor &c){
        color = c;
    });

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
			if(ImGui::Button("[Clear]"))
			{
				points.clear();
				points.emplace_back(0, 0);
				points.emplace_back(1, 1);
				
				points.front().firstCreated = false;
				points.back().firstCreated = false;
			}
			ImGui::PopStyleColor();
			ImGui::PopStyleVar();
			
			// Create our child canvas
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
			ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(50, 50, 50, 200));
			ImGui::BeginChild("scrolling_region", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse);
			ImGui::PushItemWidth(120.0f);
			
			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			
			// Display grid
			ImVec2 win_pos = ImGui::GetCursorScreenPos();
			ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
			if (true)
			{
				ImU32 GRID_COLOR = IM_COL32(80, 80, 90, 40);
				ImVec2 cell_sz = canvas_sz / ImVec2(numHorizontalDivisions, numVerticalDivisions);

				for (float x = cell_sz.x; x < canvas_sz.x; x += cell_sz.x)
					draw_list->AddLine(ImVec2(x, 0.0f) + win_pos, ImVec2(x, canvas_sz.y) + win_pos, GRID_COLOR);
				for (float y = cell_sz.y; y < canvas_sz.y; y += cell_sz.y)
					draw_list->AddLine(ImVec2(0.0f, y) + win_pos, ImVec2(canvas_sz.x, y) + win_pos, GRID_COLOR);
			}
			ImGui::PopItemWidth();
			ImGui::EndChild();
			
			auto normalizePoint = [win_pos, canvas_sz](glm::vec2 p) -> glm::vec2{
				return glm::vec2(ofMap(p.x, win_pos.x, win_pos.x+canvas_sz.x, 0, 1),
								 ofMap(p.y, win_pos.y, win_pos.y+canvas_sz.y, 1, 0));
			};
			
			auto denormalizePoint = [win_pos, canvas_sz](glm::vec2 p) -> glm::vec2{
				return glm::vec2(ofMap(p.x, 0, 1, win_pos.x, win_pos.x+canvas_sz.x),
								 ofMap(p.y, 1, 0, win_pos.y, win_pos.y+canvas_sz.y));
			};
			
			if(ImGui::IsMouseReleased(0)){
				for(int i = 0; i < points.size(); i++){
					points[i].drag = 0;
					points[i].firstCreated = false;
				}
			}
			
			// Point hover detection system
			hoveredPointIndex = -1;
			if(ImGui::IsItemHovered(ImGuiHoveredFlags_None)){
				glm::vec2 mousePos = glm::vec2(ImGui::GetMousePos());
				for(int i = 0; i < points.size(); i++){
					glm::vec2 pointPos = denormalizePoint(points[i].point);
					float mouseToPointDistance = glm::distance(mousePos, pointPos);
					if(mouseToPointDistance < 15){ // Increased hover detection radius
						hoveredPointIndex = i;
						break;
					}
				}
			}
			
			if(ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemHovered(ImGuiHoveredFlags_None)){
				createPoint = false;
				points.emplace_back(normalizePoint(ImGui::GetMousePos()));
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
						glm::vec2 normalizedPos = glm::clamp(normalizePoint(ImGui::GetMousePos()), 0.0f, 1.0f);
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
				if(ImGui::IsItemClicked(0) && !someItemClicked){
					auto mouseToPointDistance = glm::distance(glm::vec2(ImGui::GetMousePos()), pointPos);
					if(mouseToPointDistance < 15){ // Increased selection radius
						p.drag = 3;
						someItemClicked = true;
					}
				}else if(ImGui::IsItemClicked(1)){
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
						
						// Show tension exponent slider for TENSION segments
						if(lines[i-1].type == LINE2_TENSION){
							if(ImGui::SliderFloat("Tension L", &lines[i-1].tensionExponent, 0.01f, 10.0f, "%.2f")){
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
						
						// Show tension exponent slider for TENSION segments
						if(lines[i].type == LINE2_TENSION){
							if(ImGui::SliderFloat("Tension R", &lines[i].tensionExponent, 0.01f, 10.0f, "%.2f")){
								recalculate();
							}
						}
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
			
			// Shift+drag interaction for tension exponent adjustment with accelerated scaling
			if(ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_LeftShift)) && !someItemClicked){
				if(ImGui::IsMouseDragging(0)){
					if(!tensionDragActive){
						// Start new tension drag operation
						tensionDragActive = true;
						tensionDragStartPos = normalizePoint(ImGui::GetMousePos());
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
						glm::vec2 currentMousePos = normalizePoint(ImGui::GetMousePos());
						
						// Calculate total drag distance from start position
						float deltaY = currentMousePos.y - tensionDragStartPos.y;
						float dragDistance = abs(deltaY * 600.0f); // Convert to pixel distance (assuming 600px canvas height)
						
						// Accelerated scaling algorithm
						const float baseRate = 0.005f;
						const float threshold = 30.0f; // Distance where scaling begins to accelerate
						const float scaleFactor = 1.3f; // Acceleration factor
						
						float adjustment;
						if(dragDistance <= threshold){
							// Linear scaling for small distances (fine control)
							adjustment = baseRate * dragDistance;
						} else {
							// Accelerated scaling for larger distances (coarse control)
							adjustment = baseRate * pow(dragDistance / threshold, scaleFactor) * threshold;
						}
						
						// Apply inverted direction: up = decrease exponent, down = increase exponent
						if(deltaY < 0){
							// Dragging up - decrease exponent (more logarithmic curve)
							lines[tensionDragSegmentIndex].tensionExponent = std::max(0.01f, tensionDragStartExponent - adjustment);
						} else {
							// Dragging down - increase exponent (more exponential curve)
							lines[tensionDragSegmentIndex].tensionExponent = std::min(10.0f, tensionDragStartExponent + adjustment);
						}
						
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
			
			if(ImGui::IsItemClicked(0) && !someItemClicked){
				createPoint = true;
			}
			
			draw_list->AddLine(denormalizePoint(glm::vec2(0, points[0].point.y)), denormalizePoint(points[0].point), IM_COL32(10, 10, 10, 255));
			for(int i = 0; i < points.size()-1; i++){
				if(lines[i].type == LINE2_HOLD){
					auto p1 = denormalizePoint(points[i].point);
					auto p2 = denormalizePoint(points[i+1].point);
					draw_list->AddLine(p1, glm::vec2(p2.x, p1.y), IM_COL32(color.r, color.g, color.b, 200), 2);
					draw_list->AddLine(glm::vec2(p2.x, p1.y), p2, IM_COL32(color.r, color.g, color.b, 200), 2);
				}
				else if(lines[i].type == LINE2_TENSION){
					// Draw tension curve using the same calculation as in recalculate()
					const int numSegments = 50; // Number of line segments to approximate the curve
					glm::vec2 prevPoint = points[i].point;
					
					for(int seg = 1; seg <= numSegments; seg++){
						float normalizedX = (float)seg / numSegments;
						
						// Calculate normalized X position within the segment
						float segmentX = glm::mix(points[i].point.x, points[i+1].point.x, normalizedX);
						
						// Normalize Y values to [0,1] range for the segment
						float y1 = points[i].point.y;
						float y2 = points[i+1].point.y;
						float minSegmentY = std::min(y1, y2);
						float maxSegmentY = std::max(y1, y2);
						float segmentRange = maxSegmentY - minSegmentY;
						
						float resultY;
						if(segmentRange > 0.0001f) { // Avoid division by zero
							// Normalize Y1 and Y2 to [0,1] within segment range
							float normalizedY1 = (y1 - minSegmentY) / segmentRange;
							float normalizedY2 = (y2 - minSegmentY) / segmentRange;
							
							// Apply tension exponent to the normalized Y interpolation
							float normalizedYResult = glm::mix(normalizedY1, normalizedY2, pow(normalizedX, lines[i].tensionExponent));
							
							// Map back to original Y range
							resultY = minSegmentY + normalizedYResult * segmentRange;
						} else {
							// If segment has no Y range, just use linear interpolation
							resultY = glm::mix(y1, y2, normalizedX);
						}
						
						glm::vec2 currentPoint = glm::vec2(segmentX, resultY);
						
						// Draw line segment from previous point to current point
						auto p1 = denormalizePoint(prevPoint);
						auto p2 = denormalizePoint(currentPoint);
						draw_list->AddLine(p1, p2, IM_COL32(color.r, color.g, color.b, 200), 2);
						
						prevPoint = currentPoint;
					}
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
                        
                        // Normalize Y values to [0,1] range for the segment
                        float y1 = points[j-1].point.y;
                        float y2 = points[j].point.y;
                        float minSegmentY = std::min(y1, y2);
                        float maxSegmentY = std::max(y1, y2);
                        float segmentRange = maxSegmentY - minSegmentY;
                        
                        float resultY;
                        if(segmentRange > 0.0001f) { // Avoid division by zero
                            // Normalize Y1 and Y2 to [0,1] within segment range
                            float normalizedY1 = (y1 - minSegmentY) / segmentRange;
                            float normalizedY2 = (y2 - minSegmentY) / segmentRange;
                            
                            // Apply tension exponent to the normalized Y interpolation
                            float normalizedYResult = glm::mix(normalizedY1, normalizedY2, pow(normalizedX, lines[j-1].tensionExponent));
                            
                            // Map back to original Y range
                            resultY = minSegmentY + normalizedYResult * segmentRange;
                        } else {
                            // If segment has no Y range, just use linear interpolation
                            resultY = glm::mix(y1, y2, normalizedX);
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
	}
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
		}
	}
	catch (ofJson::exception& e)
	{
		ofLog() << e.what();
	}
	
}
