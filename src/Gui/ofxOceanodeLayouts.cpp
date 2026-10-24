#include "ofxOceanodeLayouts.h"
#include "ofxOceanodeWindowManager.h"
#include "ofMain.h"
#include "ofxOceanodeShared.h"
#include "ofxOceanodeMenu.h"
#include "PresetSaveCleanup.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace {
const char* globalLayoutsFolder = "config/guiLayouts/";
const char* globalLayoutsFile = "config/guiLayouts.json";

bool validView(const ofJson& view){
    return view.is_object() && view.contains("controllers") && view.at("controllers").is_object() &&
        view.contains("windows") && view.at("windows").is_object();
}
}

// Preset snapshot collection.

std::string ofxOceanodePresetLayouts::cleanName(const std::string& name){
    const auto first = name.find_first_not_of(" \t\r\n");
    if(first == std::string::npos) return {};
    return name.substr(first, name.find_last_not_of(" \t\r\n") - first + 1);
}

bool ofxOceanodePresetLayouts::validName(const std::string& name){
    for(unsigned char c : name) if(c < 32 || c == 127) return false;
    return !name.empty() && name.size() < 256 && cleanName(name) == name &&
        name.find("##") == std::string::npos;
}

void ofxOceanodePresetLayouts::create(const std::string& name, Json snapshot){
    if(!validName(name)) throw std::runtime_error("Enter a name (1-255 bytes, without ## or line breaks).");
    if(entries.count(name)) throw std::runtime_error("A preset layout with that name already exists.");
    entries.emplace(name, std::move(snapshot));
    active = name;
}

void ofxOceanodePresetLayouts::overwrite(const std::string& name, Json snapshot){
    entries.at(name) = std::move(snapshot);
    active = name;
}

void ofxOceanodePresetLayouts::rename(const std::string& from, const std::string& to){
    if(from == to) return;
    if(!validName(to)) throw std::runtime_error("Enter a name (1-255 bytes, without ## or line breaks).");
    if(entries.count(to)) throw std::runtime_error("A preset layout with that name already exists.");
    entries.emplace(to, entries.at(from));
    entries.erase(from);
    if(active == from) active = to;
    if(defaultLayout == from) defaultLayout = to;
}

void ofxOceanodePresetLayouts::erase(const std::string& name){
    entries.erase(name);
    if(active == name) active.clear();
    if(defaultLayout == name) defaultLayout.clear();
}

ofxOceanodePresetLayouts::Json ofxOceanodePresetLayouts::toJson() const {
    return {{"version", 1}, {"layouts", entries}, {"active", active}, {"default", defaultLayout}};
}

void ofxOceanodePresetLayouts::fromJson(const Json& json){
    ofxOceanodePresetLayouts loaded;
    if(!json.is_object() || !json.contains("version") || json.at("version") != 1)
        throw std::runtime_error("Unsupported layout file version.");
    if(!json.contains("layouts") || !json.at("layouts").is_object())
        throw std::runtime_error("Invalid preset layout collection.");
    for(auto it = json.at("layouts").begin(); it != json.at("layouts").end(); ++it){
        const auto& snapshot = it.value();
        if(!validName(it.key()) || !snapshot.is_object() ||
           !snapshot.contains("ini") || !snapshot.at("ini").is_string() || snapshot.at("ini").get<std::string>().empty() ||
           !snapshot.contains("view") || !validView(snapshot.at("view")))
            throw std::runtime_error("Invalid preset layout snapshot.");
        loaded.entries.emplace(it.key(), snapshot);
    }
    loaded.active = json.value("active", std::string());
    if(!loaded.entries.count(loaded.active)) loaded.active.clear();
    loaded.defaultLayout = json.value("default", std::string());
    if(!loaded.entries.count(loaded.defaultLayout)) loaded.defaultLayout.clear();
    *this = std::move(loaded);
}

// Lifecycle.

ofxOceanodeLayouts::ofxOceanodeLayouts(ofxOceanodeWindowManager& windowManager): windows(windowManager) {}

void ofxOceanodeLayouts::setup(){
    dockingSaveListener = ofxOceanodeShared::getDockingSaveRequestEvent().newListener([this](const std::string& presetPath){
        saveDocking(presetPath);
    });
    const auto path = ofToDataPath(globalLayoutsFile, true);
    if(!ofFile(path).exists()) return;
    const ofJson config = ofLoadJson(path);
    if(!config.is_object() || config.value("version", 0) != 1){
        ofLogError("Layouts") << "Ignoring " << path << ": unsupported version.";
        return;
    }
    globalDefault = config.value("default", std::string());
    // Applied by beforeFrame(), once the ImGui context is fully active.
    queueDefaultGlobalLayout();
}

// The startup arrangement: at launch and for every New preset.
void ofxOceanodeLayouts::queueDefaultGlobalLayout(){
    if(globalDefault.empty()) return;
    ofJson layout;
    if(!readGlobalLayout(globalDefault, layout, globalError)){
        ofLogError("Layouts") << globalError;
        return;
    }
    pendingIni = layout.at("ini").get<std::string>();
    pendingView = layout.at("view");
    globalLayoutName = globalDefault;
}

void ofxOceanodeLayouts::onPresetLoaded(){
    pendingIni.clear();
    pendingSnapshot.clear();
    pendingView = nullptr;
    savedView = nullptr;
    collection = {};
    layoutError.clear();
    readFailed = false;
    presetPopup = PresetLayoutPopup::None;
    const auto preset = ofxOceanodeShared::getCurrentPresetPath();
    if(preset.empty()){
        // New preset: start from the default global layout.
        queueDefaultGlobalLayout();
        return;
    }
    const auto file = ofToDataPath(preset + "/layout.json");
    if(!ofFile(file).exists()) return;
    try{
        const ofJson json = ofLoadJson(file);
        ofxOceanodePresetLayouts loaded;
        loaded.fromJson(json);
        ofJson current = json.value("current", ofJson::object());
        if(!current.is_object() ||
           (current.contains("ini") && !current.at("ini").is_string()) ||
           (current.contains("view") && !current.at("view").is_null() && !validView(current.at("view"))))
            throw std::runtime_error("Invalid current layout.");
        collection = std::move(loaded);
        const auto ini = current.value("ini", std::string());
        if(!ini.empty()) pendingIni = ini;
        pendingSnapshot = collection.layoutOnLoad();
        savedView = current.contains("view") ? current.at("view") : ofJson();
        pendingView = savedView;
    }catch(const std::exception& error){
        readFailed = true;
        layoutError = "Could not load preset layouts from " + file + ": " + std::string(error.what());
        ofLogError("Layouts") << layoutError;
    }
}

void ofxOceanodeLayouts::onPresetSaved(){
    savedView = windows.captureView();
    writeLayoutFile();
}

void ofxOceanodeLayouts::saveDocking(const std::string& presetPath){
    if(presetPath != ofxOceanodeShared::getCurrentPresetPath()) return;
    writeLayoutFile();
}

void ofxOceanodeLayouts::beforeFrame(){
    if(!pendingIni.empty()){
        const auto ini = std::move(pendingIni);
        pendingIni.clear();
        windows.applyIni(ini);
    }
    if(!pendingSnapshot.empty()){
        const auto name = pendingSnapshot;
        pendingSnapshot.clear();
        recallPresetLayout(name);
    }
    if(!pendingView.is_null()){
        // The view at preset save wins over an older named snapshot.
        // Explicitly recalling that snapshot later still restores its toggles.
        windows.restoreView(pendingView);
        pendingView = nullptr;
    }
}

// Persistence. Every write stores the whole file.

bool ofxOceanodeLayouts::writeLayoutFile(){
    if(readFailed){
        layoutError = "Cannot save preset layouts until the invalid layout file is fixed and the preset reloaded.";
        return false;
    }
    const auto preset = ofxOceanodeShared::getCurrentPresetPath();
    // An unsaved patch keeps its layouts in memory; its first Save writes them all.
    if(preset.empty()) return true;
    ofJson file = collection.toJson();
    file["current"] = {{"ini", windows.captureIni()}, {"view", savedView}};
    if(oceanodePresetSave::saveJson(preset + "/layout.json", file)){
        layoutError.clear();
        return true;
    }
    layoutError = "Could not save layout.json. Check the preset folder is writable.";
    return false;
}

ofJson ofxOceanodeLayouts::capturePresetLayout(){
    return {{"ini", windows.captureIni()}, {"view", windows.captureView()}};
}

void ofxOceanodeLayouts::recallPresetLayout(const std::string& name){
    const auto found = collection.entries.find(name);
    if(found == collection.entries.end()) return;
    const auto snapshot = found->second;
    // Apply before NewFrame and replace any older pending layout load.
    pendingIni.clear();
    windows.restoreView(snapshot.at("view"));
    // Remove settings absent from this snapshot, including stale floating
    // windows, before applying its docking tree and selected tabs.
    windows.clearIni();
    windows.applyIni(snapshot.at("ini").get<std::string>());
    const bool changed = collection.active != name;
    collection.active = name;
    if(changed) writeLayoutFile();
}

// Global layouts.

bool ofxOceanodeLayouts::validGlobalName(const std::string& name){
    return ofxOceanodePresetLayouts::validName(name) && name.find_first_of("/\\:") == std::string::npos;
}

// A global layout is {"version": 1, "ini": <non-empty ImGui ini text>, "view": View | null}.
bool ofxOceanodeLayouts::readGlobalLayout(const std::string& name, ofJson& layout, std::string& error){
    const auto path = ofToDataPath(globalLayoutsFolder + name + ".json", true);
    if(!ofFile(path).exists()){
        error = "Global layout not found: " + path;
        return false;
    }
    layout = ofLoadJson(path);
    if(!layout.is_object() || layout.value("version", 0) != 1 ||
       !layout.contains("ini") || !layout.at("ini").is_string() || layout.at("ini").get<std::string>().empty() ||
       (layout.contains("view") && !layout.at("view").is_null() && !validView(layout.at("view")))){
        error = "Invalid global layout: " + path;
        return false;
    }
    if(!layout.contains("view")) layout["view"] = nullptr;
    error.clear();
    return true;
}

void ofxOceanodeLayouts::queueGlobalLayout(const std::string& name){
    ofJson layout;
    if(!readGlobalLayout(name, layout, globalError)){
        ofLogError("Layouts") << globalError;
        return;
    }
    // Global layouts leave the named preset layout without replacing its snapshot.
    if(!collection.active.empty()){
        collection.active.clear();
        writeLayoutFile();
    }
    pendingSnapshot.clear();
    pendingIni = layout.at("ini").get<std::string>();
    pendingView = layout.at("view");
    globalLayoutName = name;
}

void ofxOceanodeLayouts::setGlobalDefault(const std::string& name){
    ofDirectory::createDirectory(ofToDataPath("config", true), true, true);
    if(ofSavePrettyJson(ofToDataPath(globalLayoutsFile, true), {{"version", 1}, {"default", name}}))
        globalDefault = name;
    else ofLogError("Layouts") << "Could not save " << globalLayoutsFile;
}

// Layouts menu and dialogs.

void ofxOceanodeLayouts::drawMenu(){
    ofxOceanodeMenu::section("Preset layouts");
    drawPresetLayoutsMenu();
    ofxOceanodeMenu::section("Global layouts");
    ofDirectory dir(ofToDataPath(globalLayoutsFolder, true));
    dir.allowExt("json");
    if(dir.exists()) dir.listDir();
    if(ImGui::BeginMenu("Load global layout")){
        if(dir.size() == 0){
            ImGui::BeginDisabled();
            ImGui::MenuItem("(no layouts saved)");
            ImGui::EndDisabled();
        } else {
            for(int i = 0; i < (int)dir.size(); i++){
                const std::string name = dir.getFile(i).getBaseName();
                bool isCurrent = collection.active.empty() && (name == globalLayoutName);
                if(ImGui::MenuItem(name.c_str(), nullptr, isCurrent)){
                    queueGlobalLayout(name);
                }
            }
        }
        ImGui::EndMenu();
    }
    if(!globalError.empty() && !openSaveGlobalLayoutPopup) ImGui::TextWrapped("%s", globalError.c_str());

    if(ImGui::MenuItem("Save global layout as...")){
        std::snprintf(globalName, sizeof(globalName), "%s", globalLayoutName.c_str());
        globalError.clear();
        openSaveGlobalLayoutPopup = true;
    }

    if(ImGui::BeginMenu("Set default global layout")){
        if(dir.size() == 0){
            ImGui::BeginDisabled();
            ImGui::MenuItem("(no layouts saved)");
            ImGui::EndDisabled();
        } else {
            for(int i = 0; i < (int)dir.size(); i++){
                const std::string name = dir.getFile(i).getBaseName();
                if(ImGui::MenuItem(name.c_str(), nullptr, name == globalDefault)){
                    setGlobalDefault(name);
                }
            }
        }
        ImGui::EndMenu();
    }
}

void ofxOceanodeLayouts::drawPresetLayoutsMenu(){
    if(ImGui::MenuItem("Save preset layout as...")){
        presetName[0] = '\0';
        presetTarget.clear();
        layoutError.clear();
        presetPopup = PresetLayoutPopup::Save;
    }
    const bool active = collection.entries.count(collection.active) != 0;
    if(ImGui::MenuItem("Update current preset layout", nullptr, false, active)){
        auto previous = collection;
        collection.entries.at(collection.active) = capturePresetLayout();
        if(!writeLayoutFile()) collection = std::move(previous);
    }
    if(ImGui::BeginMenu("Overwrite preset layout", !collection.entries.empty())){
        ImGui::PushID("overwrite-preset-layouts");
        for(const auto& entry : collection.entries){
            if(ImGui::MenuItem(entry.first.c_str(), nullptr, entry.first == collection.active)){
                presetTarget = entry.first;
                layoutError.clear();
                presetPopup = PresetLayoutPopup::Overwrite;
            }
        }
        ImGui::PopID();
        ImGui::EndMenu();
    }
    if(ImGui::MenuItem("Rename preset layout...", nullptr, false, active)){
        presetTarget = collection.active;
        std::snprintf(presetName, sizeof(presetName), "%s", presetTarget.c_str());
        layoutError.clear();
        presetPopup = PresetLayoutPopup::Rename;
    }
    if(ImGui::MenuItem("Delete preset layout...", nullptr, false, active)){
        presetTarget = collection.active;
        layoutError.clear();
        presetPopup = PresetLayoutPopup::Delete;
    }
    if(ImGui::BeginMenu("Set default preset layout")){
        auto setDefault = [this](const std::string& name){
            const auto previous = collection.defaultLayout;
            collection.defaultLayout = name;
            if(!writeLayoutFile()) collection.defaultLayout = previous;
        };
        if(ImGui::MenuItem("Last selected layout", nullptr, collection.defaultLayout.empty()))
            setDefault("");
        ImGui::Separator();
        ImGui::PushID("default-preset-layouts");
        for(const auto& entry : collection.entries){
            if(ImGui::MenuItem(entry.first.c_str(), nullptr, entry.first == collection.defaultLayout))
                setDefault(entry.first);
        }
        ImGui::PopID();
        ImGui::EndMenu();
    }
    ImGui::Separator();
    if(collection.entries.empty()) ImGui::TextDisabled("No layouts in this preset");
    ImGui::PushID("saved-presetLayouts");
    for(const auto& entry : collection.entries){
        if(ImGui::MenuItem(entry.first.c_str(), nullptr, entry.first == collection.active))
            pendingSnapshot = entry.first;
    }
    ImGui::PopID();
    if(ofxOceanodeShared::getCurrentPresetPath().empty()){
        ImGui::Separator();
        ImGui::TextDisabled("Save the preset to keep these layouts.");
    }
    if(!layoutError.empty()) ImGui::TextWrapped("%s", layoutError.c_str());
}

void ofxOceanodeLayouts::drawPopups(){
    drawPresetLayoutPopup();
    drawGlobalLayoutPopup();
}

const char* ofxOceanodeLayouts::presetLayoutPopupTitle(PresetLayoutPopup popup){
    switch(popup){
        case PresetLayoutPopup::Save: return "Save Preset Layout";
        case PresetLayoutPopup::Overwrite: return "Overwrite Preset Layout";
        case PresetLayoutPopup::Rename: return "Rename Preset Layout";
        default: return "Delete Preset Layout";
    }
}

void ofxOceanodeLayouts::drawPresetLayoutPopup(){
    if(presetPopup != PresetLayoutPopup::None){
        ImGui::OpenPopup(presetLayoutPopupTitle(presetPopup));
        presetPopup = PresetLayoutPopup::None;
    }
    for(const auto action : {PresetLayoutPopup::Save, PresetLayoutPopup::Overwrite, PresetLayoutPopup::Rename, PresetLayoutPopup::Delete}){
        const char* title = presetLayoutPopupTitle(action);
        const bool confirmOnly = action == PresetLayoutPopup::Overwrite || action == PresetLayoutPopup::Delete;
        if(!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) continue;
        bool submit = false;
        if(action == PresetLayoutPopup::Overwrite){
            ImGui::Text("Overwrite preset layout \"%s\" with the current arrangement?", presetTarget.c_str());
        }else if(action == PresetLayoutPopup::Delete){
            ImGui::Text("Delete preset layout \"%s\"?", presetTarget.c_str());
        }else{
            ImGui::TextUnformatted("Preset layout name:");
            if(ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(300);
            submit = ImGui::InputText("##presetName", presetName, sizeof(presetName), ImGuiInputTextFlags_EnterReturnsTrue);
        }
        if(!layoutError.empty()) ImGui::TextWrapped("%s", layoutError.c_str());
        const char* confirm = action == PresetLayoutPopup::Overwrite ? "Overwrite" :
                              action == PresetLayoutPopup::Delete ? "Delete" : "Save";
        if(ImGui::Button(confirm) || submit){
            auto previous = collection;
            try{
                const auto name = confirmOnly ? presetTarget : ofxOceanodePresetLayouts::cleanName(presetName);
                if(action == PresetLayoutPopup::Save) collection.create(name, capturePresetLayout());
                else if(action == PresetLayoutPopup::Overwrite) collection.overwrite(name, capturePresetLayout());
                else if(action == PresetLayoutPopup::Rename) collection.rename(presetTarget, name);
                else collection.erase(presetTarget);
                if(writeLayoutFile()){
                    layoutError.clear();
                    ImGui::CloseCurrentPopup();
                }else collection = std::move(previous);
            }catch(const std::exception& error){
                collection = std::move(previous);
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

void ofxOceanodeLayouts::drawGlobalLayoutPopup(){
    if(openSaveGlobalLayoutPopup){
        ImGui::OpenPopup("Save Global Layout");
        openSaveGlobalLayoutPopup = false;
    }
    if(ImGui::BeginPopupModal("Save Global Layout", nullptr, ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::Text("Enter a name for the layout:");
        ImGui::InputText("##layoutname", globalName, sizeof(globalName));
        if(!globalError.empty()) ImGui::TextWrapped("%s", globalError.c_str());
        ImGui::Spacing();
        if(ImGui::Button("Save", ImVec2(120, 0))){
            const auto name = ofxOceanodePresetLayouts::cleanName(globalName);
            if(!validGlobalName(name)){
                globalError = "Enter a name (1-255 bytes, without ##, line breaks, /, \\ or :).";
            }else{
                ofDirectory::createDirectory(ofToDataPath(globalLayoutsFolder, true), true, true);
                const ofJson layout = {{"version", 1}, {"ini", windows.captureIni()}, {"view", windows.captureView()}};
                if(ofSavePrettyJson(ofToDataPath(globalLayoutsFolder + name + ".json", true), layout)){
                    globalLayoutName = name;
                    globalError.clear();
                    ImGui::CloseCurrentPopup();
                }else globalError = "Could not save the layout. Check config/guiLayouts is writable.";
            }
        }
        ImGui::SameLine();
        if(ImGui::Button("Cancel", ImVec2(120, 0))){
            globalError.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
