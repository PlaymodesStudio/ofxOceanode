//
//  ofxOceanodePresetsController.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 12/03/2018.
//

#include "ofxOceanodePresetsController.h"
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeNodeMacro.h"
#include "ofxOceanodeShared.h"
#include "ofxOceanodeNode.h"
#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeNodeGui.h"
#include <filesystem>
#include <map>
#include <set>
#include "imgui.h"

namespace {
const ImVec4 bankTextColor(1.0f, 1.0f, 1.0f, 1.0f);
const ImVec4 presetItemTextColor(150.0f / 255.0f, 150.0f / 255.0f, 150.0f / 255.0f, 1.0f);

void drawReviewText(const std::string& text, ImGuiCol color, bool bold = false){
    ImFont* font = bold ? ofxOceanodeShared::getDefaultBoldFont() : nullptr;
    if(font) ImGui::PushFont(font);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(color));
    ImGui::TextWrapped("%s", text.c_str());
    ImGui::PopStyleColor();
    if(font) ImGui::PopFont();
}

void drawReviewLabelValue(const std::string& label, const std::string& value){
    drawReviewText(label, ImGuiCol_Text, true);
    ImGui::SameLine();
    drawReviewText(value, ImGuiCol_TextDisabled);
}

void drawReviewChangeTitle(const std::string& path, const std::string& parameter){
    const std::string prefix = path + " / ";
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float prefixWidth = ImGui::CalcTextSize(prefix.c_str()).x;
    ImFont* boldFont = ofxOceanodeShared::getDefaultBoldFont();
    if(boldFont) ImGui::PushFont(boldFont);
    const float parameterWidth = ImGui::CalcTextSize(parameter.c_str()).x;
    if(boldFont) ImGui::PopFont();

    drawReviewText(prefix, ImGuiCol_TextDisabled);
    // Keep long paths wrapped; move the parameter to the next line if needed.
    if(prefixWidth + parameterWidth <= availableWidth) ImGui::SameLine(0.0f, 0.0f);
    drawReviewText(parameter, ImGuiCol_Text, true);
}

string sanitizePresetName(string name){
    ofStringReplace(name, " ", "_");
    return name;
}

bool presetNameExists(const vector<string>& existingPresets, const string& requestedName){
    return find(existingPresets.begin(), existingPresets.end(), sanitizePresetName(requestedName)) != existingPresets.end();
}

bool sameChanges(const GlobalMacroSaveReview& a, const GlobalMacroSaveReview& b){
    if(a.changes.size() != b.changes.size()) return false;
    for(size_t i = 0; i < a.changes.size(); ++i){
        const auto& left = a.changes[i];
        const auto& right = b.changes[i];
        if(left.kind != right.kind || left.item != right.item || left.field != right.field ||
           left.saved != right.saved || left.current != right.current ||
           left.savedFull != right.savedFull || left.currentFull != right.currentFull) return false;
    }
    return true;
}

vector<GlobalMacroSaveReview> collapseEquivalentReviews(vector<GlobalMacroSaveReview> reviews){
    vector<GlobalMacroSaveReview> uniqueReviews;
    for(auto& review : reviews){
        auto duplicate = std::find_if(uniqueReviews.begin(), uniqueReviews.end(), [&](const auto& prior){
            return prior.globalPath == review.globalPath && sameChanges(prior, review);
        });
        if(duplicate != uniqueReviews.end()) duplicate->alsoUsedAt.push_back(review.instancePath);
        else uniqueReviews.push_back(std::move(review));
    }
    return uniqueReviews;
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
    currentBank = -1; // No bank selected until a preset is loaded/saved or a bank is created.

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
    const bool hasBank = currentBank >= 0 && currentBank < static_cast<int>(banks.size());
    const string bankName = hasBank ? banks[currentBank] : "";
    const string presetName = hasBank ? currentPreset[bankName] : "";
    ImGui::TextUnformatted("Bank:");
    ImGui::SameLine();
    ImGui::TextColored(bankTextColor, "%s", hasBank ? bankName.c_str() : "-");
    ImGui::TextUnformatted("Preset:");
    ImGui::SameLine();
    ImGui::TextColored(presetItemTextColor, "%s", presetName.empty() ? "-" : presetName.c_str());
    ImGui::Separator();

    // Allow repeated preset actions without dismissing the menu.
    ImGui::PushItemFlag(ImGuiItemFlags_AutoClosePopups, false);
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
    ImGui::PopItemFlag();
    ImGui::Separator();
    drawPresetList();
}

void ofxOceanodePresetsController::drawPopups(){
    // Render dialogs outside the menu using the persistent DockSpace ID stack
    // supplied by ofxOceanodeControls, so they remain open independently.
    if(popupRequest == PopupRequest::NewBank){
        bankNameBuffer[0] = '\0';
        ImGui::OpenPopup("Add New Bank");
    }else if(popupRequest == PopupRequest::SaveAs){
        presetNameBuffer[0] = '\0';
        saveAsBank = currentBank;
        ImGui::OpenPopup("Save Preset As");
    }else if(popupRequest == PopupRequest::Delete){
        ImGui::OpenPopup("Delete Preset?");
    }else if(popupRequest == PopupRequest::ReviewSave){
        ImGui::OpenPopup("Review Global Macros");
    }else if(popupRequest == PopupRequest::SaveResult){
        ImGui::OpenPopup("Preset Save Result");
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
        const bool hasBank = saveAsBank >= 0 && saveAsBank < static_cast<int>(banks.size());
        const bool duplicate = hasBank && presetNameExists(bankPresets[banks[saveAsBank]], requestedName);
        const bool canSave = hasBank && !requestedName.empty() && !duplicate;
        if(!hasBank) ImGui::TextDisabled("Select a bank to save the preset.");
        if(duplicate) ImGui::TextDisabled("A preset with this name already exists in this bank.");
        ImGui::BeginDisabled(!canSave);
        const bool save = ImGui::Button("Save");
        ImGui::EndDisabled();
        if(canSave && (enter || save)){
            beginSavePreset(requestedName, banks[saveAsBank], true);
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

    drawGlobalMacroSaveReview();

    const float resultWidth = std::min(620.0f, ImGui::GetIO().DisplaySize.x * 0.85f);
    ImGui::SetNextWindowSizeConstraints(ImVec2(resultWidth, 0.0f),
                                        ImVec2(resultWidth, ImGui::GetIO().DisplaySize.y * 0.85f));
    if(ImGui::BeginPopupModal("Preset Save Result", nullptr, ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::TextWrapped("%s", saveResultText.c_str());
        if(ImGui::Button("OK") || ImGui::IsKeyPressed(ImGuiKey_Enter)) ImGui::CloseCurrentPopup();
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

void ofxOceanodePresetsController::update(){
    //TODO: Test functionality
    if(loadPresetInNextUpdate != 0){
        int toLoad = loadPresetInNextUpdate;
        string itemToLoad;
        if(currentBank >= 0 && currentBank < static_cast<int>(banks.size()) &&
           toLoad >= 0 && toLoad < bankPresets[banks[currentBank]].size())
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

    beginSavePreset(name, bank, false);
}

void ofxOceanodePresetsController::beginSavePreset(string name, string bank, bool createNew){
    name = sanitizePresetName(name);
    if(name.empty() || bankPresets.find(bank) == bankPresets.end()) return;
    if(createNew && presetNameExists(bankPresets[bank], name)) return;

    auto request = std::make_unique<PendingSave>();
    request->name = std::move(name);
    request->bank = std::move(bank);
    request->createNew = createNew;
    request->reviews = collapseEquivalentReviews(collectGlobalMacroSaveReviews(*container));
    request->saveChoices.resize(request->reviews.size(), false);
    pendingSave = std::move(request);
    if(pendingSave->reviews.empty()) finishSavePreset();
    else popupRequest = PopupRequest::ReviewSave;
}

void ofxOceanodePresetsController::drawGlobalMacroSaveReview(){
    const float reviewWidth = std::min(920.0f, ImGui::GetIO().DisplaySize.x * 0.85f);
    ImGui::SetNextWindowSizeConstraints(ImVec2(reviewWidth, 0.0f),
                                        ImVec2(reviewWidth, ImGui::GetIO().DisplaySize.y * 0.9f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
    const bool open = ImGui::BeginPopupModal("Review Global Macros", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PopStyleColor();
    if(!open) return;
    if(!pendingSave || pendingSave->reviewIndex >= pendingSave->reviews.size()){
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }

    auto& request = *pendingSave;
    auto& review = request.reviews[request.reviewIndex];
    drawReviewLabelValue("Preset:", request.bank + " / " + request.name + "  |  Review " +
                         ofToString(request.reviewIndex + 1) + " of " + ofToString(request.reviews.size()));
    drawReviewLabelValue("Macro:", review.name);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::TextWrapped("%s", review.instancePath.c_str());
    for(const auto& path : review.alsoUsedAt) ImGui::TextWrapped("Also used at: %s", path.c_str());
    ImGui::PopStyleColor();
    if(!request.warning.empty()) ImGui::TextWrapped("%s", request.warning.c_str());

    bool samePathAlreadySelected = false;
    bool conflictingInstances = false;
    for(size_t i = 0; i < request.reviews.size(); ++i){
        if(i == request.reviewIndex || request.reviews[i].globalPath != review.globalPath) continue;
        if(!sameChanges(request.reviews[i], review)) conflictingInstances = true;
        if(i < request.reviewIndex && request.saveChoices[i]) samePathAlreadySelected = true;
    }
    if(conflictingInstances) ImGui::TextWrapped("Other instances of this macro have different edits. Choose only one instance to save as the global macro.");
    if(samePathAlreadySelected) ImGui::TextWrapped("Another instance has already been chosen to save this global macro.");

    ImGui::Separator();
    // Size short reviews to their content and keep longer lists scrollable.
    const float maxHeight = std::min(ImGui::GetTextLineHeightWithSpacing() * 18.0f,
                                     ImGui::GetIO().DisplaySize.y * 0.45f);
    ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(reviewWidth, maxHeight));
    ImGui::BeginChild("Changes", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AutoResizeY);
    for(size_t changeIndex = 0; changeIndex < review.changes.size(); ++changeIndex){
        const auto& change = review.changes[changeIndex];
        ImGui::PushID(static_cast<int>(changeIndex));
        if(changeIndex > 0) ImGui::Spacing();
        std::string changeTitle = review.name + " / ";
        if(change.location != review.instancePath){
            const std::string prefix = review.instancePath + " > ";
            changeTitle += (change.location.rfind(prefix, 0) == 0
                            ? change.location.substr(prefix.size()) : change.location) + " / ";
        }
        changeTitle += change.item;
        drawReviewChangeTitle(changeTitle, change.kind == GlobalMacroSaveChange::Kind::MacroReplacement
                                          ? "Macro replacement" : change.field);
        if(change.field == "Node added" || change.field == "Connection added"){
            ImGui::NewLine();
        }else{
            ImGui::Indent();
            drawReviewLabelValue("Old:", change.saved);
            drawReviewLabelValue("New:", change.current);
            ImGui::Unindent();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();

    if(request.writing){
        if(ImGui::Button("Retry macro save")){
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            finishSavePreset();
            return;
        }
        ImGui::SameLine();
        if(ImGui::Button("Stop saving")){
            saveResultText = "Preset was not saved. A global macro save failed and may have changed files in " +
                             review.globalPath + ".";
            for(const auto& name : request.savedMacros) saveResultText += "\nSaved before failure: " + name;
            pendingSave.reset();
            ImGui::CloseCurrentPopup();
            popupRequest = PopupRequest::SaveResult;
        }
        ImGui::EndPopup();
        return;
    }

    ImGui::BeginDisabled(samePathAlreadySelected);
    const bool saveMacro = ImGui::Button("Save this macro");
    ImGui::EndDisabled();
    ImGui::SameLine();
    const bool skipMacro = ImGui::Button("Skip this macro");
    ImGui::SameLine();
    const bool cancel = ImGui::Button("Cancel preset save") || ImGui::IsKeyPressed(ImGuiKey_Escape);
    if(cancel){
        pendingSave.reset();
        ImGui::CloseCurrentPopup();
    }else if(saveMacro || skipMacro){
        request.saveChoices[request.reviewIndex] = saveMacro;
        request.reviewIndex++;
        request.warning.clear();
        if(request.reviewIndex == request.reviews.size()){
            ImGui::CloseCurrentPopup();
            finishSavePreset();
        }
    }
    ImGui::EndPopup();
}

void ofxOceanodePresetsController::finishSavePreset(){
    if(!pendingSave) return;
    auto& request = *pendingSave;
    if(!request.writing && !request.reviews.empty()){
        const auto currentReviews = collapseEquivalentReviews(collectGlobalMacroSaveReviews(*container));
        bool stale = currentReviews.size() != request.reviews.size();
        if(!stale){
            for(size_t i = 0; i < currentReviews.size(); ++i){
                const auto& current = currentReviews[i];
                const auto& reviewed = request.reviews[i];
                if(current.instance != reviewed.instance || current.globalPath != reviewed.globalPath ||
                   current.instancePath != reviewed.instancePath || current.alsoUsedAt != reviewed.alsoUsedAt ||
                   !sameChanges(current, reviewed)) stale = true;
            }
        }
        if(stale){
            request.warning = "The macro changed during review. Please review the updated changes.";
            request.reviews = currentReviews;
            request.saveChoices.assign(request.reviews.size(), false);
            request.reviewIndex = 0;
            if(request.reviews.empty()){
                finishSavePreset();
                return;
            }
            popupRequest = PopupRequest::ReviewSave;
            return;
        }
    }

    request.writing = true;

    const size_t presetIndex = request.createNew
        ? bankPresets[request.bank].size()
        : std::distance(bankPresets[request.bank].begin(),
                        std::find(bankPresets[request.bank].begin(), bankPresets[request.bank].end(), request.name));
    const string presetPath = "./Presets/" + request.bank + "/" +
                              ofToString(presetIndex + 1) + "--" + request.name;

    for(size_t i = request.nextWriteIndex; i < request.reviews.size(); ++i){
        auto& review = request.reviews[i];
        if(!request.saveChoices[i]){
            request.skippedMacros.push_back(review.name);
            request.nextWriteIndex = i + 1;
            continue;
        }
        if(!review.instance->saveGlobalDefinition(false)){
            request.warning = "Could not finish saving this global macro. Retry, or stop without saving the preset. The macro folder may have been partly written.";
            request.reviewIndex = i;
            popupRequest = PopupRequest::ReviewSave;
            return;
        }
        request.savedMacros.push_back(review.name);
        request.nextWriteIndex = i + 1;
    }

    if(request.createNew){
        bankPresets[request.bank].push_back(request.name);
        newPresetCreated = true;
    }
    auto bankPosition = std::find(banks.begin(), banks.end(), request.bank);
    if(bankPosition != banks.end()) currentBank = std::distance(banks.begin(), bankPosition);
    currentPreset[request.bank] = request.name;
    ofxOceanodeShared::setCurrentPresetPath(presetPath);
    ofxOceanodeShared::setCurrentBankName(request.bank);
    ofxOceanodeShared::setCurrentPresetName(request.name);
    container->savePreset(presetPath);
    ofxOceanodeShared::presetWasSaved();

    if(!request.reviews.empty()){
        saveResultText = "Preset saved: " + request.bank + " / " + request.name + "\n";
        saveResultText += "Global macros saved: " + ofToString(request.savedMacros.size()) +
                          "\nGlobal macros skipped: " + ofToString(request.skippedMacros.size());
        for(const auto& name : request.savedMacros) saveResultText += "\n  Saved: " + name;
        for(const auto& name : request.skippedMacros) saveResultText += "\n  Skipped: " + name;
        popupRequest = PopupRequest::SaveResult;
    }
    pendingSave.reset();
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

// Global macro review compares live saveable state with the macro folder.
namespace {

ofJson readJson(const std::string& path){
    if(!ofFile::doesFileExist(path)) return ofJson();
    try { return ofLoadJson(path); }
    catch(const std::exception& e){
        ofLogWarning("GlobalMacroSaveReview") << "Could not read " << path << ": " << e.what();
        return ofJson();
    }
}

std::string canonicalMacroPath(const std::string& path){
    try { return std::filesystem::weakly_canonical(ofToDataPath(path, true)).string(); }
    catch(...) { return ofToDataPath(path, true); }
}

ofJson field(const ofJson& object, const std::string& key){
    if(object.is_object()){
        auto it = object.find(key);
        if(it != object.end()) return *it;
    }
    return ofJson();
}

std::string preview(const ofJson& value){
    if(value.is_null()) return "(none)";
    std::string result;
    try { result = value.is_string() ? value.get<std::string>() : value.dump(); }
    catch(...) { return "(value preview unavailable)"; }
    if(result.size() > 160){
        const std::string prefix = value.is_array() ? "Array (" + ofToString(value.size()) + ") " :
                                   value.is_object() ? "Object (" + ofToString(value.size()) + " fields) " : "";
        result = prefix + result.substr(0, 157) + "...";
    }
    return result;
}

std::string fullValue(const ofJson& value){
    if(value.is_null()) return "(none)";
    try { return value.dump(2); }
    catch(...) { return "(value preview unavailable)"; }
}

void addChange(std::vector<GlobalMacroSaveChange>& changes, const std::string& location,
               const std::string& item, const std::string& name,
               const ofJson& saved, const ofJson& current){
    if(saved == current) return;
    changes.push_back({location, item, name, preview(saved), preview(current),
                       fullValue(saved), fullValue(current)});
}

void compareJson(std::vector<GlobalMacroSaveChange>& changes, const std::string& location,
                 const std::string& item, const std::string& name,
                 const ofJson& saved, const ofJson& current, int depth = 0){
    if(saved == current) return;
    if(depth < 4 && saved.is_object() && current.is_object()){
        std::set<std::string> keys;
        for(auto it = saved.begin(); it != saved.end(); ++it) keys.insert(it.key());
        for(auto it = current.begin(); it != current.end(); ++it) keys.insert(it.key());
        for(const auto& key : keys){
            compareJson(changes, location, item, name + " / " + key,
                        field(saved, key), field(current, key), depth + 1);
        }
        return;
    }
    addChange(changes, location, item, name, saved, current);
}

std::string nodeKey(ofxOceanodeNode& node){
    auto& model = node.getNodeModel();
    std::string name = model.nodeName();
    ofStringReplace(name, " ", "_");
    return name + "_" + ofToString(model.getNumIdentifier());
}

std::string nodeLabel(ofxOceanodeNode& node){
    auto& model = node.getNodeModel();
    std::string label = model.nodeName() + " [" + ofToString(model.getNumIdentifier()) + "]";
    if(auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&model)){
        label += " " + (macro->isLocal() ? macro->getLocalMacroName() : macro->getCurrentMacroName());
    }
    return label;
}

std::string savedNodeKey(const std::string& type, const std::string& id){
    std::string name = type;
    ofStringReplace(name, " ", "_");
    return name + "_" + ofToString(ofToInt(id));
}

ofJson nodePosition(ofxOceanodeNode& node){
    const auto position = node.getNodeGui().getPosition();
    return {position.x, position.y};
}

std::set<std::string> currentConnections(ofxOceanodeContainer& container){
    std::set<std::string> result;
    for(const auto& connection : container.getAllConnections()){
        if(connection->getIsPersistent()) continue;
        auto& source = connection->getSourceParameter();
        auto& sink = connection->getSinkParameter();
        result.insert(source.getNodeModel()->getParameterGroup().getEscapedName() + "/" +
                      source.getName() + " -> " +
                      sink.getNodeModel()->getParameterGroup().getEscapedName() + "/" + sink.getName());
    }
    return result;
}

std::set<std::string> savedConnections(const ofJson& json){
    std::set<std::string> result;
    if(!json.is_object()) return result;
    for(auto sourceNode = json.begin(); sourceNode != json.end(); ++sourceNode){
        if(!sourceNode.value().is_object()) continue;
        for(auto sourceField = sourceNode.value().begin(); sourceField != sourceNode.value().end(); ++sourceField){
            if(!sourceField.value().is_object()) continue;
            for(auto sinkNode = sourceField.value().begin(); sinkNode != sourceField.value().end(); ++sinkNode){
                if(!sinkNode.value().is_object()) continue;
                for(auto sinkField = sinkNode.value().begin(); sinkField != sinkNode.value().end(); ++sinkField){
                    result.insert(sourceNode.key() + "/" + sourceField.key() + " -> " +
                                  sinkNode.key() + "/" + sinkField.key());
                }
            }
        }
    }
    return result;
}

ofJson currentComments(ofxOceanodeContainer& container){
    ofJson json;
    auto& comments = container.getComments();
    json["NumComments"] = comments.size();
    for(size_t i = 0; i < comments.size(); ++i){
        auto& c = comments[i];
        auto& entry = json["Comments"][i];
        entry["Text"] = c.text;
        entry["Size"]["X"] = c.size.x;
        entry["Size"]["Y"] = c.size.y;
        entry["Pos"]["X"] = c.position.x;
        entry["Pos"]["Y"] = c.position.y;
        entry["Color"]["R"] = c.color.r;
        entry["Color"]["G"] = c.color.g;
        entry["Color"]["B"] = c.color.b;
        entry["TextColor"]["R"] = c.textColor.r;
        entry["TextColor"]["G"] = c.textColor.g;
        entry["TextColor"]["B"] = c.textColor.b;
    }
    return json;
}

ofJson normalizedCustomGuis(ofJson json){
    auto panels = field(json, "panels");
    if(!panels.is_array()) return ofJson::array();
    for(auto& panel : panels){
        if(!panel.is_object()) continue;
        panel.erase("windowState");
        panel.erase("designMode");
        if(panel.contains("layout") && panel["layout"].is_object()) panel["layout"].erase("zoom");
    }
    return panels;
}

ofJson normalizedCustomGuiSnapshots(ofJson json){
    auto banks = field(json, "banks");
    if(!banks.is_array()) return ofJson::array();
    for(auto& bank : banks){
        if(bank.is_object()) bank.erase("currentSnapshotId");
    }
    return banks;
}

void compareContainer(ofxOceanodeContainer& container, const std::string& savedFolder,
                      const std::string& location, std::vector<GlobalMacroSaveChange>& changes);

void compareNode(ofxOceanodeContainer& owner, ofxOceanodeNode& node,
                 const std::string& savedFolder, const std::string& location,
                 std::vector<GlobalMacroSaveChange>& changes){
    auto& model = node.getNodeModel();
    const std::string label = nodeLabel(node);
    const ofJson saved = readJson(savedFolder + "/" + nodeKey(node) + ".json");
    ofJson parameters = node.saveParametersToJson(false);
    ofJson inspector;
    node.saveInspectorParametersToJson(inspector);

    if(parameters.is_object()){
        for(auto it = parameters.begin(); it != parameters.end(); ++it){
            if(owner.wasValueUserEdited(node, it.key(), it.value()))
                addChange(changes, location, label, it.key(), field(saved, it.key()), it.value());
        }
    }
    if(inspector.is_object()){
        for(auto it = inspector.begin(); it != inspector.end(); ++it){
            if(owner.wasValueUserEdited(node, it.key(), it.value(), true))
                addChange(changes, location, label, "Inspector / " + it.key(),
                          field(saved, it.key()), it.value());
        }
    }

    ofJson modelData;
    model.presetSave(modelData);
    if(modelData.is_object()){
        for(auto it = modelData.begin(); it != modelData.end(); ++it){
            if(parameters.contains(it.key()) || inspector.contains(it.key())) continue;
            if(!model.shouldReviewPresetSaveField(it.key(), it.value())) continue;
            // These presetSave fields describe running output, external window
            // placement, or the current snapshot choice, not macro edits.
            if(it.key() == "AllCurvesOutput" || it.key() == "ExtWindowRect" ||
               it.key() == "ExtWindowMode" || it.key() == "SelectedSnapshotSlot") continue;
            compareJson(changes, location, label, it.key(), field(saved, it.key()), it.value());
        }
    }

    if(auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&model)){
        const ofJson local = macro->isLocal();
        const bool savedWasLocal = saved.is_object() ? saved.value("LocalPreset", true) : true;
        const ofJson savedLocal = savedWasLocal;
        const bool referenceUserEdited = macro->wasMacroReferenceUserEdited();
        if(saved.is_object() && savedWasLocal && !macro->isLocal()){
            if(!referenceUserEdited) return;
            const std::string localName = saved.value("Local_Name", std::string("Local"));
            const std::string macroName = macro->getCurrentMacroName();
            std::string categoryName;
            for(const auto& part : macro->getCurrentMacroCategory()){
                if(!categoryName.empty()) categoryName += " / ";
                categoryName += part;
            }
            const std::string oldLabel = "Local Macro \"" + localName + "\"";
            const std::string newLabel = "Global Macro \"" + macroName + "\"" +
                (categoryName.empty() ? "" : " (" + categoryName + ")");
            const ofJson oldIdentity = {{"LocalPreset", true}, {"Local_Name", localName}};
            const ofJson newIdentity = {{"LocalPreset", false}, {"Macro", macroName},
                                        {"CategoryStruct", macro->getCurrentMacroCategory()},
                                        {"MacroPath", macro->getCurrentMacroPath()}};
            changes.push_back({location,
                               model.nodeName() + " [" + ofToString(model.getNumIdentifier()) + "]",
                               "Macro replacement", oldLabel, newLabel,
                               fullValue(oldIdentity), fullValue(newIdentity),
                               GlobalMacroSaveChange::Kind::MacroReplacement});
            return;
        }
        if(referenceUserEdited) addChange(changes, location, label, "Local macro", savedLocal, local);
        if(macro->isLocal()){
            const ofJson savedOrder = field(saved, "RouterSortOrder");
            const ofJson currentOrder = macro->getRouterSortOrder();
            if(macro->wasRouterOrderUserEdited() &&
               !(savedOrder.is_null() && currentOrder.empty())){
                addChange(changes, location, label, "Router order", savedOrder, currentOrder);
            }
            if(savedWasLocal || referenceUserEdited){
                const std::string childFolder = savedFolder + "/" + nodeKey(node);
                // The local child is part of the owning global macro's definition.
                compareContainer(*macro->getContainer(), childFolder,
                                 location + " > " + label, changes);
            }
        }else{
            // MacroPath is resolved during loading and may change between
            // missing, relative, and absolute forms without a user edit.
            const ofJson oldMacro = field(saved, "Macro");
            const ofJson currentMacro = macro->getCurrentMacroName();
            if(referenceUserEdited) addChange(changes, location, label, "Referenced macro", oldMacro, currentMacro);
            const ofJson category = macro->getCurrentMacroCategory();
            const ofJson oldCategory = field(saved, "CategoryStruct");
            if(referenceUserEdited) addChange(changes, location, label, "Macro category", oldCategory, category);
        }
    }
}

void compareContainer(ofxOceanodeContainer& container, const std::string& savedFolder,
                      const std::string& location, std::vector<GlobalMacroSaveChange>& changes){
    const ofJson modules = readJson(savedFolder + "/modules.json");
    std::map<std::string, ofxOceanodeNode*> liveNodes;
    for(auto* node : container.getAllModules()){
        const std::string key = nodeKey(*node);
        liveNodes[key] = node;
        auto& model = node->getNodeModel();
        const std::string type = model.nodeName();
        const std::string id = node->getIsPersistent()
            ? ofToString(model.getNumIdentifier())
            : ofToString(model.getNumIdentifier(), 2, '0');
        const ofJson savedPosition = field(field(modules, type), id);
        if(savedPosition.is_null()){
            changes.push_back({location, nodeLabel(*node), "Node added", "(absent)", "present"});
            continue;
        }
        if(!savedPosition.is_array() || savedPosition.size() < 2){
            changes.push_back({location, nodeLabel(*node), "Saved position", preview(savedPosition), preview(nodePosition(*node))});
        }else{
            const ofJson savedXY = {savedPosition[0], savedPosition[1]};
            addChange(changes, location, nodeLabel(*node), "Position", savedXY, nodePosition(*node));
        }
        compareNode(container, *node, savedFolder, location, changes);
    }
    if(modules.is_object()){
        for(auto type = modules.begin(); type != modules.end(); ++type){
            if(!type.value().is_object()) continue;
            for(auto id = type.value().begin(); id != type.value().end(); ++id){
                if(liveNodes.count(savedNodeKey(type.key(), id.key())) == 0){
                    changes.push_back({location, type.key() + " [" + ofToString(ofToInt(id.key())) + "]",
                                       "Node removed", "present", "(absent)"});
                }
            }
        }
    }

    const auto savedLinks = savedConnections(readJson(savedFolder + "/connections.json"));
    const auto liveLinks = currentConnections(container);
    for(const auto& link : liveLinks){
        if(savedLinks.count(link) == 0) changes.push_back({location, link, "Connection added", "(absent)", "present"});
    }
    for(const auto& link : savedLinks){
        if(liveLinks.count(link) == 0) changes.push_back({location, link, "Connection removed", "present", "(absent)"});
    }

    compareJson(changes, location, "Comments", "Comments",
                readJson(savedFolder + "/comments.json"), currentComments(container));
    compareJson(changes, location, "Custom GUI", "Panels",
                normalizedCustomGuis(readJson(savedFolder + "/custom_guis.json")),
                normalizedCustomGuis(container.getCustomGuisJsonForReview()));
    compareJson(changes, location, "Custom GUI snapshots", "Snapshots",
                normalizedCustomGuiSnapshots(readJson(savedFolder + "/custom_gui_snapshots.json")),
                normalizedCustomGuiSnapshots(container.getCustomGuiSnapshotsJsonForReview()));
#ifdef OFXOCEANODE_USE_MIDI
    ofJson savedMidi = readJson(savedFolder + "/midi.json");
    if(savedMidi.is_null()) savedMidi = ofJson::object();
    compareJson(changes, location, "MIDI bindings", "Bindings",
                savedMidi, container.getMidiBindingsJsonForReview());
#endif
}

void visitMacros(ofxOceanodeContainer& container, const std::string& breadcrumb,
                 std::set<ofxOceanodeNodeMacro*>& visited,
                 std::vector<GlobalMacroSaveReview>& reviews){
    for(auto* node : container.getAllModules()){
        auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel());
        if(macro == nullptr || !visited.insert(macro).second) continue;
        const std::string instancePath = breadcrumb + " > " + nodeLabel(*node);
        if(!macro->isLocal() && !macro->getCurrentMacroPath().empty()){
            GlobalMacroSaveReview review;
            review.instance = macro;
            review.name = macro->getCurrentMacroName();
            review.globalPath = canonicalMacroPath(macro->getCurrentMacroPath());
            review.instancePath = instancePath;
            compareContainer(*macro->getContainer(), review.globalPath, instancePath, review.changes);
            const ofJson savedSort = readJson(review.globalPath + "/router_sort_order.json");
            const ofJson currentSort = macro->getRouterSortOrder();
            const ofJson oldOrder = field(savedSort, "RouterSortOrder");
            if(macro->wasRouterOrderUserEdited() && !(oldOrder.is_null() && currentSort.empty()))
                addChange(review.changes, instancePath, "Macro interface", "Router order", oldOrder, currentSort);
            if(!review.changes.empty()) reviews.push_back(std::move(review));
        }
        visitMacros(*macro->getContainer(), instancePath, visited, reviews);
    }
}

} // namespace

std::vector<GlobalMacroSaveReview> collectGlobalMacroSaveReviews(ofxOceanodeContainer& root){
    std::vector<GlobalMacroSaveReview> reviews;
    std::set<ofxOceanodeNodeMacro*> visited;
    visitMacros(root, "Canvas", visited, reviews);
    std::sort(reviews.begin(), reviews.end(), [](const auto& a, const auto& b){
        return a.instancePath < b.instancePath;
    });
    return reviews;
}
