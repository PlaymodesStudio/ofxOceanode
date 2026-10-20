#pragma once

#include "ofMain.h"
#include "ofxOceanodeShared.h"
#include "ofxOceanodeMenu.h"
#include "PresetSaveCleanup.h"
#include "ofxOceanodePresetLayouts.h"
#include <cstdio>
#include <cstring>
#include <functional>
#include <utility>

// One owner for both global and preset layouts: selection, persistence, defaults,
// deferred application and UI. Header-only so existing OF/Xcode projects do not
// need a new compilation unit. The host supplies only window and docking adapters.
class ofxOceanodeLayoutManager {
public:
    struct Callbacks {
        std::function<ofJson()> captureWindows;
        std::function<void(const ofJson&)> restoreWindows;
        std::function<void(const std::string&)> applyDocking;
        std::function<void(const std::string&)> setRootCanvasPath;
    };
    explicit ofxOceanodeLayoutManager(Callbacks adapters): callbacks(std::move(adapters)) {}

    void onPresetLoaded();
    void onPresetSaved();
    void loadDefaultGlobalLayout();
    void beforeFrame(); // Call before ImGui::NewFrame.
    void saveActiveCanvasLayout();
    void drawMenu();
    void drawPopups(){ drawPresetLayoutPopup(); drawGlobalLayoutPopup(); }

private:
    Callbacks callbacks;

    // Named snapshots belong to the current preset.
    ofxOceanodePresetLayouts presetLayouts;
    std::string pendingPresetLayout;
    std::string layoutError;
    bool presetReadFailed = false;
    enum class PresetLayoutPopup { None, Save, Rename, Delete };
    PresetLayoutPopup presetPopup = PresetLayoutPopup::None;
    std::string presetTarget;
    char presetName[256] = {};

    // Global layouts retain the existing .ini files and startup preference.
    bool openSaveGlobalLayoutPopup = false;
    std::string globalLayoutName;
    char globalName[256] = {};

    // Own the current canvas layout and requests applied before NewFrame.
    bool hasActivePreset = false;
    std::string rootLayoutPath;
    std::string pendingIniLoad;

    ofJson capturePresetLayout();
    void recallPresetLayout(const std::string& name);
    bool savePresetLayouts();
    void readPresetLayouts();
    void queueGlobalLayout(const std::string& path, const std::string& name);
    void drawPresetLayoutsMenu();
    void drawPresetLayoutPopup();
    void drawGlobalLayoutPopup();
};

// Preset lifecycle and frame-boundary application.

inline void ofxOceanodeLayoutManager::onPresetLoaded(){
    readPresetLayouts();
    if(ofxOceanodeShared::getCurrentPresetPath().empty()){
        pendingIniLoad.clear();
        ofxOceanodeShared::getPendingLayoutSavePath().clear();
        ofxOceanodeShared::getPendingLayoutLoadPath().clear();
        ofxOceanodeShared::getActiveCanvasLayoutPath().clear();
        callbacks.setRootCanvasPath("");
        rootLayoutPath.clear();
        hasActivePreset = false;
        return;
    }
    std::string iniPath = ofToDataPath(ofxOceanodeShared::getCurrentPresetPath() + "/ImGuiLayout.ini");
    if(ofFile(iniPath).exists()){
        pendingIniLoad = iniPath;   // defer — will be applied before next NewFrame
        // Cache the content for preservation during future saves
        ofBuffer buf = ofBufferFromFile(iniPath);
        ofxOceanodeShared::getLayoutContentCache()[iniPath] = buf.getText();
    }
    // Set main canvas layout path
    callbacks.setRootCanvasPath(ofxOceanodeShared::getCurrentPresetPath() + "/ImGuiLayout.ini");
    ofxOceanodeShared::getActiveCanvasLayoutPath() = iniPath;
    rootLayoutPath = iniPath;
    hasActivePreset = true;
}

inline void ofxOceanodeLayoutManager::onPresetSaved(){
    std::string iniPath = ofToDataPath(ofxOceanodeShared::getCurrentPresetPath() + "/ImGuiLayout.ini");
    std::string& activeLayoutPath = ofxOceanodeShared::getActiveCanvasLayoutPath();
    // Save As: the preset's path is the new folder now, but the active
    // layout path still names the root layout of the preset saved / loaded
    // before. That is still the root canvas (not a macro's).
    const std::string previousRoot = rootLayoutPath;
    const bool rootActive = activeLayoutPath.empty() || activeLayoutPath == iniPath ||
                            (!previousRoot.empty() && activeLayoutPath == previousRoot);
    rootLayoutPath = iniPath;

    // Only save from ImGui state if the root canvas is active
    // (activeLayoutPath matches the preset's layout path).
    if(rootActive){
        activeLayoutPath = iniPath;
        ImGui::SaveIniSettingsToDisk(iniPath.c_str());
        // Update cache
        size_t sz = 0;
        const char* d = ImGui::SaveIniSettingsToMemory(&sz);
        if(d && sz > 0) ofxOceanodeShared::getLayoutContentCache()[iniPath] = std::string(d, sz);
    }
    // If a macro is active, save ImGui state to the macro's active path instead.
    // IMPORTANT: Read the root layout from disk FIRST — ImGui's in-memory state
    // currently holds the macro layout (loaded when the user focused the macro canvas),
    // and SaveIniSettingsToDisk writes that macro layout wherever we tell it.
    // The root layout file on disk is still correct (container->savePreset() never
    // touches ImGuiLayout.ini), so we snapshot it before any ImGui write, then
    // put it back afterwards to guarantee it isn't lost.
    else if(!activeLayoutPath.empty()){
        // Snapshot the root layout file from disk before any ImGui write.
        // Save As: the new folder has none yet, take the previous preset's.
        std::string rootLayoutContent;
        if(ofFile(iniPath).exists()){
            ofBuffer buf = ofBufferFromFile(iniPath);
            rootLayoutContent = buf.getText();
        }else if(!previousRoot.empty()){
            auto& cache = ofxOceanodeShared::getLayoutContentCache();
            auto cached = cache.find(previousRoot);
            if(cached != cache.end() && !cached->second.empty()) rootLayoutContent = cached->second;
            else if(ofFile(previousRoot).exists()) rootLayoutContent = ofBufferFromFile(previousRoot).getText();
        }

        // Save ImGui state (which is the macro's layout) to the macro's path
        ImGui::SaveIniSettingsToDisk(activeLayoutPath.c_str());

        // Restore the root layout from the disk snapshot we took above
        if(!rootLayoutContent.empty()){
            ofBuffer buf;
            buf.set(rootLayoutContent.c_str(), rootLayoutContent.size());
            ofBufferToFile(iniPath, buf);
            // Also update the in-memory cache to keep it in sync
            ofxOceanodeShared::getLayoutContentCache()[iniPath] = rootLayoutContent;
            ofLogNotice("ofxOceanode") << "Preserved root layout at " << iniPath;
        }
    }

    callbacks.setRootCanvasPath(ofxOceanodeShared::getCurrentPresetPath() + "/ImGuiLayout.ini");
    hasActivePreset = true;
    savePresetLayouts();
}

inline void ofxOceanodeLayoutManager::beforeFrame(){
    if(!pendingIniLoad.empty()){
        callbacks.applyDocking(ofBufferFromFile(pendingIniLoad).getText());
        pendingIniLoad.clear();
    }
    // Deferred canvas layout switching (save previous, load new)
    {
        std::string& pendingSave = ofxOceanodeShared::getPendingLayoutSavePath();
        std::string& pendingLoad = ofxOceanodeShared::getPendingLayoutLoadPath();

        if(!pendingLoad.empty()){
            // Save current ImGui state to the previous canvas's file
            if(!pendingSave.empty()){
                ImGui::SaveIniSettingsToDisk(pendingSave.c_str());
                // Cache the saved content in memory for preservation during preset save
                size_t iniSize = 0;
                const char* iniData = ImGui::SaveIniSettingsToMemory(&iniSize);
                if(iniData && iniSize > 0){
                    ofxOceanodeShared::getLayoutContentCache()[pendingSave] = std::string(iniData, iniSize);
                }
                pendingSave.clear();
            }

            // Load the new canvas's layout
            if(ofFile(pendingLoad).exists()){
                callbacks.applyDocking(ofBufferFromFile(pendingLoad).getText());
            }
            pendingLoad.clear();
        }
    }
    if(!pendingPresetLayout.empty()){
        const auto name = pendingPresetLayout;
        pendingPresetLayout.clear();
        recallPresetLayout(name);
    }
}

inline void ofxOceanodeLayoutManager::saveActiveCanvasLayout(){
    if(hasActivePreset){
        std::string iniPath = ofToDataPath(ofxOceanodeShared::getCurrentPresetPath() + "/ImGuiLayout.ini");
        std::string& activeLayout = ofxOceanodeShared::getActiveCanvasLayoutPath();

        // Save ImGui state to the currently active canvas layout path.
        // If root is active, activeLayout == iniPath so root gets saved.
        // If a macro is active, save to the macro's path — the root layout
        // on disk is already correct from the last focus-change save.
        if(!activeLayout.empty()){
            ImGui::SaveIniSettingsToDisk(activeLayout.c_str());
        } else {
            // Fallback: no active layout tracked, save to root preset path
            ImGui::SaveIniSettingsToDisk(iniPath.c_str());
        }
    }
}

// Preset snapshots and compatibility storage.

inline ofJson ofxOceanodeLayoutManager::capturePresetLayout(){
    ofJson state = callbacks.captureWindows();
    size_t size = 0;
    const char* ini = ImGui::SaveIniSettingsToMemory(&size);
    state["layout"] = std::string(ini, size);
    return state;
}

inline bool ofxOceanodeLayoutManager::savePresetLayouts(){
    if(presetReadFailed){
        layoutError = "Cannot save preset layouts until the invalid layout file is fixed and the preset reloaded.";
        return false;
    }
    const auto preset = ofxOceanodeShared::getCurrentPresetPath();
    // An unsaved patch can have preset layouts; its first Save writes them all.
    if(preset.empty()) return true;
    if(oceanodePresetSave::saveJson(preset + "/preset_layouts.json", presetLayouts.toJson())){
        layoutError.clear();
        return true;
    }
    layoutError = "Could not save preset_layouts.json. Check the preset folder is writable.";
    return false;
}

inline void ofxOceanodeLayoutManager::readPresetLayouts(){
    pendingIniLoad.clear();
    ofxOceanodeShared::getPendingLayoutSavePath().clear();
    ofxOceanodeShared::getPendingLayoutLoadPath().clear();
    presetLayouts = {};
    pendingPresetLayout.clear();
    layoutError.clear();
    presetReadFailed = false;
    presetPopup = PresetLayoutPopup::None;
    ofxOceanodeShared::getPresetLayoutActive() = false;
    const auto preset = ofxOceanodeShared::getCurrentPresetPath();
    if(preset.empty()) return;
    auto file = ofToDataPath(preset + "/preset_layouts.json");
    if(!ofFile(file).exists()){
        file = ofToDataPath(preset + "/workspaces.json"); // legacy, never written
        if(!ofFile(file).exists()) return;
    }
    try{
        presetLayouts.fromJson(ofLoadJson(file));
        pendingPresetLayout = presetLayouts.layoutOnLoad();
    }catch(const std::exception& error){
        presetReadFailed = true;
        layoutError = "Could not load preset layouts from " + file + ": " + std::string(error.what());
        ofLogError("Layouts") << layoutError;
    }
}

inline void ofxOceanodeLayoutManager::recallPresetLayout(const std::string& name){
    const auto found = presetLayouts.entries.find(name);
    if(found == presetLayouts.entries.end()) return;
    auto state = found->second;
    // This is called before NewFrame. Drop canvas-layout requests from the
    // previous frame so a macro's layout cannot overwrite the recalled one.
    pendingIniLoad.clear();
    ofxOceanodeShared::getPendingLayoutSavePath().clear();
    ofxOceanodeShared::getPendingLayoutLoadPath().clear();
    ofxOceanodeShared::getPresetLayoutActive() = true;
    ofxOceanodeShared::getActiveCanvasLayoutPath() = rootLayoutPath;
    callbacks.restoreWindows(state);
    // Remove settings absent from this snapshot, including stale floating
    // windows, before applying its docking tree and selected tabs.
    ImGui::ClearIniSettings();
    callbacks.applyDocking(state["layout"].get<std::string>());
    const bool changed = presetLayouts.active != name;
    presetLayouts.active = name;
    if(changed) savePresetLayouts();
}

// Global layout selection and startup default.

inline void ofxOceanodeLayoutManager::queueGlobalLayout(const std::string& path, const std::string& name){
    // Global layouts leave the named preset layout without replacing its snapshot.
    if(!presetLayouts.active.empty()){
        presetLayouts.active.clear();
        savePresetLayouts();
    }
    ofxOceanodeShared::getPresetLayoutActive() = false;
    ofxOceanodeShared::getPendingLayoutSavePath().clear();
    ofxOceanodeShared::getPendingLayoutLoadPath().clear();
    ofxOceanodeShared::getActiveCanvasLayoutPath() = rootLayoutPath;
    pendingPresetLayout.clear();
    pendingIniLoad = path;
    globalLayoutName = name;
}

inline void ofxOceanodeLayoutManager::loadDefaultGlobalLayout(){
    std::string configPath = ofToDataPath("config/config.json", true);
    ofFile f(configPath);
    if(!f.exists()) return;

    ofJson j = ofLoadJson(configPath);
    if(!j.contains("defaultLayout")) return;

    std::string filename = j["defaultLayout"].get<std::string>();
    if(filename.empty()) return;

    std::string fullPath = ofToDataPath("config/guiLayouts/" + filename, true);
    ofFile iniFile(fullPath);
    if(!iniFile.exists()) return;

    // Defer to draw() so ImGui context is fully active
    pendingIniLoad = fullPath;
    globalLayoutName = filename;
}

// Shared Layouts menu and dialogs.

inline void ofxOceanodeLayoutManager::drawMenu(){
    ofxOceanodeMenu::section("Preset layouts");
    drawPresetLayoutsMenu();
    ofxOceanodeMenu::section("Global layouts");
    if(ImGui::BeginMenu("Load global layout")){
        std::string layoutsDir = ofToDataPath("config/guiLayouts/", true);
        ofDirectory dir(layoutsDir);
        dir.allowExt("ini");
        dir.listDir();
        if(dir.size() == 0){
            ImGui::BeginDisabled();
            ImGui::MenuItem("(no layouts saved)");
            ImGui::EndDisabled();
        } else {
            for(int i = 0; i < (int)dir.size(); i++){
                std::string name = dir.getName(i);
                bool isCurrent = presetLayouts.active.empty() && (name == globalLayoutName);
                if(ImGui::MenuItem(name.c_str(), nullptr, isCurrent)){
                    queueGlobalLayout(dir.getPath(i), name);
                }
            }
        }
        ImGui::EndMenu();
    }

    if(ImGui::MenuItem("Save global layout as...")){
        memset(globalName, 0, sizeof(globalName));
        std::string layoutName = globalLayoutName;
        if(layoutName.size() >= 4 && layoutName.compare(layoutName.size() - 4, 4, ".ini") == 0){
            layoutName.resize(layoutName.size() - 4);
        }
        strncpy(globalName, layoutName.c_str(), sizeof(globalName) - 1);
        openSaveGlobalLayoutPopup = true;
    }

    if(ImGui::BeginMenu("Set default global layout")){
        std::string layoutsDir = ofToDataPath("config/guiLayouts/", true);
        ofDirectory dir(layoutsDir);
        dir.allowExt("ini");
        dir.listDir();

        std::string currentDefault = "";
        ofFile defaultConfig(ofToDataPath("config/config.json", true));
        if(defaultConfig.exists()){
            ofJson j = ofLoadJson(defaultConfig.path());
            if(j.contains("defaultLayout")){
                currentDefault = j["defaultLayout"].get<std::string>();
            }
        }

        if(dir.size() == 0){
            ImGui::BeginDisabled();
            ImGui::MenuItem("(no layouts saved)");
            ImGui::EndDisabled();
        } else {
            for(int i = 0; i < (int)dir.size(); i++){
                std::string name = dir.getName(i);
                bool isDefault = (name == currentDefault);
                if(ImGui::MenuItem(name.c_str(), nullptr, isDefault)){
                    std::string configPath = ofToDataPath("config/config.json", true);
                    ofJson j;
                    ofFile existingCfg(configPath);
                    if(existingCfg.exists()) j = ofLoadJson(configPath);
                    j["defaultLayout"] = name;
                    std::string cfgDir = ofToDataPath("config/", true);
                    ofDirectory cfgD(cfgDir);
                    if(!cfgD.exists()) cfgD.create(true);
                    ofSavePrettyJson(configPath, j);
                }
            }
        }
        ImGui::EndMenu();
    }

    ofxOceanodeMenu::section("Canvas switching");
    bool layoutWithMacros = ofxOceanodeShared::getGuiLayoutChangesWithMacros();
    if(ImGui::Checkbox("GUI layout changes with macros", &layoutWithMacros)){
        ofxOceanodeShared::setGuiLayoutChangesWithMacros(layoutWithMacros);
    }
}

inline void ofxOceanodeLayoutManager::drawPresetLayoutsMenu(){
    if(ImGui::MenuItem("Save preset layout as...")){
        presetName[0] = '\0';
        presetTarget.clear();
        layoutError.clear();
        presetPopup = PresetLayoutPopup::Save;
    }
    const bool active = presetLayouts.entries.count(presetLayouts.active) != 0;
    if(ImGui::MenuItem("Update current preset layout", nullptr, false, active)){
        auto previous = presetLayouts;
        presetLayouts.entries.at(presetLayouts.active) = capturePresetLayout();
        if(!savePresetLayouts()) presetLayouts = std::move(previous);
    }
    if(ImGui::MenuItem("Rename preset layout...", nullptr, false, active)){
        presetTarget = presetLayouts.active;
        std::snprintf(presetName, sizeof(presetName), "%s", presetTarget.c_str());
        layoutError.clear();
        presetPopup = PresetLayoutPopup::Rename;
    }
    if(ImGui::MenuItem("Delete preset layout...", nullptr, false, active)){
        presetTarget = presetLayouts.active;
        layoutError.clear();
        presetPopup = PresetLayoutPopup::Delete;
    }
    if(ImGui::BeginMenu("Set default preset layout")){
        auto setDefault = [this](const std::string& name){
            const auto previous = presetLayouts.defaultLayout;
            presetLayouts.defaultLayout = name;
            if(!savePresetLayouts()) presetLayouts.defaultLayout = previous;
        };
        if(ImGui::MenuItem("Last selected layout", nullptr, presetLayouts.defaultLayout.empty()))
            setDefault("");
        ImGui::Separator();
        ImGui::PushID("default-preset-layouts");
        for(const auto& entry : presetLayouts.entries){
            if(ImGui::MenuItem(entry.first.c_str(), nullptr, entry.first == presetLayouts.defaultLayout))
                setDefault(entry.first);
        }
        ImGui::PopID();
        ImGui::EndMenu();
    }
    ImGui::Separator();
    if(presetLayouts.entries.empty()) ImGui::TextDisabled("No layouts in this preset");
    ImGui::PushID("saved-presetLayouts");
    for(const auto& entry : presetLayouts.entries){
        if(ImGui::MenuItem(entry.first.c_str(), nullptr, entry.first == presetLayouts.active))
            pendingPresetLayout = entry.first;
    }
    ImGui::PopID();
    if(ofxOceanodeShared::getCurrentPresetPath().empty()){
        ImGui::Separator();
        ImGui::TextDisabled("Save the preset to keep these layouts.");
    }
    if(!layoutError.empty()) ImGui::TextWrapped("%s", layoutError.c_str());
}

inline void ofxOceanodeLayoutManager::drawPresetLayoutPopup(){
    if(presetPopup != PresetLayoutPopup::None){
        ImGui::OpenPopup(presetPopup == PresetLayoutPopup::Save ? "Save Preset Layout" :
                         presetPopup == PresetLayoutPopup::Rename ? "Rename Preset Layout" : "Delete Preset Layout");
        presetPopup = PresetLayoutPopup::None;
    }
    for(const auto action : {PresetLayoutPopup::Save, PresetLayoutPopup::Rename, PresetLayoutPopup::Delete}){
        const char* title = action == PresetLayoutPopup::Save ? "Save Preset Layout" :
                            action == PresetLayoutPopup::Rename ? "Rename Preset Layout" : "Delete Preset Layout";
        if(!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) continue;
        bool submit = false;
        if(action == PresetLayoutPopup::Delete){
            ImGui::Text("Delete preset layout \"%s\"?", presetTarget.c_str());
        }else{
            ImGui::TextUnformatted("Preset layout name:");
            if(ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(300);
            submit = ImGui::InputText("##presetName", presetName, sizeof(presetName), ImGuiInputTextFlags_EnterReturnsTrue);
        }
        if(!layoutError.empty()) ImGui::TextWrapped("%s", layoutError.c_str());
        if(ImGui::Button(action == PresetLayoutPopup::Delete ? "Delete" : "Save") || submit){
            auto previous = presetLayouts;
            try{
                const auto name = ofxOceanodePresetLayouts::cleanName(presetName);
                if(action == PresetLayoutPopup::Save) presetLayouts.create(name, capturePresetLayout());
                else if(action == PresetLayoutPopup::Rename) presetLayouts.rename(presetTarget, name);
                else presetLayouts.erase(presetTarget);
                if(savePresetLayouts()){
                    ofxOceanodeShared::getPresetLayoutActive() = !presetLayouts.active.empty();
                    if(action == PresetLayoutPopup::Save){
                        // Named preset layouts use one arrangement across all macro tabs.
                        ofxOceanodeShared::getActiveCanvasLayoutPath() = rootLayoutPath;
                        ofxOceanodeShared::getPendingLayoutSavePath().clear();
                        ofxOceanodeShared::getPendingLayoutLoadPath().clear();
                    }
                    layoutError.clear();
                    ImGui::CloseCurrentPopup();
                }else presetLayouts = std::move(previous);
            }catch(const std::exception& error){
                presetLayouts = std::move(previous);
                layoutError = error.what();
            }
        }
        ImGui::SameLine();
        if(ImGui::Button("Cancel")){
            layoutError.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

inline void ofxOceanodeLayoutManager::drawGlobalLayoutPopup(){
    if(openSaveGlobalLayoutPopup){
        ImGui::OpenPopup("Save Global Layout");
        openSaveGlobalLayoutPopup = false;
    }
    if(ImGui::BeginPopupModal("Save Global Layout", nullptr, ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::Text("Enter a name for the layout:");
        ImGui::InputText("##layoutname", globalName, sizeof(globalName));
        ImGui::Spacing();
        if(ImGui::Button("Save", ImVec2(120, 0))){
            if(strlen(globalName) > 0){
                std::string dirPath = ofToDataPath("config/guiLayouts/", true);
                ofDirectory d(dirPath);
                if(!d.exists()) d.create(true);
                std::string fullPath = dirPath + std::string(globalName) + ".ini";
                ImGui::SaveIniSettingsToDisk(fullPath.c_str());
                globalLayoutName = std::string(globalName) + ".ini";
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(ImGui::Button("Cancel", ImVec2(120, 0))){
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

}
