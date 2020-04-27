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

ofxOceanodePresetsController::ofxOceanodePresetsController(shared_ptr<ofxOceanodeContainer> _container) : container(_container), ofxOceanodeBaseController("Presets"){
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
            bankPresets[bankName].resize(bankFolder.size());

            
            for(auto preset = bankFolder.begin(); preset < bankFolder.end(); preset++){
                string presetName = preset->getFileName();
                bankPresets[bankName][ofToInt(ofSplitString(presetName, "--")[0])-1]=ofSplitString(presetName, "--")[1];
            }
            currentPreset[bankName] = "";
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
        //TODO: add 2 find embeeded ... find bank and then preset
        for(int i = 0; i < banks.size(); i++){
            if(banks[i] == presetInfo[0]){
                currentBank = i;
                foundBank = true;
                break;
            }
        }
        if(foundBank == true){
            if(find(bankPresets[banks[currentBank]].begin(),
                    bankPresets[banks[currentBank]].end(),
                    presetInfo[1])
                    != bankPresets[banks[currentBank]].end())
            {
                loadPreset(presetInfo[1], presetInfo[0]);
            }
        }
    });

//    saveCurrentPresetListener = container->saveCurrentPresetEvent.newListener([this](){
//        if(currentPreset[banks[currentBank]].first == 0){
//            createPreset(string("Untitled"));
//        }else{
//            savePreset(currentPreset[banks[currentBank]].second, banks[currentBank]);
//        }
//    });
    
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
    ImGui::Text("%s","Bank:");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55,0.55,0.55,1.0));
    ImGui::Text("%s",banks[currentBank].c_str());
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55,0.55,0.55,1.0));
    if(ImGui::Button("[+]")){
        ImGui::OpenPopup("Add New Bank");
    }
    ImGui::PopStyleColor();
    
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
                    currentPreset[banks[currentBank]] = "";
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
    if(currentPreset[banks[currentBank]]=="")
    {
        presetName = "-";
    }
    else presetName = currentPreset[banks[currentBank]];
    ImGui::Text("Preset: ");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6,0.6,0.6,1.0));
    ImGui::Text("%s",presetName.c_str());
    ImGui::PopStyleColor(1);

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55,0.55,0.55,1.0));

    ImGui::Separator();
    if(ImGui::Button("[+]")){}
    ImGui::SameLine();
    if(ImGui::Button("[-]")){
        deletePreset(presetName,banks[currentBank]);
    }
    ImGui::SameLine();
    if(ImGui::Button("[S]")){
        if(presetName != "-") savePreset(currentPreset[banks[currentBank]],banks[currentBank]);
    }
    ImGui::SameLine();
    if(ImGui::Button("[SA]")){
        ImGui::OpenPopup("Save preset as :");
    }
   ImGui::PopStyleColor(1);

    ImGui::SetNextWindowSize(ImVec2(200,100));
    if(ImGui::BeginPopupModal("Save preset as :", NULL)){
        static char cString[256];
        //ImGui::SetKeyboardFocusHere(0);
        
        if (ImGui::InputText("##Preset Name : ", cString, 256, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            string proposedNewName(cString);
            ofStringReplace(proposedNewName, " ", "_");

            bool nameExists=false;
            for(int i=0;i<bankPresets[banks[currentBank]].size();i++)
            {
                if(proposedNewName==bankPresets[banks[currentBank]][i])
                {
                    nameExists = true;
                }
            }
            
            if(!nameExists)
            {
                if(strcmp(proposedNewName.c_str(), "") != 0){
                    createPreset(string(proposedNewName));
                    newPresetCreated=true;
                }
                strcpy(cString, "");
                ImGui::CloseCurrentPopup();
            }
            else
            {
                cout << "Preset name already existing : " << proposedNewName << endl;
                strcpy(cString, "");
                //ImGui::CloseCurrentPopup();
                // TODO : why this doens't show up ?
                ImGui::OpenPopup("Preset name already exists.");
                if(ImGui::BeginPopupModal("Preset name already exists.", NULL,ImGuiWindowFlags_AlwaysAutoResize))
                {
                    if (ImGui::Button("OK", ImVec2(120,0)) || ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Enter))) {
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
        }
        
        if(ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsItemActive()){
            strcpy(cString, "");
        }
        if(ImGui::Combo("Bank ", &currentBank, vector_getter, static_cast<void*>(&banks), banks.size())){
            //TODO: Do something when load bank?
        }
        if (ImGui::Button("Cancel"))
        {
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

    ImGuiStyle style = ImGui::GetStyle();

    for(int b=0;b<banks.size();b++)
    {
        if(b==currentBank)
        {
            ImGui::PushStyleColor(ImGuiCol_Text,style.Colors[ImGuiCol_TabHovered]);
            ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(1.0f,1.0f,0.0f,1.0f));
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0.55f,0.55f,0.55f,1.00f));
            ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(1.0f,0.0f,0.0f,1.0f));
        }
        
        ImGui::SetNextItemOpen(true);
        
        if (ImGui::TreeNode(banks[b].c_str()))
        {
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text,style.Colors[ImGuiCol_Text]);
            string presetName;

            ImGui::PushStyleColor(ImGuiCol_HeaderActive,style.Colors[ImGuiCol_TabActive] );
            for (int n = 0; n < bankPresets[banks[b]].size(); n++)
            {
                presetName = bankPresets[banks[b]][n];
                
                bool isCurrentPreset = (presetName == currentPreset[banks[currentBank]]);
                if((isCurrentPreset)&&(b==currentBank)) ImGui::PushStyleColor(ImGuiCol_Text,style.Colors[ImGuiCol_TabHovered]);
                
                ImGui::Text((ofToString(n+1)+"|").c_str());
                ImGui::SameLine();
                ImGui::Selectable((presetName).c_str());
                if((isCurrentPreset)&&(b==currentBank)) ImGui::PopStyleColor();
                
                // reordering presets
                if (ImGui::IsItemActive() && !ImGui::IsItemHovered())
                {
                    currentPreset[banks[b]] = presetName;
                    
                    int n_next = n + (ImGui::GetMouseDragDelta(0).y < 0.f ? -1 : 1);
                    if (n_next >= 0 && n_next < bankPresets[banks[b]].size())
                    {
                        string nOldPresetName = ofToString(n+1) +  "--" + bankPresets[banks[b]][n];
                        string nNextOldPresetName = ofToString(n_next+1) +  "--" + bankPresets[banks[b]][n_next];

                        // reorder data
                        bankPresets[banks[b]][n] = bankPresets[banks[b]][n_next];
                        bankPresets[banks[b]][n_next] = presetName;
                        
                        cout << n << " _ " << bankPresets[banks[b]][n] << endl;
                        cout << n_next << " _" << bankPresets[banks[b]][n_next] << endl;
                        
                        ImGui::ResetMouseDragDelta();

                        // reorder in folder
                        string nNewPresentName =  ofToString(n+1) +  "--" + bankPresets[banks[b]][n];
                        string nNextNewPresetName = ofToString(n_next+1) +  "--" + bankPresets[banks[b]][n_next];
                        string dirPath = "./Presets/" + banks[currentBank];
                        ofDirectory dir;
                        dir.open(dirPath);
                        cout << dir.getAbsolutePath() << endl;

                        std::filesystem::rename(dir.getAbsolutePath()+"/"+nOldPresetName,dir.getAbsolutePath()+"/" + nNextNewPresetName);
                        std::filesystem::rename(dir.getAbsolutePath()+"/"+nNextOldPresetName,dir.getAbsolutePath()+"/" + nNewPresentName);
                        
                    }

                }
                if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(0))
                {
                    currentBank = b;
                    currentPreset[banks[b]] = presetName;
                    
                    if(ImGui::GetIO().KeyShift){
                        savePreset(bankPresets[banks[b]][n], banks[b]);
                    }else{
                        loadPreset(bankPresets[banks[b]][n], banks[b]);
                        currentPreset[banks[b]] = bankPresets[banks[b]][n];

                    }

                }
            }
            ImGui::TreePop();
            ImGui::PopStyleColor(2);
        }
        ImGui::PopStyleColor(2);
    }
    ImGui::EndChild();
}

void ofxOceanodePresetsController::createPreset(string name){
    char newPresetName;
    int newPresetNum = 1;
    if(bankPresets[banks[currentBank]].size() != 0){
        int lastPreset = bankPresets[banks[currentBank]].size();
        newPresetNum = lastPreset + 1;
    }
    sprintf(&newPresetName, "%s", name.c_str());
    string newPresetString(&newPresetName);
    ofStringReplace(newPresetString, " ", "_");
    bankPresets[banks[currentBank]].push_back(newPresetString);
    currentPreset[banks[currentBank]] = bankPresets[banks[currentBank]].back();
    savePreset(newPresetString, banks[currentBank]);
    newPresetCreated = true;
}

void ofxOceanodePresetsController::update(){
    //TODO: Test functionality
    if(loadPresetInNextUpdate != 0){
        int toLoad = loadPresetInNextUpdate;
        string itemToLoad;
        if(toLoad<bankPresets[banks[currentBank]].size())
        {
            itemToLoad = bankPresets[banks[currentBank]][toLoad];
            loadPreset(itemToLoad, banks[currentBank]);
            currentPreset[banks[currentBank]] = itemToLoad;
        }
        loadPresetInNextUpdate = 0;
    }
}


void ofxOceanodePresetsController::loadPresetFromNumber(int num){
    loadPresetInNextUpdate = num;
}

void ofxOceanodePresetsController::loadPreset(string name, string bank){
    
    int presetIndex = distance(bankPresets[banks[currentBank]].begin(),
                               find(bankPresets[banks[currentBank]].begin(),
                                    bankPresets[banks[currentBank]].end(),
                                    name)
                               );
    
    string myPath = "./Presets/" + banks[currentBank] +"/" + ofToString(presetIndex+1) +  "--" + name;
    container->loadPreset(myPath);
}

void ofxOceanodePresetsController::savePreset(string name, string bank)
{
    int presetIndex = distance(bankPresets[banks[currentBank]].begin(),
                     find(bankPresets[banks[currentBank]].begin(),
                          bankPresets[banks[currentBank]].end(),
                          name)
                     );
    string myPath = "./Presets/" + banks[currentBank] +"/" + ofToString(presetIndex+1) +  "--" + name;
    container->savePreset(myPath);
}
void ofxOceanodePresetsController::deletePreset(string presetName, string bankName)
{
    // delete from folder
    ofDirectory dir;
    int n = distance(bankPresets[banks[currentBank]].begin(),
                     find(bankPresets[banks[currentBank]].begin(),
                          bankPresets[banks[currentBank]].end(),
                          presetName)
                     );
    
    string myPath = "./Presets/" + banks[currentBank] +"/" + ofToString(n+1) +  "--" + presetName;
    dir.open(myPath);
    cout << dir.getAbsolutePath() << endl;
    if(std::filesystem::remove_all(dir.getAbsolutePath()))
    {
        // delete data
        cout << "Deleted :" << dir.getAbsolutePath() << endl;
        bankPresets[banks[currentBank]].erase(find(bankPresets[banks[currentBank]].begin(),bankPresets[banks[currentBank]].end(),currentPreset[banks[currentBank]]));
        
        // rename needed data and folders
        string dirPath = "./Presets/" + banks[currentBank];
        ofDirectory dir;
        dir.open(dirPath);
        cout << dir.getAbsolutePath() << endl;
        for(int i=n;i<bankPresets[banks[currentBank]].size();i++){
            string oldPresentName =  ofToString(i+2) +  "--" + bankPresets[banks[currentBank]][i];
            string newPresentName =  ofToString(i+1) +  "--" + bankPresets[banks[currentBank]][i];
            cout << "renaming "<< oldPresentName << " to " << newPresentName << endl;
            std::filesystem::rename(dir.getAbsolutePath()+"/"+oldPresentName,dir.getAbsolutePath()+"/" + newPresentName);
        }

    }
    update();
    
    
    
}
#endif
