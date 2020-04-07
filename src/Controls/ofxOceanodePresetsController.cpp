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
        }
    }
    if(dir.listDir() == 0){
        banks.push_back("Initial_Bank");
    }
    currentBank = 0;
    banks.push_back(" -- NEW BANK -- ");

//    gui->addTextInput("New Preset");

    presetListener = container->loadPresetEvent.newListener([this](string preset){
//        vector<string> presetInfo = ofSplitString(preset, "/");
//        oldPresetButton = nullptr;
//        bool foundBank = false;
//        for(int i = 0; i < bankSelect->getNumOptions(); i++){
//            if(bankSelect->getChildAt(i)->getName() == presetInfo[0]){
//                bankSelect->select(i);
//                foundBank = true;
//                break;
//            }
//        }
//        if(foundBank == true){
//            loadBank();
//            if(presetsList->getItemByName(presetInfo[1]) != nullptr)
//               changePresetLabelHighliht(presetsList->getItemByName(presetInfo[1]));
//               loadPreset(presetInfo[1], presetInfo[0]);
//        }
    });

    saveCurrentPresetListener = container->saveCurrentPresetEvent.newListener([this](){
//        if(currentPreset == "Untitled"){
////            ofxDatGuiTextInputEvent tie(nullptr, "Untitled");
////            onGuiTextInputEvent(tie);
//        }else{
//            savePreset(currentPreset, currentBank);
//        }
    });

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
    
    ImGui::Begin(controllerName.c_str()),
    ImGui::Combo("Bank", &currentBank, vector_getter, static_cast<void*>(&banks), banks.size());
    ImGui::Text("%s", "<== Presets List ==>");
    ImGui::BeginGroup();
    for(auto &p: bankPresets[banks[currentBank]]){
        if(ImGui::Button(p.second.c_str())){
            if(ImGui::GetIO().KeyShift){
                savePreset(p.second, banks[currentBank]);
            }else{
                loadPreset(p.second, banks[currentBank]);
            }
        }
    }
    ImGui::Text("-----------");
    ImGui::Separator();
    static char cString[256] = "";
    if (ImGui::InputText("New Preset", cString, 256, ImGuiInputTextFlags_EnterReturnsTrue))
    {
        if(strcmp(cString, "") != 0){
            char newPresetName;
            int newPresetNum = 1;
            if(bankPresets[banks[currentBank]].size() != 0){
                int lastPreset = bankPresets[banks[currentBank]].back().first;
                newPresetNum = lastPreset + 1;
            }
            sprintf(&newPresetName, "%d--%s", newPresetNum, cString);
            string newPresetString(&newPresetName);
            ofStringReplace(newPresetString, " ", "_");
            bankPresets[banks[currentBank]].push_back(pair<int, string>(newPresetNum, newPresetString));
            currentPreset[banks[currentBank]] = bankPresets[banks[currentBank]].back();
            savePreset(newPresetString, banks[currentBank]);
        }
        strcpy(cString, "");
    }
    ImGui::EndGroup();
    ImGui::End();
}

void ofxOceanodePresetsController::update(){
//    if(loadPresetInNextUpdate != 0){
//        if(currentBankPresets.count(loadPresetInNextUpdate) > 0){
//            changePresetLabelHighliht(presetsList->getItemByName(currentBankPresets[loadPresetInNextUpdate]));
//            loadPreset(currentBankPresets[loadPresetInNextUpdate], bankSelect->getSelected()->getName());
//        }
//        loadPresetInNextUpdate = 0;
//    }
}


void ofxOceanodePresetsController::loadPresetFromNumber(int num){
    loadPresetInNextUpdate = num;
}

//void ofxOceanodePresetsController::onGuiScrollViewEvent(ofxDatGuiScrollViewEvent e){
//    if(ofGetKeyPressed(OF_KEY_SHIFT)){
//        changePresetLabelHighliht(e.target);
//        savePreset(e.target->getName(), bankSelect->getSelected()->getName());
//    }else{
//        changePresetLabelHighliht(e.target);
//        loadPreset(e.target->getName(), bankSelect->getSelected()->getName());
//    }
//}
//
//void ofxOceanodePresetsController::onGuiTextInputEvent(ofxDatGuiTextInputEvent e){
//    if(e.text != ""){
//        string newPresetName;
//        int newPresetNum;
//        if(presetsList->getNumItems() != 0){
//            string lastPreset = presetsList->getItemAtIndex(presetsList->getNumItems()-1)->getName();
//            newPresetNum = ofToInt(ofSplitString(lastPreset, "--")[0]) + 1;
//            newPresetName = ofToString(newPresetNum) + "--" + e.text;
//        }else
//            newPresetName = "1--" + e.text;
//        
//        ofStringReplace(newPresetName, " ", "_"); 
//        presetsList->add(newPresetName);
//        currentBankPresets[newPresetNum] = newPresetName;
//        changePresetLabelHighliht(presetsList->getItemAtIndex(presetsList->getNumItems()-1));
//        savePreset(newPresetName, bankSelect->getSelected()->getName());
//        currentBank = bankSelect->getSelected()->getName();
//        currentPreset = newPresetName;
//        e.text = "";
//    }
//}
//
//void ofxOceanodePresetsController::windowResized(ofResizeEventArgs &a){
//    ofxOceanodeBaseController::windowResized(a);
//    int layoutHeight = mainGuiTheme->layout.height;
//    presetsList->setNumVisible(floor((ofGetHeight()-((layoutHeight+1.5)*3))/(float)(layoutHeight+1.5)));
//}

//void ofxOceanodePresetsController::changePresetLabelHighliht(ofxDatGuiButton *presetToHighlight){
//    if(presetToHighlight != nullptr){
//        if(oldPresetButton != nullptr) oldPresetButton->setTheme(mainGuiTheme);
//        presetToHighlight->setLabelColor(ofColor::red);
//        oldPresetButton = presetToHighlight;
//    }
//}

void ofxOceanodePresetsController::loadBank(){

}

void ofxOceanodePresetsController::loadPreset(string name, string bank){
    container->loadPreset("Presets/" + bank + "/" + name);
//    currentPreset = name;
//    currentBank = bank;
}

void ofxOceanodePresetsController::savePreset(string name, string bank){
    container->savePreset("Presets/" + bank + "/" + name);
}

#endif
