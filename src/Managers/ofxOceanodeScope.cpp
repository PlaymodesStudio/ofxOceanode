//
//  ofxOceanodeScope.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 05/05/2020.
//
#define IMGUI_DEFINE_MATH_OPERATORS
#include "ofxOceanodeScope.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "ofxOceanodeParameter.h"

// https://github.com/ocornut/imgui/issues/1720
bool Splitter(int splitNum, bool split_vertically, float thickness, float* size1, float* size2, float min_size1, float min_size2, float splitter_long_axis_size = -1.0f)
{
    using namespace ImGui;
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiID id = window->GetID(("##Splitter" + ofToString(splitNum)).c_str());
    ImRect bb;
    bb.Min = window->DC.CursorPos + (split_vertically ? ImVec2(*size1, 0.0f) : ImVec2(0.0f, *size1));
    bb.Max = bb.Min + CalcItemSize(split_vertically ? ImVec2(thickness, splitter_long_axis_size) : ImVec2(splitter_long_axis_size, thickness), 0.0f, 0.0f);
    return SplitterBehavior(bb, id, split_vertically ? ImGuiAxis_X : ImGuiAxis_Y, size1, size2, min_size1, min_size2, 4.0f, 0.04f);
}

void ofxOceanodeScope::setup(){
    scopeTypes.push_back([](ofxOceanodeAbstractParameter *p, ImVec2 size) -> bool{
        // VECTOR FLOAT PARAM
        if(p->valueType() == typeid(std::vector<float>).name())
        {
            auto param = p->cast<std::vector<float>>().getParameter();
            auto size2 = ImGui::GetContentRegionAvail();

            if(param->size() == 1 && size.x > size.y)
            {
                ImGui::ProgressBar((param.get()[0] - param.getMin()[0]) / (param.getMax()[0] - param.getMin()[0]), size2, "");

                if(ImGui::IsItemHovered()){
                    ImGui::BeginTooltip();
                    ImGui::Text("%3f", param.get()[0]);
                    ImGui::EndTooltip();
                }
            }else{
                ImGui::PlotHistogram((p->getName()).c_str(), &param.get()[0], param->size(), 0, NULL, param.getMin()[0], param.getMax()[0], size2);
            }
            return true;
        }

        return false;
    });
}

void ofxOceanodeScope::draw(){

    if(scopedParameters.size() > 0){
        ImGui::Begin("Scopes", NULL, ImGuiWindowFlags_NoScrollbar);
        windowHeight = ImGui::GetContentRegionAvail().y;
        for(int i = 0; i < scopedParameters.size()-1; i++){
            float topHeight = 0;
            float bottomHeight = 0;
                for(int j = 0; j < i+1; j++) topHeight += (scopedParameters[j].sizeRelative / scopedParameters.size() * windowHeight);
                for(int j = i+1; j < scopedParameters.size(); j++) bottomHeight += (scopedParameters[j].sizeRelative / scopedParameters.size() * windowHeight);
            float oldTopHeight = topHeight;
            float oldBottomHeight = bottomHeight;
            
            bool isShiftPresed = ImGui::GetIO().KeyShift;
            
            float minTop = isShiftPresed ? 10 * (i+1) : 10 + topHeight - scopedParameters[i].sizeRelative / scopedParameters.size() * windowHeight;
            float minBottom = isShiftPresed ? 10 * (scopedParameters.size() - (i+1)) : 10 + bottomHeight - scopedParameters[i+1].sizeRelative / scopedParameters.size() * windowHeight;
            
            if(Splitter(i, false, 1, &topHeight, &bottomHeight, minTop, minBottom)){
                if(isShiftPresed){
                    float topScale = topHeight / oldTopHeight;
                    float bottomScale = bottomHeight / oldBottomHeight;
                    for(int j = 0; j < i+1; j++) scopedParameters[j].sizeRelative *= topScale;
                    for(int j = i+1; j < scopedParameters.size(); j++) scopedParameters[j].sizeRelative *= bottomScale;
                }else{
                    float topInc = topHeight - oldTopHeight;
                    float bottomInc = bottomHeight - oldBottomHeight;
                    scopedParameters[i].sizeRelative += topInc * scopedParameters.size() / windowHeight;
                    scopedParameters[i+1].sizeRelative += bottomInc * scopedParameters.size() / windowHeight;
                }
            }
        }
        for(int i = 0; i < scopedParameters.size(); i++)
        {
            auto &p = scopedParameters[i];
            auto itemHeight = (p.sizeRelative / scopedParameters.size() * windowHeight) - 2.5;

            auto size = ImVec2(ImGui::GetContentRegionAvailWidth(), itemHeight);
            
            ImGui::PushStyleColor(ImGuiCol_SliderGrab,ImVec4(p.color*0.75f));
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,ImVec4(p.color*0.75f));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram,ImVec4(p.color*0.75f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55,0.55,0.55,1.0));
            ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(0.0,0.0,0.0,0.0));
            
            ImGui::BeginChild(("Child_" + p.parameter->getGroupHierarchyNames().front() + "/" + p.parameter->getName()).c_str(), size, true);
            if(ImGui::Button("x##RemoveScope"))
            {
                ofxOceanodeScope::getInstance()->removeParameter(p.parameter);
            }
            ImGui::SameLine();
            ImGui::Text((p.parameter->getGroupHierarchyNames().front() + "/" + p.parameter->getName()).c_str());
            ImGui::SameLine();
            if(ImGui::Button("[^]##MoveScopeUp"))
            {
                if(i>0) std::swap(scopedParameters[i],scopedParameters[i-1]);
            }
            ImGui::SameLine();
            if(ImGui::Button("[v]##MoveScopeDown"))
            {
                if(i<scopedParameters.size()-1) std::swap(scopedParameters[i],scopedParameters[i+1]);
                
            }


            // f() function to properly draw each scope item
            for(auto f : scopeTypes)
            {
                if(f(p.parameter, ImVec2(ImGui::GetContentRegionAvailWidth(), itemHeight))) break;
            }
            
            ImGui::PopStyleColor(5);
            ImGui::EndChild();
        }
        ImGui::End();
    }
}

void ofxOceanodeScope::addParameter(ofxOceanodeAbstractParameter* p, ofColor _color){
    p->setScoped(true);
    scopedParameters.emplace_back(p,_color);
}

void ofxOceanodeScope::removeParameter(ofxOceanodeAbstractParameter* p){
    p->setScoped(false);
    auto scopeToRemove = std::find_if(scopedParameters.begin(), scopedParameters.end(), [p](const ofxOceanodeScopeItem& i){return i.parameter == p;});
    float sizeBackup = scopeToRemove->sizeRelative;
    scopedParameters.erase(scopeToRemove);
    for (auto &sp : scopedParameters) {
        sp.sizeRelative += ((sizeBackup - 1) / scopedParameters.size());
    }
    
}
