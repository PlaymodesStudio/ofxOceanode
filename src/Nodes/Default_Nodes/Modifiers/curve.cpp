//
//  curve.cpp
//  ofxOceanode
//
//  Created by Eduard Frigola on 25/05/21.
//
//

#include "curve.h"
#include "imgui.h"
#include "glm/gtx/closest_point.hpp"

void curve::setup() {
    color = ofColor::white;
    addParameter(input.set("Input", {0}, {0}, {1}));
	addParameter(showWindow.set("Show", true));
    addOutputParameter(output.set("Output", {0}, {0}, {1}));
	
	addInspectorParameter(numVerticalDivisions.set("Vert Div", 10, 1, 1000));
	addInspectorParameter(numHorizontalDivisions.set("Hor Div", 2, 1, 1000));

    listeners.push(input.newListener([&](vector<float> &vf){
        recalculate();
    }));
	
	points.emplace_back(0, 0);
	points.emplace_back(1, 1);
	
	points.front().cp2 = glm::vec2(0.25, 0.25);
	points.back().cp1 = glm::vec2(0.75, 0.75);
	
	points.front().firstCreated = false;
	points.back().firstCreated = false;
}

void curve::draw(ofEventArgs &args){
	
	if(showWindow){
		string modCanvasID = canvasID == "Canvas" ? "" : (canvasID + "/");
		if(ImGui::Begin((modCanvasID + "Curve " + ofToString(getNumIdentifier())).c_str(), (bool *)&showWindow.get())){
			ImGui::SameLine();
			ImGui::BeginGroup();
			
			const ImVec2 NODE_WINDOW_PADDING(8.0f, 7.0f);
			
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55,0.55,0.55,1.0));
			if(ImGui::Button("[Clear]"))
			{
				points.clear();
				points.emplace_back(0, 0);
				points.emplace_back(1, 1);
				
				points.front().cp2 = glm::vec2(0.25, 0.25);
				points.back().cp1 = glm::vec2(0.75, 0.75);
				
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
			ImGui::PopStyleColor();
			ImGui::PopStyleVar(2);
			ImGui::EndGroup();
			
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
			
			if(ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemHovered(ImGuiHoveredFlags_None)){
				createPoint = false;
				points.emplace_back(normalizePoint(ImGui::GetMousePos()));
				points.back().drag = 3;
			}
			
			std::sort(points.begin(), points.end(), [](curvePoint &p1, curvePoint &p2){
				return p1.point.x < p2.point.x;
			});
			
			bool someItemClicked = false;
			int indexToRemove = -1;
			for(int i = 0; i < points.size(); i++){
				auto &p = points[i];
				if(p.firstCreated){
					//Edit control points on creation
					auto tangent = glm::normalize(points[i+1].point - points[i-1].point);
					p.cp1 = p.point - (tangent * glm::distance(p.point, points[i-1].point)/4);
					p.cp2 = p.point + (tangent * glm::distance(p.point, points[i+1].point)/4);
					
					if(i == 1){
						points[i-1].cp2 = points[i-1].point + (tangent * glm::distance(p.point, points[i-1].point)/4);
						auto ccp2 = glm::closestPointOnLine(points[i-1].cp2, points[i-1].point, p.point);
						points[i-1].cp2 = ccp2 + (ccp2 - points[i-1].cp2);
						points[i-1].cp2 = glm::clamp(points[i-1].cp2, glm::vec2(points[i-1].point.x, 0), glm::vec2(1, 1));
					}
					
					if(i == points.size() - 2){
						points[i+1].cp1 = points[i+1].point - (tangent * glm::distance(p.point, points[i+1].point)/4);
						auto ccp1 = glm::closestPointOnLine(points[i+1].cp1, points[i+1].point, p.point);
						points[i+1].cp1 = ccp1 + (ccp1 - points[i+1].cp1);
						points[i+1].cp1 = glm::clamp(points[i+1].cp1, glm::vec2(0, 0), glm::vec2(points[i+1].point.x, 1));
					}
//
					
				}
				
				if(ImGui::IsMouseDragging(0) && p.drag != 0){
					if(p.drag == 1){
						p.cp1 = glm::clamp(normalizePoint(ImGui::GetMousePos()), glm::vec2(0, 0), glm::vec2(p.point.x, 1));
						if(ImGui::GetIO().KeyAlt){
							p.cp2 = p.point - (p.cp1 - p.point);
						}
					}
					if(p.drag == 2){
						p.cp2 = glm::clamp(normalizePoint(ImGui::GetMousePos()), glm::vec2(p.point.x, 0), glm::vec2(1, 1));
						if(ImGui::GetIO().KeyAlt){
							p.cp1 = p.point - (p.cp2 - p.point);
						}
					}
					if(p.drag == 3){
						glm::vec2 normalizedPos = glm::clamp(normalizePoint(ImGui::GetMousePos()), 0.0f, 1.0f);
						if(ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_X))){
							normalizedPos.x = round(normalizedPos.x * numHorizontalDivisions) / numHorizontalDivisions;
						}
						if(ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Y))){
							normalizedPos.y = round(normalizedPos.y * numVerticalDivisions) / numVerticalDivisions;
						}
						p.cp1 = p.cp1 - p.point + normalizedPos;
						p.cp2 = p.cp2 - p.point + normalizedPos;
						p.point = normalizedPos;
					}
					
					p.cp1 = glm::clamp(p.cp1, glm::vec2(points[i-1].point.x, 0), glm::vec2(p.point.x, 1));
					p.cp2 = glm::clamp(p.cp2, glm::vec2(p.point.x, 0), glm::vec2(points[i+1].point.x, 1));
					
				}
				glm::vec2 pointPos = denormalizePoint(p.point);
				glm::vec2 cp1Pos = denormalizePoint(p.cp1);
				glm::vec2 cp2Pos = denormalizePoint(p.cp2);
				if(ImGui::IsItemClicked(0) && !someItemClicked){
					auto mouseToPointDistance = glm::distance(glm::vec2(ImGui::GetMousePos()), pointPos);
					auto mouseToCp1Distance = glm::distance(glm::vec2(ImGui::GetMousePos()), cp1Pos);
					auto mouseToCp2Distance = glm::distance(glm::vec2(ImGui::GetMousePos()), cp2Pos);
					if(mouseToPointDistance < 5){
						p.drag = 3;
						someItemClicked = true;
					}
					else if(mouseToCp1Distance < 5){
						p.drag = 1;
						someItemClicked = true;
					}
					else if(mouseToCp2Distance < 5){
						p.drag = 2;
						someItemClicked = true;
					}
				}else if(ImGui::IsItemClicked(1)){
					auto mouseToPointDistance = glm::distance(glm::vec2(ImGui::GetMousePos()), pointPos);
					if(mouseToPointDistance < 5){
						ImGui::OpenPopup(("##PointPopup " + ofToString(i)).c_str());
					}
				}
				
				if(ImGui::BeginPopup(("##PointPopup " + ofToString(i)).c_str())){
					auto pointCopy = p.point;
					bool pointEdited = false;
					if(ImGui::SliderFloat("X", &p.point.x, 0, 1)) pointEdited = true;
					if(ImGui::SliderFloat("Y", &p.point.y, 0, 1)) pointEdited = true;
					if(pointEdited){
						p.cp1 = p.cp1 - pointCopy + p.point;
						p.cp2 = p.cp2 - pointCopy + p.point;
						p.cp1 = glm::clamp(p.cp1, glm::vec2(0, 0), glm::vec2(p.point.x, 1));
						p.cp2 = glm::clamp(p.cp2, glm::vec2(p.point.x, 0), glm::vec2(1, 1));
					}
					
					if(ImGui::Selectable("Remove")){
						ImGui::CloseCurrentPopup();
						indexToRemove = i;
					}
					ImGui::EndPopup();
				}
			}
			
			if(indexToRemove != -1){
				points.erase(points.begin()+indexToRemove);
			}
			
			
			if(ImGui::IsItemClicked(0) && !someItemClicked){
				createPoint = true;
			}
			
			draw_list->AddLine(denormalizePoint(glm::vec2(0, points[0].point.y)), denormalizePoint(points[0].point), IM_COL32(10, 10, 10, 255));
			for(int i = 0; i < points.size()-1; i++){
				draw_list->AddBezierCurve(denormalizePoint(points[i].point),
										  denormalizePoint(points[i].cp2),
										  denormalizePoint(points[i+1].cp1),
										  denormalizePoint(points[i+1].point), IM_COL32(255, 127, 0, 127), 2);
				
//				draw_list->AddLine(denormalizePoint(points[i].point), denormalizePoint(points[i+1].point), IM_COL32(10, 10, 10, 255));
			}
			
			for(int i = 0; i < points.size(); i++){
				auto &p = points[i];
				if(i != 0){
					draw_list->AddLine(denormalizePoint(p.cp1), denormalizePoint(p.point), IM_COL32(15, 15, 15, 255));
				}
				if(i != points.size()-1){
					draw_list->AddLine(denormalizePoint(p.cp2), denormalizePoint(p.point), IM_COL32(15, 15, 15, 255));
				}
				
				draw_list->AddCircleFilled(denormalizePoint(p.point), 5, IM_COL32(0, 0, 0, 255));
				if(i != 0){
					draw_list->AddCircleFilled(denormalizePoint(p.cp1), 5, IM_COL32(0, 255, 255, 127));
				}
				if(i != points.size()-1){
					draw_list->AddCircleFilled(denormalizePoint(p.cp2), 5, IM_COL32(255, 0, 255, 127));
				}
			}
			
			draw_list->AddLine(denormalizePoint(points.back().point), denormalizePoint(glm::vec2(1, points.back().point.y)), IM_COL32(10, 10, 10, 255));
			
			for(auto &v : input.get()){
				draw_list->AddLine(denormalizePoint(glm::vec2(v, 0)), denormalizePoint(glm::vec2(v, 1)), IM_COL32(127, 127, 127, 127));
			}
			
			for(auto &p : debugPoints){
				draw_list->AddCircleFilled(denormalizePoint(p), 3, IM_COL32(255, 0, 0, 255));
			}
		}
		ImGui::End();
	}
}

void curve::recalculate()
{
	// https://stackoverflow.com/questions/7348009/y-coordinate-for-a-given-x-cubic-bezier
	auto getYfromXCubicBezier = [](glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, float xTarget) -> glm::vec2{
	  float xTolerance = 0.0001; //adjust as you please

	  //we could do something less stupid, but since the x is monotonic
	  //increasing given the problem constraints, we'll do a binary search.

	  //establish bounds
	  float lower = 0;
	  float upper = 1;
	  float percent = (upper + lower) / 2;

	  //get initial x
	  float x = ofBezierPoint(p0, p1, p2, p3, percent).x;

	  //loop until completion
	  while(abs(xTarget - x) > xTolerance) {
		if(xTarget > x)
		  lower = percent;
		else
		  upper = percent;

		percent = (upper + lower) / 2;
		x = ofBezierPoint(p0, p1, p2, p3, percent).x;
	  }
	  //we're within tolerance of the desired x value.
	  //return the y value.
	  return ofBezierPoint(p0, p1, p2, p3, percent);
	};
	
	debugPoints.resize(input->size());
	vector<float> tempOut(input->size());
	for(int i = 0; i < input->size(); i++){
		auto &p = input->at(i);
		// TODO: updatejar debug point
		if(p <= points[0].point.x){
			tempOut[i] = points[0].point.y;
			debugPoints[i] = glm::vec2(p, points[0].point.y);
		}else if(p >= points.back().point.x){
			tempOut[i] = points.back().point.y;
			debugPoints[i] = glm::vec2(p, points.back().point.y);
		}else{
			for(int j = 1; j < points.size(); j++){
				if(p < points[j].point.x){
					debugPoints[i] = getYfromXCubicBezier(points[j-1].point, points[j-1].cp2, points[j].cp1, points[j].point, p);
					tempOut[i] = debugPoints[i].y;
//					tempOut[i] = ofMap(p, points[j-1].point.x, points[j].point.x, points[j-1].point.y, points[j].point.y);
					break;
				}
			}
		}
	}
	output = tempOut;
}

void curve::presetSave(ofJson &json){
	json["NumPoints"] = points.size();
	for(int i = 0; i < points.size(); i++){
		json["Points"][i]["Point"]["x"] = points[i].point.x;
		json["Points"][i]["Point"]["y"] = points[i].point.y;
		json["Points"][i]["Cp1"]["x"] = points[i].cp1.x;
		json["Points"][i]["Cp1"]["y"] = points[i].cp1.y;
		json["Points"][i]["Cp2"]["x"] = points[i].cp2.x;
		json["Points"][i]["Cp2"]["y"] = points[i].cp2.y;
	}
}

void curve::presetRecallAfterSettingParameters(ofJson &json){
	try{
		points.resize(json["NumPoints"]);
		for(int i = 0; i < points.size(); i++){
			points[i].point.x = json["Points"][i]["Point"]["x"];
			points[i].point.y = json["Points"][i]["Point"]["y"];
			points[i].cp1.x = json["Points"][i]["Cp1"]["x"];
			points[i].cp1.y = json["Points"][i]["Cp1"]["y"];
			points[i].cp2.x = json["Points"][i]["Cp2"]["x"];
			points[i].cp2.y = json["Points"][i]["Cp2"]["y"];
			points[i].firstCreated = false;
		}
	}
	catch (ofJson::exception& e)
	{
		ofLog() << e.what();
	}
	
}
