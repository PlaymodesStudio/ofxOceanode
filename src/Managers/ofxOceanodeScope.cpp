//
//  ofxOceanodeScope.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 05/05/2020.
//

#include "ofxOceanodeScope.h"
#include "imgui.h"
#include "ofxOceanodeParameter.h"

void ofxOceanodeScope::setup(){
    scopeTypes.push_back([](ofxOceanodeAbstractParameter *p) -> bool{
        if(p->valueType() == typeid(std::vector<float>).name()){
            auto param = p->cast<std::vector<float>>().getParameter();
            auto size = ImGui::GetContentRegionAvail();
            if(param->size() == 1 && size.x > size.y){
                ImGui::ProgressBar((param.get()[0] - param.getMin()[0]) * (param.getMax()[0] - param.getMin()[0]), size, "");
                if(ImGui::IsItemHovered()){
                    ImGui::BeginTooltip();
                    ImGui::Text("%3f", param.get()[0]);
                    ImGui::EndTooltip();
                }
            }else{
                ImGui::PlotHistogram(p->getName().c_str(), &param.get()[0], param->size(), 0, NULL, param.getMin()[0], param.getMax()[0], size);
            }
            return true;
        }
        return false;
    });
}

void ofxOceanodeScope::draw(){
    for(auto p : scopedParameters){
        ImGui::Begin(("Scope " + p->getGroupHierarchyNames().front() + "/" + p->getName()).c_str());
        for(auto f : scopeTypes){
            if(f(p)) break;
        }
        ImGui::End();
    }
}

void ofxOceanodeScope::addParameter(ofxOceanodeAbstractParameter* p){
    scopedParameters.push_back(p);
    p->setScoped(true);
}

void ofxOceanodeScope::removeParameter(ofxOceanodeAbstractParameter* p){
    scopedParameters.erase(std::remove(scopedParameters.begin(), scopedParameters.end(), p));
    p->setScoped(false);
}
