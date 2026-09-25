//
//  ofxOceanodePresetsController.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 12/03/2018.
//

#include "ofxOceanodePresetsController.h"
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeShared.h"
#include "imgui.h"

namespace {
const ImVec4 bankTextColor(1.0f, 1.0f, 1.0f, 1.0f);
const ImVec4 presetItemTextColor(150.0f / 255.0f, 150.0f / 255.0f, 150.0f / 255.0f, 1.0f);

string sanitizePresetName(string name){
    ofStringReplace(name, " ", "_");
    return name;
}

bool presetNameExists(const vector<string>& existingPresets, const string& requestedName){
    return find(existingPresets.begin(), existingPresets.end(), sanitizePresetName(requestedName)) != existingPresets.end();
}
}

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
    sort(banks.begin(), banks.end());
    currentBank = 0;

    presetListener = container->loadPresetEvent.newListener([this](pair<string, string> presetInfo){
        string bankName = presetInfo.first;
        string presetName = presetInfo.second;
        auto bankPos = std::find(banks.begin(), banks.end(), bankName);
        if(bankPos != banks.end()){
            currentBank = bankPos - banks.begin();
            if(std::find(bankPresets[bankName].begin(), bankPresets[bankName].end(), presetName) != bankPresets[bankName].end()){
                loadPreset(presetName, bankName);
                currentPreset[bankName] = presetName;
            }
        }
    });
    
    presetNumListener = container->loadPresetNumEvent.newListener([this](pair<string, int> presetInfo){
        string bankName = presetInfo.first;
        int presetNum = presetInfo.second;
        
        auto bankPos = std::find(banks.begin(), banks.end(), bankName);
        if(bankPos != banks.end()){
            currentBank = bankPos - banks.begin();
            if(bankPresets[bankName].size() >= presetNum){
                loadPreset(bankPresets[bankName][presetNum-1], bankName);
                currentPreset[bankName] = bankPresets[bankName][presetNum-1];
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
    const string& bankName = banks[currentBank];
    const string& presetName = currentPreset[bankName];
    ImGui::TextUnformatted("Bank:");
    ImGui::SameLine();
    ImGui::TextColored(bankTextColor, "%s", bankName.c_str());
    ImGui::TextUnformatted("Preset:");
    ImGui::SameLine();
    ImGui::TextColored(presetItemTextColor, "%s", presetName.empty() ? "-" : presetName.c_str());
    ImGui::Separator();

    if(ImGui::MenuItem("Save Preset", nullptr, false, !presetName.empty())){
        savePreset(presetName, bankName);
    }
    if(ImGui::MenuItem("Save Preset As...")){
        popupRequest = PopupRequest::SaveAs;
    }
    if(ImGui::MenuItem("Delete Preset...", nullptr, false, !presetName.empty())){
        deleteBankName = bankName;
        deletePresetName = presetName;
        popupRequest = PopupRequest::Delete;
    }
    ImGui::Separator();
    if(ImGui::MenuItem("New Bank...")){
        popupRequest = PopupRequest::NewBank;
    }
    if(ImGui::MenuItem("Reload Macros")){
        ofxOceanodeShared::updateMacrosStructure();
    }
    ImGui::Separator();
    drawPresetList();
}

void ofxOceanodePresetsController::drawPopups(){
    // MenuItem closes the menu. Open and render dialogs here instead, using
    // the persistent DockSpace ID stack supplied by ofxOceanodeControls.
    if(popupRequest == PopupRequest::NewBank){
        bankNameBuffer[0] = '\0';
        ImGui::OpenPopup("Add New Bank");
    }else if(popupRequest == PopupRequest::SaveAs){
        presetNameBuffer[0] = '\0';
        saveAsBank = currentBank;
        ImGui::OpenPopup("Save Preset As");
    }else if(popupRequest == PopupRequest::Delete){
        ImGui::OpenPopup("Delete Preset?");
    }
    popupRequest = PopupRequest::None;

    if(ImGui::BeginPopupModal("Add New Bank", nullptr, ImGuiWindowFlags_AlwaysAutoResize)){
        if(ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 22.0f);
        const bool enter = ImGui::InputText("Bank Name", bankNameBuffer, sizeof(bankNameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
        const string requestedName = sanitizePresetName(bankNameBuffer);
        const bool duplicate = find(banks.begin(), banks.end(), requestedName) != banks.end();
        const bool canCreate = !requestedName.empty() && !duplicate;
        if(duplicate) ImGui::TextDisabled("A bank with this name already exists.");
        ImGui::BeginDisabled(!canCreate);
        const bool create = ImGui::Button("Create");
        ImGui::EndDisabled();
        if(canCreate && (enter || create)){
            banks.push_back(requestedName);
            sort(banks.begin(), banks.end());
            currentBank = distance(banks.begin(), find(banks.begin(), banks.end(), requestedName));
            currentPreset[requestedName] = "";
            bankPresets[requestedName];
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if(ImGui::BeginPopupModal("Save Preset As", nullptr, ImGuiWindowFlags_AlwaysAutoResize)){
        if(ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::PushItemWidth(ImGui::GetFontSize() * 22.0f);
        const bool enter = ImGui::InputText("Preset Name", presetNameBuffer, sizeof(presetNameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
        auto vector_getter = [](void* vec, int idx, const char** out_text){
            auto& values = *static_cast<vector<string>*>(vec);
            if(idx < 0 || idx >= static_cast<int>(values.size())) return false;
            *out_text = values[idx].c_str();
            return true;
        };
        ImGui::Combo("Bank", &saveAsBank, vector_getter, &banks, banks.size());
        ImGui::PopItemWidth();
        const string requestedName = sanitizePresetName(presetNameBuffer);
        const bool duplicate = presetNameExists(bankPresets[banks[saveAsBank]], requestedName);
        const bool canSave = !requestedName.empty() && !duplicate;
        if(duplicate) ImGui::TextDisabled("A preset with this name already exists in this bank.");
        ImGui::BeginDisabled(!canSave);
        const bool save = ImGui::Button("Save");
        ImGui::EndDisabled();
        if(canSave && (enter || save)){
            currentBank = saveAsBank;
            createPreset(requestedName);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if(ImGui::BeginPopupModal("Delete Preset?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::Text("Bank: %s", deleteBankName.c_str());
        ImGui::Text("Preset: %s", deletePresetName.c_str());
        ImGui::Separator();
        const bool confirmWithEnter = !ImGui::IsWindowAppearing() && ImGui::IsKeyPressed(ImGuiKey_Enter, false);
        if(ImGui::Button("Delete", ImVec2(120, 0)) || confirmWithEnter){
            deletePreset(deletePresetName, deleteBankName);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void ofxOceanodePresetsController::drawPresetList(){
    ImGui::TextUnformatted("Load Preset");
    // The menu grows with the expanded banks. ImGui limits oversized popup
    // menus to the viewport and gives the menu itself a scrollbar.
    ImGui::PushStyleColor(ImGuiCol_Text, presetItemTextColor);
    for(int b = 0; b < banks.size(); ++b){
        const string& bankName = banks[b];
        ImGui::SetNextItemOpen(b == currentBank, ImGuiCond_Once);
        ImGui::PushStyleColor(ImGuiCol_Text, bankTextColor);
        const bool bankOpen = ImGui::TreeNode(bankName.c_str());
        ImGui::PopStyleColor();
        if(bankOpen){
            const auto& presets = bankPresets[bankName];
            if(presets.empty()) ImGui::TextDisabled("No presets in this bank");
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(presets.size()));
            while(clipper.Step()){
                for(int n = clipper.DisplayStart; n < clipper.DisplayEnd; ++n){
                    const string label = ofToString(n + 1) + " | " + presets[n];
                    const bool selected = b == currentBank && presets[n] == currentPreset[bankName];
                    ImGui::PushID(n);
                    if(ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_NoAutoClosePopups)){
                        currentBank = b;
                        loadPreset(presets[n], bankName);
                        currentPreset[bankName] = presets[n];
                    }
                    ImGui::PopID();
                }
            }
            ImGui::TreePop();
        }
    }
    ImGui::PopStyleColor();
}

void ofxOceanodePresetsController::createPreset(string name){
    name = sanitizePresetName(name);
    if(name.empty()) return;
    if(presetNameExists(bankPresets[banks[currentBank]], name)){
        ofLogWarning("ofxOceanodePresetsController")
            << "Refusing to create duplicate preset '" << name << "' in bank '" << banks[currentBank] << "'";
        return;
    }
    bankPresets[banks[currentBank]].push_back(name);
    currentPreset[banks[currentBank]] = bankPresets[banks[currentBank]].back();
    savePreset(name, banks[currentBank]);
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
    auto bankIt = bankPresets.find(bank);
    if(bankIt == bankPresets.end()){
        ofLogWarning("ofxOceanodePresetsController") << "Cannot load preset from unknown bank: " << bank;
        return;
    }

    const auto &presets = bankIt->second;
    auto presetIt = find(presets.begin(), presets.end(), name);
    if(presetIt == presets.end()){
        ofLogWarning("ofxOceanodePresetsController") << "Cannot load unknown preset '" << name
                                                     << "' from bank '" << bank << "'";
        return;
    }

    int presetIndex = distance(presets.begin(), presetIt);
    string myPath = "./Presets/" + bank + "/" + ofToString(presetIndex+1) +  "--" + name;
    if(!ofDirectory::doesDirectoryExist(myPath)){
        ofLogWarning("ofxOceanodePresetsController") << "Preset folder does not exist: " << myPath;
        return;
    }

    ofxOceanodeShared::startedLoadingPreset();
	ofxOceanodeShared::setCurrentPresetPath(myPath);
	ofxOceanodeShared::setCurrentBankName(bank);
	ofxOceanodeShared::setCurrentPresetName(name);
    
	container->loadPreset(myPath);

	ofxOceanodeShared::finishedLoadingPreset();
	
}

void ofxOceanodePresetsController::savePreset(string name, string bank)
{
    auto bankIt = bankPresets.find(bank);
    if(bankIt == bankPresets.end()){
        ofLogWarning("ofxOceanodePresetsController") << "Cannot save preset to unknown bank: " << bank;
        return;
    }

    const auto &presets = bankIt->second;
    auto presetIt = find(presets.begin(), presets.end(), name);
    if(presetIt == presets.end()){
        ofLogWarning("ofxOceanodePresetsController") << "Cannot save unknown preset '" << name
                                                     << "' in bank '" << bank << "'";
        return;
    }

    int presetIndex = distance(presets.begin(), presetIt);
    string myPath = "./Presets/" + bank + "/" + ofToString(presetIndex+1) +  "--" + name;
	
	ofxOceanodeShared::setCurrentPresetPath(myPath);
	ofxOceanodeShared::setCurrentBankName(bank);
	ofxOceanodeShared::setCurrentPresetName(name);

	container->savePreset(myPath);
	ofxOceanodeShared::presetWasSaved();
	
}
void ofxOceanodePresetsController::deletePreset(string presetName, string bankName)
{
    // The confirmation dialog holds its own target: external preset loads may
    // change the current bank while the dialog is open.
    auto bankIt = bankPresets.find(bankName);
    if(bankIt == bankPresets.end()) return;
    auto& presets = bankIt->second;
    auto presetIt = find(presets.begin(), presets.end(), presetName);
    if(presetIt == presets.end()) return;
    const int index = distance(presets.begin(), presetIt);

    ofDirectory bankDirectory("./Presets/" + bankName);
    const string bankPath = bankDirectory.getAbsolutePath();
    const string presetPath = bankPath + "/" + ofToString(index + 1) + "--" + presetName;
    if(std::filesystem::remove_all(presetPath)){
        presets.erase(presetIt);
        if(currentPreset[bankName] == presetName) currentPreset[bankName].clear();
        for(int i = index; i < presets.size(); ++i){
            const string oldName = ofToString(i + 2) + "--" + presets[i];
            const string newName = ofToString(i + 1) + "--" + presets[i];
            std::filesystem::rename(bankPath + "/" + oldName, bankPath + "/" + newName);
        }
    }
}
