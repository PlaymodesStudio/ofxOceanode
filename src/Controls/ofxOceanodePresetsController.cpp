//
//  ofxOceanodePresetsController.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 12/03/2018.
//

#ifndef OFXOCEANODE_HEADLESS

#include "ofxOceanodePresetsController.h"
#include "ofxOceanodeContainer.h"
#include "imgui.h"

ofxOceanodePresetsController::ofxOceanodePresetsController(shared_ptr<ofxOceanodeContainer> _container) : ofxOceanodeBaseController(_container, "Presets"){
    //Preset Control
    ofDirectory dir;
    dir.open("Presets");
    if(!dir.exists()){
        dir.createDirectory("Presets");
    }
    for(auto bank = dir.begin() ; bank < dir.end(); bank++){
        if(bank->isDirectory()){
            ofDirectory bankFolder;
            string bankName = bank->getFileName();
            banks.push_back(bankName);
            bankFolder.open("Presets/" + bankName);
            bankFolder.sort();
            for(auto preset = bankFolder.begin(); preset < bankFolder.end(); preset++){
                string presetName = preset->getFileName();
                bankPresets[bankName].push_back(pair<int, string>(ofToInt(ofSplitString(presetName, "--")[0]), presetName));
            }
            
            std::sort(bankPresets[bankName].begin(), bankPresets[bankName].end(), [](pair<int, string> &left, pair<int, string> &right) {
                return left.first< right.first;
            });
            currentPreset[bankName].first = 0;
        }
        
    }
    if(dir.listDir() == 0){
        banks.push_back("Initial_Bank");
    }
    currentBank = 0;

    //TODO: test listeners check if new logic is working, when untitled
    presetListener = container->loadPresetEvent.newListener([this](string preset){
        vector<string> presetInfo = ofSplitString(preset, "/");
        bool foundBank = false;
        for(int i = 0; i < banks.size(); i++){
            if(banks[i] == presetInfo[0]){
                currentBank = i;
                foundBank = true;
                break;
            }
        }
        if(foundBank == true){
            if(find_if(bankPresets[banks[currentBank]].begin(), bankPresets[banks[currentBank]].begin(), [presetInfo](pair<int, string> &preset){
                return preset.second == presetInfo[1];
            }) != bankPresets[banks[currentBank]].end()){
               loadPreset(presetInfo[1], presetInfo[0]);
            }
        }
    });

    saveCurrentPresetListener = container->saveCurrentPresetEvent.newListener([this](){
        if(currentPreset[banks[currentBank]].first == 0){
            createPreset(string("Untitled"));
        }else{
            savePreset(currentPreset[banks[currentBank]].second, banks[currentBank]);
        }
    });
    
    newPresetCreated = false;

    loadPresetInNextUpdate = 0;
}

void ofxOceanodePresetsController::draw(){
    auto vector_getter = [](void* vec, int idx, const char** out_text)
    {
        auto& vector = *static_cast<std::vector<std::string>*>(vec);
        if (idx < 0 || idx >= static_cast<int>(vector.size())) { return false; }
        *out_text = vector.at(idx).c_str();
        return true;
    };
    // Bank related
    ImGui::Text("%s","Bank :");
    ImGui::SameLine();
    if(ImGui::Combo("##Bank", &currentBank, vector_getter, static_cast<void*>(&banks), banks.size())){
        //TODO: Do something when load bank?
    }
    ImGui::SameLine(ImGui::GetWindowWidth() - 23);
    if(ImGui::Button("+")){
        ImGui::OpenPopup("Add New Bank");
    }
    if(ImGui::BeginPopupModal("Add New Bank", NULL, ImGuiWindowFlags_AlwaysAutoResize)){
        static char cString[256];
        if (ImGui::InputText("Bank Name", cString, 256, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            string proposedNewName(cString);
            ofStringReplace(proposedNewName, " ", "_");
            if(find(banks.begin(), banks.end(), proposedNewName) == banks.end()){
                if(proposedNewName != ""){
                    banks.push_back(proposedNewName);
                    currentBank = banks.size()-1;
                    currentPreset[banks[currentBank]].first = 0;
                }
                ImGui::CloseCurrentPopup();
            }
            strcpy(cString, "");
        }
        if(ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsItemActive()){
            strcpy(cString, "");
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();
    
    // Preset related

    // preset name, by default = "-" until it's loaded or saved.
    string presetName = "-";
    if(currentPreset[banks[currentBank]].second=="")
    {
        presetName = "-";
    }
    else presetName = currentPreset[banks[currentBank]].second;
    ImGui::Text("Preset : %s",presetName.c_str());
    ImGui::SameLine(ImGui::GetWindowWidth() - 62);
    if(ImGui::Button("Save")){
        savePreset(currentPreset[banks[currentBank]].second,banks[currentBank]);
    }
    ImGui::SameLine(ImGui::GetWindowWidth() - 23);
    if(ImGui::Button("+##NewPreset")){
        ImGui::OpenPopup("Add New Preset");
    }
    if(ImGui::BeginPopupModal("Add New Preset", NULL, ImGuiWindowFlags_AlwaysAutoResize)){
        static char cString[256];
        if (ImGui::InputText("Preset Name", cString, 256, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            string proposedNewName(cString);
            ofStringReplace(proposedNewName, " ", "_");

            if(strcmp(proposedNewName.c_str(), "") != 0){
                createPreset(string(proposedNewName));
                newPresetCreated=true;
            }
            strcpy(cString, "");
            ImGui::CloseCurrentPopup();
        }
        if(ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsItemActive()){
            strcpy(cString, "");
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }


    float child_h = ImGui::GetContentRegionAvail().y;
    float child_w = ImGui::GetContentRegionAvail().x;
    //ImGuiWindowFlags child_flags = ImGuiWindowFlags_MenuBar;
    ImGui::BeginChild("Preset List", ImVec2(child_w, child_h), true);


    // Draw Preset List
    
    for(auto &p: bankPresets[banks[currentBank]]){
        
        bool isCurrentPreset = (p == currentPreset[banks[currentBank]]);
        
        ImGuiStyle style = ImGui::GetStyle();
    
        if(isCurrentPreset) ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TabHovered]);
        
        if(ImGui::Button(p.second.c_str())){
            if(ImGui::GetIO().KeyShift){
                savePreset(p.second, banks[currentBank]);
            }else{
                loadPreset(p.second, banks[currentBank]);
                currentPreset[banks[currentBank]] = p;
            }
        }
        if(isCurrentPreset) ImGui::PopStyleColor();

        if(newPresetCreated && p == bankPresets[banks[currentBank]].back()){
            ImGui::SetScrollHereY(0.0f);
            newPresetCreated = false;
        }
    }
    ImGui::EndChild();
}

void ofxOceanodePresetsController::createPreset(string name){
    char newPresetName;
    int newPresetNum = 1;
    if(bankPresets[banks[currentBank]].size() != 0){
        int lastPreset = bankPresets[banks[currentBank]].back().first;
        newPresetNum = lastPreset + 1;
    }
    sprintf(&newPresetName, "%d--%s", newPresetNum, name.c_str());
    string newPresetString(&newPresetName);
    ofStringReplace(newPresetString, " ", "_");
    bankPresets[banks[currentBank]].push_back(pair<int, string>(newPresetNum, newPresetString));
    currentPreset[banks[currentBank]] = bankPresets[banks[currentBank]].back();
    savePreset(newPresetString, banks[currentBank]);
    newPresetCreated = true;
}

void ofxOceanodePresetsController::update(){
    //TODO: Test functionality
    if(loadPresetInNextUpdate != 0){
        int toLoad = loadPresetInNextUpdate;
        auto itemToLoad = find_if(bankPresets[banks[currentBank]].begin(), bankPresets[banks[currentBank]].end(), [toLoad](pair<int, string> &preset){
            return preset.first == toLoad;
        });
        if(itemToLoad != bankPresets[banks[currentBank]].end()){
            loadPreset(itemToLoad->second, banks[currentBank]);
            currentPreset[banks[currentBank]] = *itemToLoad;
        }
        loadPresetInNextUpdate = 0;
    }
}


void ofxOceanodePresetsController::loadPresetFromNumber(int num){
    loadPresetInNextUpdate = num;
}

void ofxOceanodePresetsController::loadPreset(string name, string bank){
    container->loadPreset("Presets/" + bank + "/" + name);
}

void ofxOceanodePresetsController::savePreset(string name, string bank){
    container->savePreset("Presets/" + bank + "/" + name);
}

#endif
