#include "customGuiSnapshots.h"

#ifndef OFXOCEANODE_HEADLESS

#include "Managers/ofxOceanodeContainer.h"
#include "Nodes/ofxOceanodeNodeMacro.h"
#include "ofxOceanodeColors.h"
#include "imgui.h"
#include <algorithm>

customGuiSnapshots::customGuiSnapshots()
: ofxOceanodeNodeModel("CustomGui Snapshots")
{
}

void customGuiSnapshots::setup()
{
    color = ofColor(120, 210, 120);
    description = "Recall and store snapshots for published Custom GUIs";

    customGuiIndex.set("Custom GUI", -1, -1, -1);
    snapshotSlot.set("Snapshot", -1, -1, 0);
    showWindow.set("Show", false);
    matrixRows.set("Snapshot Rows", 2, 1, 8);
    matrixCols.set("Snapshot Cols", 8, 1, 8);
    showSnapshotNames.set("Show Names", true);

    customGuiIndexParam = addParameter(customGuiIndex, ofxOceanodeParameterFlags_DisableSavePreset);
    snapshotSlotParam = addParameter(snapshotSlot, ofxOceanodeParameterFlags_DisableSavePreset);
    addParameter(showWindow);

    addInspectorParameter(matrixRows);
    addInspectorParameter(matrixCols);
    addInspectorParameter(showSnapshotNames);

    snapshotMatrixRegion.set("Snapshot Matrix", [this]() {
        renderMatrix(false);
    });
    snapshotMatrixRegionParam = addCustomRegion(snapshotMatrixRegion, [this]() {
        renderMatrix(false);
    });
    snapshotMatrixRegionParam->setFlags(snapshotMatrixRegionParam->getFlags() | ofxOceanodeParameterFlags_NoGuiWidget);

    listeners.push(customGuiIndex.newListener([this](int&) {
        if(suppressTargetListener) return;
        if(customGuiIndex >= 0 && customGuiIndex < (int)availableTargets.size()) {
            selectedCanvasId = availableTargets[customGuiIndex].canvasId;
            selectedPanelId = availableTargets[customGuiIndex].panelId;
        } else {
            selectedCanvasId.clear();
            selectedPanelId.clear();
        }
        refreshSnapshotOptions();
    }));

    listeners.push(snapshotSlot.newListener([this](int& slot) {
        if(suppressSnapshotListener) return;
        if(slot < 0) return;
        recallSlot(slot);
    }));
}

void customGuiSnapshots::setContainer(ofxOceanodeContainer* container)
{
    ofxOceanodeNodeModel::setContainer(container);
    ownerContainer = container;
}

void customGuiSnapshots::presetSave(ofJson& json)
{
    json["SelectedCanvasId"] = selectedCanvasId;
    json["SelectedPanelId"] = selectedPanelId;
    json["SelectedSnapshotSlot"] = snapshotSlot.get();
}

void customGuiSnapshots::presetRecallBeforeSettingParameters(ofJson& json)
{
    if(json.contains("SelectedCanvasId")) selectedCanvasId = json["SelectedCanvasId"].get<std::string>();
    if(json.contains("SelectedPanelId")) selectedPanelId = json["SelectedPanelId"].get<std::string>();
    if(json.contains("SelectedSnapshotSlot")) {
        suppressSnapshotListener = true;
        snapshotSlot = json["SelectedSnapshotSlot"].get<int>();
        suppressSnapshotListener = false;
    }
}

void customGuiSnapshots::update(ofEventArgs& a)
{
    refreshTargets();
}

void customGuiSnapshots::draw(ofEventArgs& a)
{
    if(!showWindow) return;

    std::string windowTitle = "CustomGui Snapshots " + ofToString(getNumIdentifier());
    if(!canvasID.empty() && canvasID != "Canvas" && canvasID != "0") {
        windowTitle = canvasID + " / " + windowTitle;
    }

    if(ImGui::Begin(windowTitle.c_str(), (bool*)&showWindow.get())) {
        renderMatrix(true);
    }
    ImGui::End();
}

void customGuiSnapshots::refreshTargets()
{
    if(ownerContainer == nullptr) return;

    std::vector<TargetRef> newTargets;
    collectTargets(ownerContainer, newTargets);
    std::sort(newTargets.begin(), newTargets.end(), [](const TargetRef& a, const TargetRef& b) {
        return a.label < b.label;
    });

    bool changed = newTargets.size() != availableTargets.size();
    if(!changed) {
        for(size_t i = 0; i < newTargets.size(); i++) {
            if(newTargets[i].canvasId != availableTargets[i].canvasId ||
               newTargets[i].panelId != availableTargets[i].panelId ||
               newTargets[i].label != availableTargets[i].label) {
                changed = true;
                break;
            }
        }
    }

    availableTargets = std::move(newTargets);

    std::vector<std::string> options;
    options.reserve(availableTargets.size());
    for(const auto& target : availableTargets) options.push_back(target.label);
    customGuiIndexParam->cast<int>().setDropdownOptions(options);

    syncSelectionFromStoredIds();
    refreshSnapshotOptions();
}

void customGuiSnapshots::syncSelectionFromStoredIds()
{
    int resolvedIndex = -1;
    for(size_t i = 0; i < availableTargets.size(); i++) {
        if(availableTargets[i].canvasId == selectedCanvasId && availableTargets[i].panelId == selectedPanelId) {
            resolvedIndex = (int)i;
            break;
        }
    }

    if(resolvedIndex < 0 && !availableTargets.empty()) {
        resolvedIndex = 0;
        selectedCanvasId = availableTargets[0].canvasId;
        selectedPanelId = availableTargets[0].panelId;
    } else if(resolvedIndex < 0) {
        selectedCanvasId.clear();
        selectedPanelId.clear();
    }

    suppressTargetListener = true;
    customGuiIndex.setMin(availableTargets.empty() ? -1 : 0);
    customGuiIndex.setMax(availableTargets.empty() ? -1 : (int)availableTargets.size() - 1);
    customGuiIndex = resolvedIndex;
    suppressTargetListener = false;
}

void customGuiSnapshots::refreshSnapshotOptions()
{
    std::vector<std::string> options;
    int maxSlot = matrixRows * matrixCols - 1;

    ofxOceanodeContainer* targetContainer = getSelectedTargetContainer();
    const TargetRef* target = getSelectedTarget();
    const CustomGuiSnapshotBank* bank = (targetContainer != nullptr && target != nullptr)
        ? targetContainer->getCustomGuiSnapshotBank(target->panelId)
        : nullptr;

    if(bank != nullptr) {
        for(const auto& snapshot : bank->snapshots) {
            maxSlot = std::max(maxSlot, snapshot.slot);
        }
    }
    maxSlot = std::max(maxSlot, 0);

    options.resize(maxSlot + 1);
    for(int slot = 0; slot <= maxSlot; slot++) {
        std::string label = ofToString(slot + 1);
        const CustomGuiSnapshotData* snapshot = (targetContainer != nullptr && target != nullptr)
            ? targetContainer->getCustomGuiSnapshotBySlot(target->panelId, slot)
            : nullptr;
        if(snapshot != nullptr) label += ": " + snapshot->name;
        else label += ": -";
        options[slot] = label;
    }
    snapshotSlotParam->cast<int>().setDropdownOptions(options);

    int desiredSlot = snapshotSlot.get();
    if(bank != nullptr && !bank->currentSnapshotId.empty()) {
        for(const auto& snapshot : bank->snapshots) {
            if(snapshot.id == bank->currentSnapshotId) {
                desiredSlot = snapshot.slot;
                break;
            }
        }
    } else if(desiredSlot > maxSlot) {
        desiredSlot = -1;
    }

    suppressSnapshotListener = true;
    snapshotSlot.setMin(-1);
    snapshotSlot.setMax(maxSlot);
    if(desiredSlot > maxSlot) desiredSlot = -1;
    snapshotSlot = desiredSlot;
    suppressSnapshotListener = false;
}

void customGuiSnapshots::renderMatrix(bool withControls)
{
    if(ownerContainer == nullptr) {
        ImGui::TextDisabled("No container");
        return;
    }

    refreshTargets();

    ofxOceanodeContainer* targetContainer = getSelectedTargetContainer();
    const TargetRef* target = getSelectedTarget();
    if(target == nullptr || targetContainer == nullptr) {
        ImGui::TextDisabled("No Custom GUI available");
        return;
    }

    if(withControls) {
        if(ImGui::BeginCombo("Custom GUI", target->label.c_str())) {
            for(size_t i = 0; i < availableTargets.size(); i++) {
                const bool isSelected = (int)i == customGuiIndex.get();
                if(ImGui::Selectable(availableTargets[i].label.c_str(), isSelected)) {
                    suppressTargetListener = true;
                    customGuiIndex = (int)i;
                    suppressTargetListener = false;
                    selectedCanvasId = availableTargets[i].canvasId;
                    selectedPanelId = availableTargets[i].panelId;
                    refreshSnapshotOptions();
                    targetContainer = getSelectedTargetContainer();
                    target = getSelectedTarget();
                }
                if(isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        std::string snapshotLabel = "None";
        if(const CustomGuiSnapshotData* current = targetContainer->getCustomGuiSnapshotBySlot(target->panelId, snapshotSlot.get())) {
            snapshotLabel = current->name;
        }
        if(ImGui::BeginCombo("Snapshot", snapshotLabel.c_str())) {
            const int maxSlot = std::max(matrixRows * matrixCols - 1, snapshotSlot.get());
            for(int slot = 0; slot <= maxSlot; slot++) {
                const CustomGuiSnapshotData* snapshot = targetContainer->getCustomGuiSnapshotBySlot(target->panelId, slot);
                std::string label = snapshot != nullptr ? snapshot->name : ("Slot " + ofToString(slot + 1));
                bool isSelected = snapshotSlot.get() == slot;
                if(ImGui::Selectable(label.c_str(), isSelected)) {
                    suppressSnapshotListener = true;
                    snapshotSlot = slot;
                    suppressSnapshotListener = false;
                    recallSlot(slot);
                }
                if(isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::Separator();
    }

    const int rows = std::max(1, matrixRows.get());
    const int cols = std::max(1, matrixCols.get());
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float totalWidth = std::max(1.0f, avail.x);
    float btnW = std::max(18.0f, (totalWidth - spacing * (cols - 1)) / cols);
    float btnH = btnW;

    const CustomGuiSnapshotBank* bank = targetContainer->getCustomGuiSnapshotBank(target->panelId);
    const int currentSlot = getCurrentSnapshotSlot();

    ImGui::PushID("CustomGuiSnapshotMatrix");
    for(int row = 0; row < rows; row++) {
        for(int col = 0; col < cols; col++) {
            if(col > 0) ImGui::SameLine();
            const int slot = row * cols + col;
            const CustomGuiSnapshotData* snapshot = targetContainer->getCustomGuiSnapshotBySlot(target->panelId, slot);
            const bool hasData = snapshot != nullptr;
            const bool isActive = hasData && slot == currentSlot;

            if(isActive) {
                ImGui::PushStyleColor(ImGuiCol_Button, OceanodeColors::SnapshotActive);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, OceanodeColors::SnapshotActiveHovered);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, OceanodeColors::SnapshotActivePressed);
            } else if(hasData) {
                ImGui::PushStyleColor(ImGuiCol_Button, OceanodeColors::SnapshotFilled);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, OceanodeColors::SnapshotFilledHovered);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, OceanodeColors::SnapshotFilledPressed);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Button));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            }

            std::string label;
            if(hasData && showSnapshotNames.get()) label = snapshot->name;
            else label = ofToString(slot + 1);

            if(ImGui::Button(label.c_str(), ImVec2(btnW, btnH))) {
                if(ImGui::GetIO().KeyShift) storeSlot(slot);
                else recallSlot(slot);
            }
            ImGui::PopStyleColor(3);
        }
    }
    ImGui::PopID();
}

void customGuiSnapshots::storeSlot(int slot)
{
    ofxOceanodeContainer* targetContainer = getSelectedTargetContainer();
    const TargetRef* target = getSelectedTarget();
    if(targetContainer == nullptr || target == nullptr || slot < 0) return;

    std::string snapshotId = targetContainer->storeCustomGuiSnapshotToSlot(target->panelId, slot);
    if(snapshotId.empty()) return;

    suppressSnapshotListener = true;
    snapshotSlot = slot;
    suppressSnapshotListener = false;
    selectedCanvasId = target->canvasId;
    selectedPanelId = target->panelId;
    refreshSnapshotOptions();
}

void customGuiSnapshots::recallSlot(int slot)
{
    ofxOceanodeContainer* targetContainer = getSelectedTargetContainer();
    const TargetRef* target = getSelectedTarget();
    if(targetContainer == nullptr || target == nullptr || slot < 0) return;

    if(targetContainer->recallCustomGuiSnapshotSlot(target->panelId, slot)) {
        suppressSnapshotListener = true;
        snapshotSlot = slot;
        suppressSnapshotListener = false;
        refreshSnapshotOptions();
    }
}

int customGuiSnapshots::getCurrentSnapshotSlot() const
{
    ofxOceanodeContainer* targetContainer = getSelectedTargetContainer();
    const TargetRef* target = getSelectedTarget();
    if(targetContainer == nullptr || target == nullptr) return -1;

    const CustomGuiSnapshotBank* bank = targetContainer->getCustomGuiSnapshotBank(target->panelId);
    if(bank == nullptr) return -1;
    for(const auto& snapshot : bank->snapshots) {
        if(snapshot.id == bank->currentSnapshotId) return snapshot.slot;
    }
    return -1;
}

const customGuiSnapshots::TargetRef* customGuiSnapshots::getSelectedTarget() const
{
    for(const auto& target : availableTargets) {
        if(target.canvasId == selectedCanvasId && target.panelId == selectedPanelId) return &target;
    }
    return nullptr;
}

ofxOceanodeContainer* customGuiSnapshots::getSelectedTargetContainer() const
{
    if(ownerContainer == nullptr || selectedPanelId.empty()) return nullptr;
    return findContainerForCanvasId(ownerContainer, selectedCanvasId);
}

ofxOceanodeContainer* customGuiSnapshots::findContainerForCanvasId(ofxOceanodeContainer* root, const std::string& canvasId) const
{
    if(root == nullptr) return nullptr;
    if(root->getCanvasID() == canvasId) return root;

    for(auto* node : root->getAllModules()) {
        if(node == nullptr) continue;
        if(ofxOceanodeNodeMacro* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel())) {
            ofxOceanodeContainer* found = findContainerForCanvasId(macro->getContainer().get(), canvasId);
            if(found != nullptr) return found;
        }
    }
    return nullptr;
}

void customGuiSnapshots::collectTargets(ofxOceanodeContainer* root, std::vector<TargetRef>& out) const
{
    if(root == nullptr) return;

    const std::string rootCanvasId = root->getCanvasID();
    for(const auto& panel : root->getCustomGuiPanelsData()) {
        if(!root->customGuiPanelHasSnapshotEligibleParameters(panel.id)) continue;

        TargetRef target;
        target.canvasId = rootCanvasId;
        target.panelId = panel.id;
        target.panelName = panel.name;
        target.label = panel.name;
        if(rootCanvasId != "Canvas" && rootCanvasId != "0" && !rootCanvasId.empty()) {
            target.label += " [" + rootCanvasId + "]";
        }
        out.push_back(std::move(target));
    }

    for(auto* node : root->getAllModules()) {
        if(node == nullptr) continue;
        if(ofxOceanodeNodeMacro* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel())) {
            collectTargets(macro->getContainer().get(), out);
        }
    }
}

#endif
